# Wdrożenie: Historia dolewek w FRAM zamiast VPS

## 1. Obecnie logowane parametry do VPS

### Przy każdym cyklu automatycznym (`logCycleToVPS`)

| Parametr | Typ | Opis |
|----------|-----|------|
| `device_id` | string | ID urządzenia z FRAM |
| `unix_time` | uint32 | Timestamp cyklu |
| `event_type` | string | "AUTO_CYCLE_COMPLETE" |
| `volume_ml` | uint16 | Objętość dolewki (ml) |
| `water_status` | string | Status czujników |
| `system_status` | string | "OK" lub "ERROR" |
| `time_gap_1` | uint32 | Czas opadania poziomu (s) |
| `time_gap_2` | uint32 | Czas podnoszenia poziomu (s) |
| `water_trigger_time` | uint32 | Czas reakcji czujników na pompę (s) |
| `pump_duration` | uint16 | Rzeczywisty czas pracy pompy (s) |
| `pump_attempts` | uint8 | Liczba prób pompy |
| `gap1_fail_sum` | uint16 | Suma błędów GAP1 (kumulatywna) |
| `gap2_fail_sum` | uint16 | Suma błędów GAP2 (kumulatywna) |
| `water_fail_sum` | uint16 | Suma błędów WATER (kumulatywna) |
| `daily_volume_ml` | uint16 | Dzienna objętość (ml) |
| `algorithm_data` | string | Podsumowanie parametrów algorytmu |
| `time_uncertain` | bool | Flaga niepewności czasu (opcjonalna) |

### Przy dolewce manualnej (`logEventToVPS`)

| Parametr | Typ | Opis |
|----------|-----|------|
| `device_id` | string | ID urządzenia |
| `unix_time` | uint32 | Timestamp |
| `event_type` | string | "MANUAL_NORMAL" |
| `volume_ml` | uint16 | Objętość |
| `water_status` | string | Status czujników |
| `daily_volume_ml` | uint16 | Dzienna objętość |

---

## 2. Istniejąca infrastruktura FRAM

### Struktura `PumpCycle` (algorithm_config.h:69-85)

```cpp
struct PumpCycle {
    uint32_t timestamp;           // 4B - Unix timestamp
    uint32_t trigger_time;        // 4B - Czas aktywacji TRIGGER
    uint32_t time_gap_1;          // 4B - Czas GAP1
    uint32_t time_gap_2;          // 4B - Czas GAP2
    uint32_t water_trigger_time;  // 4B - Czas reakcji na pompę
    uint16_t pump_duration;       // 2B - Czas pracy pompy
    uint8_t  pump_attempts;       // 1B - Liczba prób
    uint8_t  sensor_results;      // 1B - Flagi wyników (bity)
    uint8_t  error_code;          // 1B - Kod błędu
    uint16_t volume_dose;         // 2B - Objętość w ml
};                                // = 27B (padded to 28B)
```

### Ring Buffer w FRAM (fram_controller.h)

```
FRAM_ADDR_CYCLE_COUNT  = 0x0530  // 2B - liczba zapisanych cykli
FRAM_ADDR_CYCLE_INDEX  = 0x0532  // 2B - current write index
FRAM_ADDR_CYCLE_DATA   = 0x0600  // Start danych cykli

FRAM_MAX_CYCLES = 200            // Max cykli (~20 dni)
FRAM_CYCLE_SIZE = 24             // Rozmiar rekordu (uwaga: struct ma 27B!)
```

**UWAGA**: Niezgodność rozmiaru! `sizeof(PumpCycle)` = ~27-28B, ale `FRAM_CYCLE_SIZE` = 24B. Wymaga weryfikacji/poprawki.

### Istniejące funkcje FRAM

- `saveCycleToFRAM(const PumpCycle& cycle)` - zapis do ring buffer
- `loadCyclesFromFRAM(vector<PumpCycle>& cycles, uint16_t maxCount)` - odczyt
- `getCycleCountFromFRAM()` - liczba zapisanych cykli
- `clearOldCyclesFromFRAM(uint32_t olderThanDays)` - usuwanie starych

---

## 3. Analiza problemu: AsyncWebServer vs Loop

### Problem współbieżności

