
# Analiza: Tabela historii cykli dolewania w GUI

## Planowana funkcjonalność

Przewijana tabela pod sekcją "System Status" w dashboardzie WWW, prezentująca rekordy z każdego procesu dolewania.

### Kolumny tabeli

| # | Kolumna | Pole w PumpCycle | Sposób odczytu |
|---|---------|------------------|----------------|
| 1 | Data i czas | `timestamp` (uint32_t Unix) | JS: `new Date(ts * 1000).toLocaleString('pl-PL')` |
| 2 | Aktywacja S1 przy opadaniu | `sensor_results` bit flags | `!(RESULT_SENSOR1_DEBOUNCE_FAIL & 0x40)` = S1 OK |
| 3 | Aktywacja S2 przy opadaniu | `sensor_results` bit flags | `!(RESULT_SENSOR2_DEBOUNCE_FAIL & 0x80)` = S2 OK |
| 4 | Debounce potwierdził pre-qual | `sensor_results` flag | `!(RESULT_FALSE_TRIGGER & 0x20)` = potwierdzone |
| 5 | Liczba prób pompy | `pump_attempts` (uint8_t) | Wprost, zakres 1-3 |
| 6 | Deaktywacja S1 po pompowaniu | `sensor_results` bit flags | `!(RESULT_SENSOR1_RELEASE_FAIL & 0x08)` && `!(RESULT_WATER_FAIL & 0x04)` |
| 7 | Deaktywacja S2 po pompowaniu | `sensor_results` bit flags | `!(RESULT_SENSOR2_RELEASE_FAIL & 0x10)` && `!(RESULT_WATER_FAIL & 0x04)` |
| 8 | Alarm | `error_code` (uint8_t) | 0=brak, 1=limit dobowy, 2=awaria pompy, 3=oba |

### Rekomendowane dodatkowe kolumny

| Kolumna | Pole | Uzasadnienie |
|---------|------|--------------|
| Objętość (ml) | `volume_dose` | Ile wody dolano w cyklu |
| Czas pompy (s) | `pump_duration` | Diagnostyka wydajności pompy |
| Gap S1-S2 opadanie (s) | `time_gap_1` | Diagnostyka synchronizacji czujników |

---

## Krytyczny bug: FRAM_CYCLE_SIZE

### Problem

`FRAM_CYCLE_SIZE` = 24 bajty, ale `sizeof(PumpCycle)` = 28 bajtów.

Pola `error_code` (uint8_t) i `volume_dose` (uint16_t) zostały dodane do struktury PumpCycle
bez aktualizacji stałej FRAM_CYCLE_SIZE. Kompilator dodaje 1 bajt paddingu między
`error_code` a `volume_dose` (wyrównanie uint16_t do 2 bajtów), co daje 28 bajtów.

### Mechanizm korupcji

```
fram_controller.cpp:258 - adres obliczany ze skokiem 24B:
  writeAddr = FRAM_ADDR_CYCLE_DATA + (writeIndex * FRAM_CYCLE_SIZE)  // ×24

fram_controller.cpp:261 - zapis pełnego rozmiaru struktury:
  fram.write(writeAddr, (uint8_t*)&cycle, sizeof(PumpCycle))         // pisze 28B
```

Każdy zapis cyklu nadpisuje 4 bajty NASTĘPNEGO rekordu w buforze.
Przy odczycie: `error_code` i `volume_dose` są czytane z pamięci kolejnego cyklu.

### Naprawa

1. Zaktualizować `FRAM_CYCLE_SIZE` z 24 na 28
2. Zaktualizować `FRAM_DATA_VERSION` (dodać logikę migracji w `verifyFRAM()`)
3. Przeliczyć zajętość FRAM: 200 cykli × 28B = 5600B (było 4800B, mieści się w 32KB)
4. Alternatywa: `__attribute__((packed))` na PumpCycle i FRAM_CYCLE_SIZE = 27
   (oszczędność pamięci FRAM, ale wolniejszy dostęp na RISC-V)

---

## Architektura implementacji

### Nowy endpoint API

```
GET /api/get-cycles?page=0&count=20
```

- Autentykacja: wymagana sesja (jak pozostałe endpointy)
- Paginacja: domyślnie 20 rekordów, parametr `page` i `count`
- Kolejność: od najnowszych (odwrócony ring buffer)
- Response format: JSON array obiektów PumpCycle