```
┌─────────────────┐     ┌─────────────────┐
│   Main Loop     │     │ AsyncWebServer  │
│                 │     │   (callbacks)   │
│ waterAlgorithm  │     │                 │
│   .update()     │     │ /api/history    │
│       │         │     │      │          │
│       ▼         │     │      ▼          │
│ saveCycleToFRAM │ ◄── │ loadCyclesFrom  │
│       │         │ ──► │      FRAM       │
│       ▼         │     │                 │
│   I2C Write     │ ⚠️  │   I2C Read      │
└─────────────────┘     └─────────────────┘
         │                      │
         └──────── KONFLIKT ────┘
```

**Scenariusze błędów:**
1. Loop zapisuje cykl → WebServer próbuje czytać → uszkodzone dane
2. WebServer czyta podczas inkrementacji indeksu → niepoprawny offset
3. Przerwanie transakcji I2C → timeout/błąd komunikacji

### Dlaczego to problem w tym projekcie

- ESP32-C3 to single-core (w przeciwieństwie do ESP32 dual-core)
- AsyncWebServer używa task/callback w kontekście WiFi
- Brak natywnej ochrony I2C przed równoczesnym dostępem
- FRAM wymaga sekwencji read-modify-write dla ring buffer

---

## 4. Ocena propozycji użytkownika

### Propozycja: Przycisk w GUI + pobranie na żądanie

**Zalety:**
- Eliminuje problem ciągłego odpytywania
- Determinystyczny moment odczytu (inicjowany przez użytkownika)
- Możliwość implementacji prostego locka

**Wady:**
- Wymaga zmiany UX (użytkownik musi kliknąć)
- Nie rozwiązuje problemu jeśli klik trafi w moment zapisu

### Propozycja: Kontrola konfliktów zamiast auto-odświeżania

**Zalety:**
- Redukuje częstotliwość potencjalnych konfliktów
- Prostsze debugowanie

**Wady:**
- Nadal możliwy konflikt przy manualnym żądaniu
- Nie eliminuje problemu całkowicie

---

## 5. Rekomendowane rozwiązanie

### A. Mutex/Semaphore dla dostępu do FRAM

```cpp
// fram_controller.h
extern SemaphoreHandle_t framMutex;

// fram_controller.cpp
SemaphoreHandle_t framMutex = nullptr;

bool initFRAM() {
    framMutex = xSemaphoreCreateMutex();
    // ... reszta inicjalizacji
}

bool saveCycleToFRAM_Safe(const PumpCycle& cycle) {
    if (xSemaphoreTake(framMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool result = saveCycleToFRAM(cycle);
        xSemaphoreGive(framMutex);
        return result;
    }
    return false; // Timeout - FRAM zajęty
}
```

### B. Flaga "zajętości" + retry w WebServer

```cpp
// Prostsza alternatywa bez FreeRTOS mutex
volatile bool framBusy = false;

// W loop (water_algorithm.cpp)
framBusy = true;
saveCycleToFRAM(cycle);
framBusy = false;

// W web_handlers.cpp
void handleGetHistory(AsyncWebServerRequest *request) {
    if (framBusy) {
        request->send(503, "application/json",
            "{\"error\":\"FRAM busy, retry in 1s\"}");
        return;
    }
    // ... odczyt historii
}
```

### C. Przycisk "Pobierz historię" w GUI (zgodnie z propozycją)

```javascript
// W html_pages.cpp - sekcja JavaScript
async function loadHistory() {
    const btn = document.getElementById('loadHistoryBtn');
    btn.disabled = true;
    btn.textContent = 'Pobieranie...';

    try {
        const response = await fetch('/api/history');
        if (response.status === 503) {
            alert('System zajęty, spróbuj za chwilę');
            return;
        }
        const data = await response.json();
        renderHistoryTable(data.cycles);
    } catch (e) {
        alert('Błąd połączenia');
    } finally {
        btn.disabled = false;
        btn.textContent = 'Pobierz historię';
    }
}
```

---

## 6. Rozszerzona struktura dla pełnej historii

Obecna `PumpCycle` nie zawiera wszystkich danych VPS. Propozycja rozszerzenia:

```cpp
struct PumpCycleExtended {
    // Istniejące pola
    uint32_t timestamp;
    uint32_t time_gap_1;
    uint32_t time_gap_2;
    uint32_t water_trigger_time;
    uint16_t pump_duration;
    uint8_t  pump_attempts;
    uint8_t  sensor_results;
    uint8_t  error_code;
    uint16_t volume_dose;

    // Nowe pola (dla pełnej zgodności z VPS)
    uint16_t daily_volume_at_cycle;  // Dzienna objętość w momencie cyklu
    uint8_t  event_type;             // 0=AUTO, 1=MANUAL_NORMAL, 2=MANUAL_EXTENDED
    uint8_t  reserved[3];            // Padding do 32B
};  // = 32B
```

**Nowy layout FRAM:**
```
FRAM_CYCLE_SIZE_EXT = 32  // Nowy rozmiar
FRAM_MAX_CYCLES_EXT = 150 // Mniej cykli ale pełniejsze dane
```

---

## 7. Plan wdrożenia

### Faza 1: Zabezpieczenie dostępu (KRYTYCZNE)
- [ ] Dodać mutex/flagę dla operacji FRAM
- [ ] Wrapper funkcji `*_Safe()` dla krytycznych operacji
- [ ] Obsługa HTTP 503 w web_handlers

### Faza 2: API endpoint historii
- [ ] Nowy endpoint `GET /api/history?limit=50`
- [ ] Formatowanie JSON zgodne z obecnym VPS
- [ ] Przycisk w GUI z obsługą retry

### Faza 3: Rozszerzenie struktury (opcjonalne)
- [ ] Migracja `PumpCycle` → `PumpCycleExtended`
- [ ] Wersjonowanie layoutu FRAM (bump version)
- [ ] Backward compatibility dla starych danych

### Faza 4: Wyłączenie VPS (opcjonalne)
- [ ] Flaga konfiguracyjna `VPS_LOGGING_ENABLED`
- [ ] Zachowanie logowania dla krytycznych błędów

---

## 8. Endpoint API - specyfikacja

### `GET /api/history`

**Query params:**
- `limit` (opcjonalny, default=50, max=200)
- `offset` (opcjonalny, default=0)

**Response (200 OK):**
```json
{
    "success": true,
    "total_cycles": 156,
    "returned": 50,
    "cycles": [
        {
            "timestamp": 1705329025,
            "time_formatted": "2024-01-15 14:30:25",
            "volume_ml": 200,
            "time_gap_1": 1850,
            "time_gap_2": 12,
            "water_trigger_time": 45,
            "pump_duration": 20,
            "pump_attempts": 1,
            "error_code": 0,
            "gap1_fail": false,
            "gap2_fail": false,
            "water_fail": false
        }
        // ... więcej cykli
    ]
}
```

**Response (503 Service Unavailable):**
```json
{
    "success": false,
    "error": "FRAM_BUSY",
    "retry_after_ms": 1000
}
```

---

## 9. Uwagi dotyczące dostępu przez WireGuard

Dostęp przez internet (router → VPS → WireGuard) nie zmienia architektury rozwiązania, ale dodaje:

1. **Latencja** - żądania HTTP będą wolniejsze, ale to nie wpływa na problem współbieżności
2. **Timeout** - warto zwiększyć timeout dla `/api/history` (dużo danych)
3. **Kompresja** - rozważyć gzip dla odpowiedzi JSON (ESPAsyncWebServer wspiera)

```cpp
// Zwiększony timeout dla długich odpowiedzi
request->send(response);
// AsyncWebServer domyślnie nie ma limitu, ale klient może mieć
```

---

## 10. Podsumowanie ryzyk

| Ryzyko | Prawdopodobieństwo | Wpływ | Mitygacja |
|--------|-------------------|-------|-----------|
| Konflikt I2C | Średnie | Wysoki | Mutex + retry |
| Przepełnienie FRAM | Niskie | Średni | Ring buffer (już jest) |
| Timeout WireGuard | Niskie | Niski | Retry w GUI |
| Uszkodzenie danych | Niskie | Wysoki | Checksum (już jest) |

---

**Autor:** Claude Code
**Data:** 2026-01-23
**Wersja:** 1.0