### Strategia ładowania danych

1. **Przy otwarciu strony**: auto-load ostatnich ~10 rekordów
2. **Button "Załaduj więcej"**: doładowanie kolejnej strony (20 rekordów)
3. **Button "Załaduj wszystkie"**: opcjonalne, z ostrzeżeniem o czasie ładowania

### Schemat odpowiedzi JSON

```json
{
  "cycles": [
    {
      "ts": 1706000000,
      "s1_drop": true,
      "s2_drop": true,
      "debounce_ok": true,
      "attempts": 1,
      "s1_rise": true,
      "s2_rise": false,
      "alarm": 0,
      "volume_ml": 200,
      "pump_s": 12,
      "gap1_s": 5
    }
  ],
  "total": 45,
  "page": 0,
  "has_more": true
}
```

Dekodowanie flag `sensor_results` po stronie serwera (ESP32) zamiast wysyłania
surowego bajtu - upraszcza logikę JS i zmniejsza rozmiar odpowiedzi.

---

## Kwestie do rozwiązania przy implementacji

### 1. Pamięć RAM ESP32-C3 (~400KB SRAM)

- `std::vector<PumpCycle>` dla 200 cykli = ~5.6KB RAM
- JSON dla 200 cykli = ~30-40KB
- ESPAsyncWebServer buduje odpowiedź w pamięci
- **Rozwiązanie**: paginacja (20 rekordów/request) lub chunked streaming response

### 2. Synchronizacja dostępu do FRAM (I2C)

- Odczyt 200 cykli z FRAM (I2C 400kHz) trwa ~50-100ms
- W tym czasie algorytm nie może pisać do FRAM ani czytać RTC
- Wyścig: handler HTTP czyta FRAM podczas zapisu cyklu przez algorytm
- **Rozwiązanie**: mutex/flaga `framBusy` na dostęp do magistrali I2C

### 3. Interakcja z auto-cleanup

- `clearOldCyclesFromFRAM()` przebudowuje cały ring buffer (read all → filter → rewrite)
- Jeśli handler HTTP czyta jednocześnie - niespójne dane
- **Rozwiązanie**: ten sam mutex co punkt 2, lub atomowy flag operacji

### 4. Kolejność ring buffera

- `loadCyclesFromFRAM()` zwraca chronologicznie (od najstarszego)
- Tabela wymaga odwrotnej kolejności (najnowsze na górze)
- **Rozwiązanie**: odwrócenie w handlerze API lub modyfikacja loadCycles

### 5. Formatowanie dat

- Timestamp to Unix seconds UTC
- Konwersję na datę lokalną (CET/CEST) wykonywać w JavaScript po stronie klienta
- Nie obciążać ESP32 formatowaniem stringów

### 6. FRAM wersjonowanie i migracja

- Po naprawie FRAM_CYCLE_SIZE: nowa wersja danych
- Logika migracji w `verifyFRAM()` analogicznie do istniejącego upgrade v1→v2
- Istniejące dane w starym formacie (24B/cykl) wymagają reinterpretacji lub wyczyszczenia

---

## Istniejąca infrastruktura do wykorzystania

| Element | Lokalizacja | Status |
|---------|-------------|--------|
| Struktura PumpCycle | `algorithm_config.h:85-108` | Gotowa (wymaga packed lub fix CYCLE_SIZE) |
| Ring buffer FRAM | `fram_controller.cpp:244-330` | Gotowy (wymaga fix CYCLE_SIZE) |
| loadCyclesFromFRAM() | `fram_controller.cpp:279` | Gotowa, wymaga paginacji |
| Web handlers | `web/web_handlers.cpp` | Szablon do nowego endpointu |
| HTML dashboard | `web/html_pages.h` | Miejsce na nową sekcję tabeli |
| Sesja auth | `security/session_manager.*` | Do ochrony nowego endpointu |

---

## Szacunkowy układ pamięci FRAM po naprawie

```
0x0000-0x0017  (24B)    : Magic + version
0x0018-0x0417  (1024B)  : Encrypted credentials
0x0500-0x053F  (64B)    : Settings + stats
0x0600-0x1DB7  (5600B)  : Cycle ring buffer (200 × 28B) -- było 4800B
                          Koniec: 0x0600 + 5600 = 0x1DC0
Wolne:          0x1DC0-0x7FFF = ~25KB
```
