# Wdrożenie Debouncing V2 - Pre-qualification + Debouncing

## 1. Problem z obecną implementacją

### 1.1 Opis problemu
Obecny algorytm debouncingu ma krytyczny błąd: **pojedyncze drganie (spike) na czujniku natychmiast startuje cały proces**, który trwa do 2300 sekund (TIME_GAP_1_MAX).

### 1.2 Obserwowane symptomy
- Duży rozrzut `time_gap_1`: od 100s do ponad 2300s
- 20-30% zdarzeń przekracza TIME_GAP_1_MAX mimo prawidłowo działających czujników
- Fałszywe błędy `RESULT_GAP1_FAIL`

### 1.3 Przyczyna
Błąd w oryginalnej logice debounce:
```cpp
// STARA BŁĘDNA LOGIKA
if (currentSensor != lastSensor) {
    if (currentTime - lastDebounce > DEBOUNCE_TIME) {  // 1 sekunda
        lastDebounce = currentTime;  // Aktualizacja TYLKO przy akceptacji!
        onSensorStateChange();       // TRIGGER natychmiast!
    }
}
```

**Problem:** `lastDebounce` aktualizowano tylko przy zaakceptowanej zmianie, nie przy każdym drganiu. Po upływie 1 sekundy od ostatniego triggera, pierwszy sygnał LOW był natychmiast akceptowany - nawet jeśli to tylko drganie czujnika pływakowego.

---

## 2. Nowa architektura - Pre-qualification + Debouncing

### 2.1 Koncepcja
Dwuetapowa weryfikacja sygnału:

1. **Pre-qualification** - szybki test "czy to prawdziwe opadanie wody?"
2. **Settling** - czas na uspokojenie się wody
3. **Debouncing** - pełna weryfikacja stabilności sygnału

### 2.2 Diagram stanów

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  STATE_IDLE                                                             │
│      │                                                                  │
│      ▼ (pierwszy LOW na dowolnym czujniku)                              │
│                                                                         │
│  STATE_PRE_QUALIFICATION                                                │
│      │                                                                  │
│      │  PRE_QUAL_WINDOW = 30s                                           │
│      │  PRE_QUAL_INTERVAL = 10s                                         │
│      │  Wymóg: 3 KOLEJNE LOW na którymkolwiek czujniku                  │
│      │                                                                  │
│      ├─► Timeout (30s) bez 3×LOW → IDLE (cicho, bez błędu)              │
│      │                                                                  │
│      ▼ (3×LOW osiągnięte)                                               │
│                                                                         │
│  STATE_SETTLING                                                         │
│      │                                                                  │
│      │  SETTLING_TIME = 60s                                             │
│      │  Akcja: BRAK (pasywne czekanie)                                  │
│      │                                                                  │
│      ▼ (po 60s)                                                         │
│                                                                         │
│  STATE_DEBOUNCING                                                       │
│      │                                                                  │
│      │  TOTAL_DEBOUNCE_TIME = 1200s                                     │
│      │  DEBOUNCE_INTERVAL = 60s (20 pomiarów max)                       │
│      │  DEBOUNCE_COUNTER = 4 (dla każdego czujnika)                     │
│      │                                                                  │
│      │  Logika pomiarów (co 60s, dla każdego czujnika):                 │
│      │  ┌─────────────────────────────────────────┐                     │
│      │  │ LOW  → counter++                        │                     │
│      │  │ HIGH → counter = 0                      │                     │
│      │  │ counter >= 4 → AKTYWNY                  │                     │
│      │  │        zapisz czas zaliczenia           │                     │
│      │  └─────────────────────────────────────────┘                     │
│      │                                                                  │
│      │  Wczesne zakończenie:                                            │
│      │  ┌─────────────────────────────────────────┐                     │
│      │  │ OBA czujniki AKTYWNE → natychmiast      │                     │
│      │  │ przejdź do OCENY (nie czekaj 1200s)     │                     │
│      │  └─────────────────────────────────────────┘                     │
│      │                                                                  │
│      ▼ (oba aktywne LUB timeout 1200s)                                  │
│                                                                         │
│  OCENA WYNIKU                                                           │
│      │                                                                  │
│      │  Oblicz time_gap_1:                                              │
│      │  = |czas_zaliczenia_S1 - czas_zaliczenia_S2|                     │
│      │  (tylko jeśli oba zaliczyły)                                     │
│      │                                                                  │
│      ├─► Oba AKTYWNE:                                                   │
│      │   → STATE_PUMP (sukces)                                          │
│      │   → Raport: time_gap_1                                           │
│      │                                                                  │
│      ├─► Tylko S1 AKTYWNY:                                              │
│      │   → STATE_PUMP + ERR_SENSOR_2                                    │
│      │                                                                  │
│      ├─► Tylko S2 AKTYWNY:                                              │
│      │   → STATE_PUMP + ERR_SENSOR_1                                    │
│      │                                                                  │
│      └─► Oba NIEAKTYWNE:                                                │
│          → STATE_IDLE + ERR_FALSE_TRIGGER                               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Parametry konfiguracyjne

### 3.1 Pre-qualification

| Parametr | Wartość | Opis |
|----------|---------|------|
| `PRE_QUAL_WINDOW` | 30s | Okno czasowe na potwierdzenie pierwszego LOW |
| `PRE_QUAL_INTERVAL` | 10s | Interwał między pomiarami |
| `PRE_QUAL_CONFIRM_COUNT` | 3 | Wymagana liczba kolejnych LOW |

**Logika:**
- Pierwszy LOW na dowolnym czujniku → start pre-qualification
- System sprawdza czujniki co `PRE_QUAL_INTERVAL` (10s)
- Wymóg: 3 kolejne LOW na **którymkolwiek** czujniku
- Timeout bez potwierdzenia → cichy powrót do IDLE (bez błędu)

### 3.2 Settling

| Parametr | Wartość | Opis |
|----------|---------|------|
| `SETTLING_TIME` | 60s | Czas uspokojenia wody |

**Logika:**
- Po zaliczeniu pre-qualification → pasywne czekanie 60s
- Brak monitoringu czujników w tym czasie
- Cel: pozwolić wodzie się uspokoić po falowaniu

### 3.3 Debouncing

| Parametr | Wartość | Opis |
|----------|---------|------|
| `TOTAL_DEBOUNCE_TIME` | 1200s | Maksymalny czas fazy debouncing (20 minut) |
| `DEBOUNCE_INTERVAL` | 60s | Interwał między pomiarami (1200/20) |
| `DEBOUNCE_COUNTER` | 4 | Wymagana liczba kolejnych LOW dla zaliczenia |

**Logika dla każdego czujnika (niezależnie):**
```
Pomiar co 60s:
  - Odczyt LOW  → counter++
  - Odczyt HIGH → counter = 0 (reset!)
  - counter >= 4 → czujnik AKTYWNY, zapisz czas zaliczenia
```

**Zakończenie debouncing:**
- **Wczesne:** OBA czujniki osiągną counter >= 4 → natychmiast do OCENY
- **Timeout:** Po 1200s → do OCENY (niezależnie od stanu czujników)

---

## 4. Tabela błędów

| Kod błędu | Warunek | Akcja | Opis |
|-----------|---------|-------|------|
| `ERR_SENSOR_1` | S1 nieaktywny, S2 aktywny | PUMP + raport | Czujnik 1 nie przeszedł debounce |
| `ERR_SENSOR_2` | S2 nieaktywny, S1 aktywny | PUMP + raport | Czujnik 2 nie przeszedł debounce |
| `ERR_FALSE_TRIGGER` | Oba nieaktywne po debounce | IDLE + raport | Fałszywy alarm (przeszedł pre-qual, nie przeszedł debounce) |

**Uwaga:** Pre-qualification niezaliczone → IDLE **bez błędu** (cichy reset).

---

## 5. Raportowane metryki

| Metryka | Kiedy raportowana | Opis |
|---------|-------------------|------|
| `time_gap_1` | Oba czujniki aktywne | Różnica czasu zaliczenia S1 vs S2 (sekundy) |

**Obliczenie time_gap_1:**
```
time_gap_1 = |sensor1_debounce_complete_time - sensor2_debounce_complete_time|
```

---

## 6. Scenariusze działania

### 6.1 Scenariusz: Prawdziwe opadanie wody (sukces)

```
t=0:00    Woda zaczyna opadać
t=0:05    Czujnik 1: pierwszy LOW → START PRE_QUALIFICATION
t=0:15    Czujnik 1: LOW (counter=2)
t=0:25    Czujnik 1: LOW (counter=3) → PRE_QUAL ZALICZONE
t=0:25    → START SETTLING (60s)
t=1:25    → START DEBOUNCING
t=1:25    Pomiar 1: S1=LOW (c1=1), S2=HIGH (c2=0)
t=2:25    Pomiar 2: S1=LOW (c1=2), S2=LOW (c2=1)
t=3:25    Pomiar 3: S1=LOW (c1=3), S2=LOW (c2=2)
t=4:25    Pomiar 4: S1=LOW (c1=4→AKTYWNY), S2=LOW (c2=3)
          S1 zaliczony w t=4:25
t=5:25    Pomiar 5: S2=LOW (c2=4→AKTYWNY)
          S2 zaliczony w t=5:25
          → OBA AKTYWNE → OCENA
          time_gap_1 = |4:25 - 5:25| = 60s
          → PUMP (sukces)
```

### 6.2 Scenariusz: Fałszywe drganie (odrzucone w pre-qual)

```
t=0:00    Przypadkowe drganie czujnika
t=0:00    Czujnik 1: LOW → START PRE_QUALIFICATION
t=0:10    Czujnik 1: HIGH (drganie ustało) → counter=0
t=0:20    Czujnik 1: HIGH → counter=0
t=0:30    TIMEOUT pre-qual bez 3×LOW
          → IDLE (cicho, bez błędu)

Czas zmarnowany: 30s (zamiast 2300s w starej logice!)
```

### 6.3 Scenariusz: Jeden czujnik uszkodzony

```
t=0:00    Woda opada
t=0:00    Czujnik 1: LOW → START PRE_QUALIFICATION
...       (pre-qual zaliczone, settling zakończone)
t=1:25    → START DEBOUNCING
t=1:25    Pomiar 1: S1=LOW (c1=1), S2=HIGH (c2=0)
t=2:25    Pomiar 2: S1=LOW (c1=2), S2=HIGH (c2=0)  ← S2 nie działa!
t=3:25    Pomiar 3: S1=LOW (c1=3), S2=HIGH (c2=0)
t=4:25    Pomiar 4: S1=LOW (c1=4→AKTYWNY), S2=HIGH (c2=0)
...       (czekamy na S2, ale on ciągle HIGH)
t=21:25   TIMEOUT 1200s
          → OCENA: S1=AKTYWNY, S2=NIEAKTYWNY
          → PUMP + ERR_SENSOR_2
```

### 6.4 Scenariusz: Ślimak (ERR_FALSE_TRIGGER)

```
t=0:00    Ślimak wchodzi na czujnik 1
t=0:00    Czujnik 1: LOW → START PRE_QUALIFICATION
t=0:10    Czujnik 1: LOW (counter=2)
t=0:20    Czujnik 1: LOW (counter=3) → PRE_QUAL ZALICZONE
t=0:20    → START SETTLING
t=1:20    → START DEBOUNCING
t=1:20    Pomiar 1: S1=LOW (c1=1), S2=HIGH
t=2:20    Ślimak odpada!
t=2:20    Pomiar 2: S1=HIGH (c1=0), S2=HIGH
t=3:20    Pomiar 3: S1=HIGH (c1=0), S2=HIGH
...       (oba czujniki HIGH przez resztę czasu)
t=21:20   TIMEOUT 1200s
          → OCENA: S1=NIEAKTYWNY, S2=NIEAKTYWNY
          → IDLE + ERR_FALSE_TRIGGER
```

---

## 7. Zmiany w kodzie - przegląd

### 7.1 Pliki do modyfikacji

| Plik | Zmiany |
|------|--------|
| `algorithm_config.h` | Nowe parametry, usunięcie TIME_GAP_1_MAX |
| `water_sensors.cpp` | Nowa logika pre-qual i debouncing |
| `water_sensors.h` | Nowe deklaracje funkcji i zmiennych |
| `water_algorithm.cpp` | Nowe stany, obsługa callbacków |
| `water_algorithm.h` | Nowe stany w enum, nowe metody |

### 7.2 Usunięte parametry

- `TIME_GAP_1_MAX` - zastąpiony przez `TOTAL_DEBOUNCE_TIME`
- `TIME_TO_PUMP` - usunięty (pompa startuje po debounce)
- `THRESHOLD_1`, `THRESHOLD_2`, `THRESHOLD_WATER` - usunięte
- `SENSOR_DEBOUNCE_TIME` - zastąpiony przez nową logikę
- `DEBOUNCE_RATIO` - nieużywany w nowej logice

### 7.3 Nowe stany algorytmu

```cpp
enum AlgorithmState {
    STATE_IDLE = 0,
    STATE_PRE_QUALIFICATION,  // NOWY
    STATE_SETTLING,           // NOWY
    STATE_DEBOUNCING,         // NOWY (zastępuje STATE_TRYB_1_WAIT)
    STATE_TRYB_2_PUMP,
    STATE_TRYB_2_VERIFY,
    STATE_TRYB_2_WAIT_GAP2,
    STATE_LOGGING,
    STATE_ERROR,
    STATE_MANUAL_OVERRIDE
};
```

### 7.4 Nowe kody błędów

```cpp
// W PumpCycle lub osobny enum
static const uint8_t ERR_SENSOR_1 = 0x10;      // Czujnik 1 nie zaliczył
static const uint8_t ERR_SENSOR_2 = 0x20;      // Czujnik 2 nie zaliczył
static const uint8_t ERR_FALSE_TRIGGER = 0x40; // Oba nie zaliczyły (fałszywy alarm)
```

---

## 8. Plan implementacji

### Faza 1: algorithm_config.h
1. Dodać nowe parametry (PRE_QUAL_*, SETTLING_TIME, TOTAL_DEBOUNCE_TIME, etc.)
2. Usunąć stare parametry (TIME_GAP_1_MAX, TIME_TO_PUMP, THRESHOLD_*)
3. Zaktualizować static_assert
4. Dodać nowe stany do enum AlgorithmState
5. Dodać nowe kody błędów

### Faza 2: water_sensors.h / water_sensors.cpp
1. Dodać zmienne stanu pre-qualification
2. Dodać zmienne stanu debouncing (countery dla obu czujników)
3. Zaimplementować logikę pre-qualification
4. Zaimplementować logikę settling (proste czekanie)
5. Zaimplementować logikę debouncing
6. Dodać callbacki do algorytmu

### Faza 3: water_algorithm.h / water_algorithm.cpp
1. Obsłużyć nowe stany w metodzie update()
2. Zaimplementować logikę przejść między stanami
3. Zaimplementować obliczanie time_gap_1
4. Zaimplementować raportowanie błędów
5. Usunąć nieużywany kod (stara logika TIME_GAP_1_MAX)

### Faza 4: Testy
1. Test: prawdziwe opadanie → sukces
2. Test: fałszywe drganie → cichy reset w pre-qual
3. Test: jeden czujnik uszkodzony → ERR_SENSOR_X
4. Test: ślimak scenario → ERR_FALSE_TRIGGER
5. Test: drganie w trakcie debounce → reset countera, ale kontynuacja

---

## 9. Oczekiwane rezultaty

### Przed wdrożeniem
- `time_gap_1` rozrzut: 100s - 9000s+ (chaotyczny)
- `GAP1_FAIL`: 20-30% (fałszywe alarmy)

### Po wdrożeniu
- `time_gap_1` rozrzut: powtarzalny, wąski zakres
- `ERR_SENSOR_X`: tylko prawdziwe awarie czujników (<5%)
- `ERR_FALSE_TRIGGER`: sporadycznie (ślimaki, błędy mechaniczne)
- Fałszywe triggery: odrzucane w pre-qual (30s zamiast 2300s)

---

# CZĘŚĆ II: Faza 2 - Release Verification (po pompowaniu)

---

## 10. Problem z obecną implementacją fazy 2

### 10.1 Opis problemu

Obecna logika weryfikacji po pompowaniu ma kilka słabości:

```cpp
// OBECNY KOD - POJEDYNCZY ODCZYT!
bool sensorsOK = !readWaterSensor1() && !readWaterSensor2();

if (sensorsOK) {
    calculateWaterTrigger();  // ← Akceptuje natychmiast!
}
```

**Problemy:**
1. **Brak debouncingu** - pojedynczy odczyt HIGH może być fałszywy (falowanie wody)
2. **Brak kontekstu** - nie uwzględnia, które czujniki wyzwoliły cykl
3. **Nieprecyzyjne błędy** - `RESULT_WATER_FAIL` nie mówi, który czujnik zawiódł

### 10.2 Obserwowane symptomy

- Fałszywe `water_trigger_time` (zbyt krótkie - bo pierwszy HIGH od falowania)
- Brak informacji, który czujnik nie potwierdził podniesienia wody
- Nierozróżnialność między awarią czujnika a brakiem wody

---

## 11. Nowa architektura - Release Verification

### 11.1 Koncepcja

Weryfikacja podniesienia poziomu wody po pompowaniu z debouncing:

1. **Kontekst z fazy 1** - pamiętamy, które czujniki wyzwoliły cykl
2. **Release Debouncing** - wymagamy 3 kolejnych HIGH dla potwierdzenia
3. **Precyzyjne błędy** - wiemy dokładnie, który czujnik zawiódł

### 11.2 Diagram stanów fazy 2

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  STATE_PUMPING_AND_VERIFY (połączony stan)                              │
│      │                                                                  │
│      │  Pompa pracuje przez PUMP_WORK_TIME                              │
│      │  JEDNOCZEŚNIE: Release verification aktywne!                     │
│      │                                                                  │
│      │  RELEASE_CHECK_INTERVAL = 2s                                     │
│      │  RELEASE_DEBOUNCE_COUNT = 3                                      │
│      │  Timeout: WATER_TRIGGER_MAX_TIME (240s) od startu pompy          │
│      │                                                                  │
│      │  Dla każdego czujnika (który wyzwolił cykl w fazie 1):           │
│      │  ┌─────────────────────────────────────────┐                     │
│      │  │ HIGH → counter++                        │                     │
│      │  │ LOW  → counter = 0 (reset!)             │                     │
│      │  │ counter >= 3 → POTWIERDZONY             │                     │
│      │  │        zapisz czas potwierdzenia        │                     │
│      │  └─────────────────────────────────────────┘                     │
│      │                                                                  │
│      │  Zakończenie gdy WSZYSTKIE spełnione:                            │
│      │  ┌─────────────────────────────────────────┐                     │
│      │  │ 1. Pompa skończyła pracę                │                     │
│      │  │ 2. WSZYSTKIE wymagane czujniki          │                     │
│      │  │    POTWIERDZONE → do LOGGING            │                     │
│      │  └─────────────────────────────────────────┘                     │
│      │                                                                  │
│      ▼ (pompa skończyła + wszystkie potwierdzone) LUB (timeout 240s)                        │
│                                                                         │
│  OCENA WYNIKU                                                           │
│      │                                                                  │
│      │  Oblicz metryki:                                                 │
│      │  - water_trigger_time = najwcześniejsze potwierdzenie - start    │
│      │  - time_gap_2 = |czas_S1 - czas_S2| (jeśli oba potwierdzone)     │
│      │                                                                  │
│      ├─► Wszystkie wymagane POTWIERDZONE:                               │
│      │   → STATE_LOGGING (sukces)                                       │
│      │                                                                  │
│      ├─► S1 potwierdzony, S2 NIE (i był wymagany):                      │
│      │   → STATE_LOGGING + ERR_SENSOR2_RELEASE                          │
│      │                                                                  │
│      ├─► S2 potwierdzony, S1 NIE (i był wymagany):                      │
│      │   → STATE_LOGGING + ERR_SENSOR1_RELEASE                          │
│      │                                                                  │
│      └─► Żaden NIE potwierdzony:                                        │
│          → ERR_NO_WATER                                                 │
│          → Jeśli attempts < MAX → RETRY PUMP                            │
│          → Jeśli attempts >= MAX → STATE_ERROR                          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 11.3 Kluczowa zasada: Kontekst z fazy 1

```
┌────────────────────────────────────────────────────────────────────────┐
│                                                                        │
│  FAZA 1 (debouncing opadania)        FAZA 2 (release verification)     │
│  ─────────────────────────────       ─────────────────────────────     │
│                                                                        │
│  Oba czujniki zaliczyły        →     Oczekuj potwierdzenia z OBU       │
│  (sensor1OK=true, sensor2OK=true)    (sensor1Required=true,            │
│                                       sensor2Required=true)            │
│                                                                        │
│  Tylko S1 zaliczył             →     Oczekuj potwierdzenia TYLKO z S1  │
│  (sensor1OK=true, sensor2OK=false)   (sensor1Required=true,            │
│                                       sensor2Required=false)           │
│                                                                        │
│  Tylko S2 zaliczył             →     Oczekuj potwierdzenia TYLKO z S2  │
│  (sensor1OK=false, sensor2OK=true)   (sensor1Required=false,           │
│                                       sensor2Required=true)            │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

**Uzasadnienie:** Jeśli w fazie 1 tylko jeden czujnik wykrył opadanie wody (np. drugi jest uszkodzony), to nie ma sensu wymagać od niego potwierdzenia w fazie 2.

---

## 12. Parametry konfiguracyjne fazy 2

### 12.1 Release Verification

| Parametr | Wartość | Opis |
|----------|---------|------|
| `RELEASE_CHECK_INTERVAL` | 2s | Interwał między sprawdzeniami czujników |
| `RELEASE_DEBOUNCE_COUNT` | 3 | Wymagana liczba kolejnych HIGH dla potwierdzenia |
| `WATER_TRIGGER_MAX_TIME` | 240s | Maksymalny czas oczekiwania na potwierdzenie |
| `PUMP_MAX_ATTEMPTS` | 3 | Maksymalna liczba prób pompowania |

**Logika dla każdego WYMAGANEGO czujnika:**
```
Pomiar co 2s:
  - Odczyt HIGH → counter++
  - Odczyt LOW  → counter = 0 (reset!)
  - counter >= 3 → czujnik POTWIERDZONY, zapisz czas
```

**Zakończenie release verification:**
- **Wczesne:** WSZYSTKIE wymagane czujniki potwierdzone → natychmiast do LOGGING
- **Timeout:** Po 240s → ocena wyniku (sukces/błąd)

---

## 13. Tabela błędów fazy 2

| Kod błędu | Warunek | Akcja | Opis |
|-----------|---------|-------|------|
| `ERR_SENSOR1_RELEASE` | S1 wymagany i nie potwierdził, S2 OK | LOGGING + raport | Czujnik 1 nie potwierdził podniesienia wody |
| `ERR_SENSOR2_RELEASE` | S2 wymagany i nie potwierdził, S1 OK | LOGGING + raport | Czujnik 2 nie potwierdził podniesienia wody |
| `ERR_NO_WATER` | Żaden wymagany czujnik nie potwierdził | RETRY lub ERROR | Woda nie dotarła (brak wody, awaria pompy, zatkana rurka) |

### 13.1 Logika retry przy ERR_NO_WATER

```
Próba 1: ERR_NO_WATER → RETRY (attempts=2)
Próba 2: ERR_NO_WATER → RETRY (attempts=3)
Próba 3: ERR_NO_WATER → STATE_ERROR (ERROR_PUMP_FAILURE)
```

---

## 14. Raportowane metryki fazy 2

| Metryka | Kiedy raportowana | Opis |
|---------|-------------------|------|
| `water_trigger_time` | Zawsze | Czas od startu pompy do pierwszego potwierdzenia (sekundy) |
| `time_gap_2` | Oba czujniki potwierdzone | Różnica czasu potwierdzenia S1 vs S2 (sekundy) |

**Obliczenia:**
```
water_trigger_time = najwcześniejsze_potwierdzenie - pump_start_time

time_gap_2 = |sensor1_confirm_time - sensor2_confirm_time|
             (tylko jeśli oba wymagane i oba potwierdzone)
```

---

## 15. Scenariusze działania fazy 2

### 15.1 Scenariusz: Sukces - oba czujniki potwierdzone

```
t=0:00    Pompa startuje (oba czujniki wyzwoliły cykl w fazie 1)
t=0:20    Pompa kończy (PUMP_WORK_TIME)
t=0:20    → STATE_RELEASE_VERIFY
t=0:22    Pomiar 1: S1=HIGH (c1=1), S2=LOW (c2=0)  ← woda jeszcze faluje
t=0:24    Pomiar 2: S1=HIGH (c1=2), S2=HIGH (c2=1)
t=0:26    Pomiar 3: S1=HIGH (c1=3→POTWIERDZONY), S2=HIGH (c2=2)
          S1 potwierdzony w t=0:26
t=0:28    Pomiar 4: S2=HIGH (c2=3→POTWIERDZONY)
          S2 potwierdzony w t=0:28
          → OBA POTWIERDZONE → STATE_LOGGING

Wynik:
  water_trigger_time = 26 - 0 = 26s (czas do pierwszego potwierdzenia)
  time_gap_2 = |26 - 28| = 2s
  Błędy: brak
```

### 15.2 Scenariusz: Sukces - tylko jeden czujnik wymagany

```
(W fazie 1 tylko S1 zaliczył debouncing - S2 uszkodzony)

t=0:00    Pompa startuje (sensor1Required=true, sensor2Required=false)
t=0:20    Pompa kończy
t=0:20    → STATE_RELEASE_VERIFY
t=0:22    Pomiar 1: S1=LOW (c1=0)   ← falowanie
t=0:24    Pomiar 2: S1=HIGH (c1=1)
t=0:26    Pomiar 3: S1=HIGH (c1=2)
t=0:28    Pomiar 4: S1=HIGH (c1=3→POTWIERDZONY)
          → S1 POTWIERDZONY (jedyny wymagany) → STATE_LOGGING

Wynik:
  water_trigger_time = 28 - 0 = 28s
  time_gap_2 = 0 (tylko jeden czujnik)
  Błędy: brak (S2 nie był wymagany)
```

### 15.3 Scenariusz: Błąd jednego czujnika

```
(W fazie 1 oba czujniki zaliczyły debouncing)

t=0:00    Pompa startuje (oba wymagane)
t=0:20    Pompa kończy
t=0:20    → STATE_RELEASE_VERIFY
t=0:22    Pomiar 1: S1=HIGH (c1=1), S2=LOW (c2=0)
t=0:24    Pomiar 2: S1=HIGH (c1=2), S2=LOW (c2=0)  ← S2 zablokowany!
t=0:26    Pomiar 3: S1=HIGH (c1=3→POTWIERDZONY), S2=LOW (c2=0)
...       (S2 ciągle LOW przez 240s)
t=4:00    TIMEOUT 240s
          → OCENA: S1=POTWIERDZONY, S2=NIE
          → STATE_LOGGING + ERR_SENSOR2_RELEASE

Wynik:
  water_trigger_time = 26s (czas potwierdzenia S1)
  time_gap_2 = 240s (timeout - oznacza błąd)
  Błędy: ERR_SENSOR2_RELEASE
```

### 15.4 Scenariusz: Brak wody - retry

```
t=0:00    Pompa startuje (próba 1)
t=0:20    Pompa kończy
t=0:20    → STATE_RELEASE_VERIFY
...       (oba czujniki LOW przez 240s - woda nie dotarła)
t=4:00    TIMEOUT 240s
          → OCENA: S1=NIE, S2=NIE
          → ERR_NO_WATER → RETRY (attempts=2)

t=4:00    Pompa startuje (próba 2)
t=4:20    Pompa kończy
...       (nadal oba LOW)
t=8:00    TIMEOUT 240s
          → ERR_NO_WATER → RETRY (attempts=3)

t=8:00    Pompa startuje (próba 3)
t=8:20    Pompa kończy
...       (nadal oba LOW)
t=12:00   TIMEOUT 240s
          → ERR_NO_WATER → attempts >= MAX
          → STATE_ERROR (ERROR_PUMP_FAILURE)
```

### 15.5 Scenariusz: Falowanie wody (debouncing w akcji)

```
t=0:00    Pompa startuje
t=0:20    Pompa kończy (woda faluje!)
t=0:20    → STATE_RELEASE_VERIFY
t=0:22    Pomiar 1: S1=HIGH (c1=1), S2=HIGH (c2=1)
t=0:24    Pomiar 2: S1=LOW (c1=0!), S2=HIGH (c2=2)   ← S1 reset przez falowanie
t=0:26    Pomiar 3: S1=HIGH (c1=1), S2=HIGH (c2=3→POTWIERDZONY)
t=0:28    Pomiar 4: S1=HIGH (c1=2), S2=już OK
t=0:30    Pomiar 5: S1=HIGH (c1=3→POTWIERDZONY)
          → OBA POTWIERDZONE → STATE_LOGGING

Wynik:
  Debouncing odfiltrował fałszywy odczyt z falowania!
  water_trigger_time = 26s (S2 było pierwsze)
  time_gap_2 = |30 - 26| = 4s
```

---

## 16. Kompletny diagram stanów (Faza 1 + Faza 2)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  ═══════════════════ FAZA 1: DEBOUNCING OPADANIA ═══════════════════   │
│                                                                         │
│  STATE_IDLE                                                             │
│      │                                                                  │
│      ▼ (pierwszy LOW)                                                   │
│                                                                         │
│  STATE_PRE_QUALIFICATION (30s, 3×LOW)                                   │
│      │                                                                  │
│      ├─► Timeout → IDLE (cicho)                                         │
│      ▼                                                                  │
│                                                                         │
│  STATE_SETTLING (60s)                                                   │
│      │                                                                  │
│      ▼                                                                  │
│                                                                         │
│  STATE_DEBOUNCING (1200s, 4×LOW na czujnik)                             │
│      │                                                                  │
│      ├─► Oba OK → STATE_PUMP + kontekst(oba=true)                       │
│      ├─► Tylko S1 OK → STATE_PUMP + kontekst(s1=true, s2=false)         │
│      ├─► Tylko S2 OK → STATE_PUMP + kontekst(s1=false, s2=true)         │
│      └─► Oba FAIL → IDLE + ERR_FALSE_TRIGGER                            │
│                                                                         │
│  ═══════════ FAZA 2: POMPOWANIE + RELEASE VERIFICATION ════════════   │
│                                                                         │
│  STATE_PUMPING_AND_VERIFY                                               │
│      │                                                                  │
│      │  Pompa pracuje + jednocześnie monitoring czujników               │
│      │  (240s timeout, 3×HIGH na wymagany czujnik)                      │
│      │                                                                  │
│      ├─► Pompa skończyła + wszystkie wymagane OK → STATE_LOGGING        │
│      ├─► Timeout: S1 OK, S2 NIE → LOGGING + ERR_SENSOR2_RELEASE         │
│      ├─► Timeout: S2 OK, S1 NIE → LOGGING + ERR_SENSOR1_RELEASE         │
│      └─► Timeout: żaden OK → ERR_NO_WATER                               │
│              │                                                          │
│              ├─► attempts < MAX → RETRY PUMP                            │
│              └─► attempts >= MAX → STATE_ERROR                          │
│                                                                         │
│  ════════════════════ ZAKOŃCZENIE CYKLU ════════════════════════════   │
│                                                                         │
│  STATE_LOGGING (5s)                                                     │
│      │                                                                  │
│      ▼                                                                  │
│                                                                         │
│  STATE_IDLE                                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 17. Zmiany w kodzie - przegląd (aktualizacja)

### 17.1 Pliki do modyfikacji (rozszerzenie)

| Plik | Zmiany Faza 1 | Zmiany Faza 2 |
|------|---------------|---------------|
| `algorithm_config.h` | Parametry pre-qual, debouncing | Parametry release verification |
| `water_sensors.cpp` | Logika pre-qual i debouncing | - |
| `water_sensors.h` | Deklaracje funkcji | - |
| `water_algorithm.cpp` | Stany fazy 1, callbacki | Nowy STATE_RELEASE_VERIFY, logika retry |
| `water_algorithm.h` | Stany enum, metody | Nowe zmienne kontekstu, release debounce |

### 17.2 Nowe parametry (faza 2)

```cpp
// ============== PARAMETRY RELEASE VERIFICATION ==============
#define RELEASE_CHECK_INTERVAL      2     // sekundy między sprawdzeniami
#define RELEASE_DEBOUNCE_COUNT      3     // wymagana liczba kolejnych HIGH
```

### 17.3 Nowe flagi błędów

```cpp
// W PumpCycle::sensor_results
static const uint8_t RESULT_GAP1_FAIL = 0x01;           // Faza 1 - timeout
static const uint8_t RESULT_GAP2_FAIL = 0x02;           // Faza 2 - time_gap_2 timeout
static const uint8_t RESULT_WATER_FAIL = 0x04;          // Brak potwierdzenia (oba)
static const uint8_t RESULT_SENSOR1_RELEASE_FAIL = 0x08; // S1 nie potwierdził (S2 OK)
static const uint8_t RESULT_SENSOR2_RELEASE_FAIL = 0x10; // S2 nie potwierdził (S1 OK)
```

### 17.4 Nowe zmienne w WaterAlgorithm

```cpp
// Kontekst z fazy 1 (które czujniki wyzwoliły cykl)
bool sensor1TriggeredCycle;       // Czy S1 zaliczył debouncing fazy 1
bool sensor2TriggeredCycle;       // Czy S2 zaliczył debouncing fazy 1

// Stan debouncingu release (faza 2)
struct {
    uint8_t counter;              // Licznik kolejnych HIGH (0-3)
    bool confirmed;               // Czy osiągnięto 3×HIGH
    uint32_t confirmTime;         // Czas potwierdzenia (sekundy)
} releaseDebounce[2];
```

### 17.5 Stany algorytmu (finalna wersja)

```cpp
enum AlgorithmState {
    STATE_IDLE = 0,

    // Faza 1: Debouncing opadania
    STATE_PRE_QUALIFICATION,      // Szybki test pierwszego LOW
    STATE_SETTLING,               // Czas uspokojenia wody
    STATE_DEBOUNCING,             // Pełna weryfikacja LOW

    // Faza 2: Pompowanie i weryfikacja (POŁĄCZONE!)
    STATE_PUMPING_AND_VERIFY,     // Pompa pracuje + monitoring czujników
                                  // (zastępuje STATE_TRYB_2_PUMP + STATE_TRYB_2_VERIFY + STATE_TRYB_2_WAIT_GAP2)

    // Zakończenie
    STATE_LOGGING,
    STATE_ERROR,
    STATE_MANUAL_OVERRIDE
};
```

**Uproszczenie:** Połączenie pompowania i weryfikacji w jeden stan eliminuje zbędne przejścia i pozwala mierzyć `water_trigger_time` od momentu startu pompy.

---

## 18. Pseudokod implementacji fazy 2

### 18.1 Inicjalizacja w resetCycle()

```cpp
void WaterAlgorithm::resetCycle() {
    // ... istniejący kod ...

    // Reset kontekstu z fazy 1
    sensor1TriggeredCycle = false;
    sensor2TriggeredCycle = false;

    // Reset release debounce
    for (int i = 0; i < 2; i++) {
        releaseDebounce[i].counter = 0;
        releaseDebounce[i].confirmed = false;
        releaseDebounce[i].confirmTime = 0;
    }
}
```

### 18.2 Ustawienie kontekstu w callbackach fazy 1

```cpp
void WaterAlgorithm::onDebounceBothComplete() {
    // Oba czujniki zaliczyły - oba wymagane w fazie 2
    sensor1TriggeredCycle = true;
    sensor2TriggeredCycle = true;

    // ... uruchom pompę ...
}

void WaterAlgorithm::onDebounceTimeout(bool sensor1OK, bool sensor2OK) {
    // Zapamiętaj które zaliczyły - tylko te wymagane w fazie 2
    sensor1TriggeredCycle = sensor1OK;
    sensor2TriggeredCycle = sensor2OK;

    if (sensor1OK || sensor2OK) {
        // ... uruchom pompę ...
    }
}
```

### 18.3 Logika STATE_PUMPING_AND_VERIFY (połączony stan)

```cpp
case STATE_PUMPING_AND_VERIFY: {
    // Sprawdzaj czujniki co RELEASE_CHECK_INTERVAL (2s)
    // NIEZALEŻNIE od tego czy pompa jeszcze pracuje!
    if (currentTime - lastReleaseCheck >= RELEASE_CHECK_INTERVAL) {
        lastReleaseCheck = currentTime;

        // Odczytaj czujniki (HIGH = woda podniesiona = !readWaterSensorX())
        bool sensor1High = !readWaterSensor1();
        bool sensor2High = !readWaterSensor2();

        // Aktualizuj release debounce dla S1 (jeśli wymagany)
        if (sensor1TriggeredCycle && !releaseDebounce[0].confirmed) {
            if (sensor1High) {
                releaseDebounce[0].counter++;
                if (releaseDebounce[0].counter >= RELEASE_DEBOUNCE_COUNT) {
                    releaseDebounce[0].confirmed = true;
                    releaseDebounce[0].confirmTime = currentTime;
                    LOG_INFO("Sensor1 release confirmed at %lus", currentTime);
                }
            } else {
                releaseDebounce[0].counter = 0;  // Reset na LOW
            }
        }

        // Analogicznie dla S2 (jeśli wymagany)
        if (sensor2TriggeredCycle && !releaseDebounce[1].confirmed) {
            if (sensor2High) {
                releaseDebounce[1].counter++;
                if (releaseDebounce[1].counter >= RELEASE_DEBOUNCE_COUNT) {
                    releaseDebounce[1].confirmed = true;
                    releaseDebounce[1].confirmTime = currentTime;
                    LOG_INFO("Sensor2 release confirmed at %lus", currentTime);
                }
            } else {
                releaseDebounce[1].counter = 0;
            }
        }
    }

    // Sprawdź warunki zakończenia
    bool pumpFinished = !isPumpActive();
    bool allConfirmed = true;
    if (sensor1TriggeredCycle && !releaseDebounce[0].confirmed) {
        allConfirmed = false;
    }
    if (sensor2TriggeredCycle && !releaseDebounce[1].confirmed) {
        allConfirmed = false;
    }

    // SUKCES: Pompa skończyła + wszystkie wymagane potwierdzone
    if (pumpFinished && allConfirmed) {
        LOG_INFO("SUCCESS: Pump finished + all sensors confirmed");
        calculateWaterTriggerTime();
        calculateTimeGap2();
        currentState = STATE_LOGGING;
        break;
    }

    // TIMEOUT: 240s od startu pompy
    if (currentTime - pumpStartTime >= WATER_TRIGGER_MAX_TIME) {
        handleReleaseTimeout();
    }
    break;
}
```

### 18.4 Obsługa timeout

```cpp
void WaterAlgorithm::handleReleaseTimeout() {
    // Zatrzymaj pompę jeśli jeszcze pracuje
    if (isPumpActive()) {
        stopPump();
        LOG_WARNING("Pump stopped due to timeout");
    }

    bool s1Required = sensor1TriggeredCycle;
    bool s2Required = sensor2TriggeredCycle;
    bool s1OK = releaseDebounce[0].confirmed;
    bool s2OK = releaseDebounce[1].confirmed;

    LOG_WARNING("Release verification timeout (240s)");
    LOG_INFO("S1: required=%d, confirmed=%d", s1Required, s1OK);
    LOG_INFO("S2: required=%d, confirmed=%d", s2Required, s2OK);

    // Przypadek 1: Przynajmniej jeden wymagany potwierdził
    if ((s1Required && s1OK) || (s2Required && s2OK)) {
        // Sukces częściowy - loguj błąd dla niepotwierdzonego
        if (s1Required && !s1OK) {
            currentCycle.sensor_results |= RESULT_SENSOR1_RELEASE_FAIL;
            LOG_ERROR("ERR_SENSOR1_RELEASE: S1 did not confirm");
        }
        if (s2Required && !s2OK) {
            currentCycle.sensor_results |= RESULT_SENSOR2_RELEASE_FAIL;
            LOG_ERROR("ERR_SENSOR2_RELEASE: S2 did not confirm");
        }

        calculateWaterTriggerTime();
        calculateTimeGap2();
        currentState = STATE_LOGGING;
        return;
    }

    // Przypadek 2: Żaden wymagany nie potwierdził = ERR_NO_WATER
    currentCycle.sensor_results |= RESULT_WATER_FAIL;
    LOG_ERROR("ERR_NO_WATER: No sensor confirmed water delivery");

    if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
        // Retry
        pumpAttempts++;
        LOG_WARNING("Retrying pump, attempt %d/%d", pumpAttempts, PUMP_MAX_ATTEMPTS);

        // Reset release debounce dla nowej próby
        for (int i = 0; i < 2; i++) {
            releaseDebounce[i].counter = 0;
            releaseDebounce[i].confirmed = false;
            releaseDebounce[i].confirmTime = 0;
        }

        // Uruchom pompę ponownie (pozostajemy w STATE_PUMPING_AND_VERIFY)
        pumpStartTime = currentTime;
        triggerPump(calculatePumpWorkTime(), "AUTO_PUMP_RETRY");
        // Stan pozostaje STATE_PUMPING_AND_VERIFY
    } else {
        // Wszystkie próby wyczerpane
        LOG_ERROR("All %d pump attempts failed!", PUMP_MAX_ATTEMPTS);
        currentCycle.error_code = ERROR_PUMP_FAILURE;
        currentState = STATE_ERROR;
        startErrorSignal(ERROR_PUMP_FAILURE);
    }
}
```

### 18.5 Obliczanie metryk

```cpp
void WaterAlgorithm::calculateWaterTriggerTime() {
    uint32_t earliestConfirm = UINT32_MAX;

    if (releaseDebounce[0].confirmed) {
        earliestConfirm = min(earliestConfirm, releaseDebounce[0].confirmTime);
    }
    if (releaseDebounce[1].confirmed) {
        earliestConfirm = min(earliestConfirm, releaseDebounce[1].confirmTime);
    }

    if (earliestConfirm != UINT32_MAX) {
        currentCycle.water_trigger_time = earliestConfirm - pumpStartTime;
    } else {
        currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
    }
}

void WaterAlgorithm::calculateTimeGap2() {
    if (releaseDebounce[0].confirmed && releaseDebounce[1].confirmed) {
        currentCycle.time_gap_2 = abs((int32_t)releaseDebounce[0].confirmTime -
                                      (int32_t)releaseDebounce[1].confirmTime);
    } else {
        // Tylko jeden czujnik potwierdzony lub żaden
        currentCycle.time_gap_2 = 0;
    }
}
```

---

## 19. Plan implementacji (aktualizacja)

### Etap 1: algorithm_config.h
1. Dodać parametry pre-qual, settling, debouncing (faza 1)
2. Dodać parametry release verification (faza 2)
3. Dodać nowe flagi błędów
4. Zaktualizować enum AlgorithmState

### Etap 2: water_sensors.h / water_sensors.cpp
1. Zaimplementować logikę pre-qualification
2. Zaimplementować logikę settling
3. Zaimplementować logikę debouncing (faza 1)
4. Dodać callbacki do algorytmu

### Etap 3: water_algorithm.h
1. Dodać zmienne kontekstu (sensor1TriggeredCycle, sensor2TriggeredCycle)
2. Dodać strukturę releaseDebounce[2]
3. Dodać nowe metody (handleReleaseTimeout, resetReleaseDebounce)

### Etap 4: water_algorithm.cpp
1. Zaimplementować obsługę stanów fazy 1
2. Zaimplementować STATE_RELEASE_VERIFY z debouncing
3. Zaimplementować logikę retry przy ERR_NO_WATER
4. Zaimplementować obliczanie water_trigger_time i time_gap_2
5. Usunąć STATE_TRYB_2_WAIT_GAP2 (zastąpiony przez nową logikę)

### Etap 5: Testy
1. Test: oba czujniki → sukces
2. Test: jeden czujnik wymagany → sukces
3. Test: jeden czujnik fail → ERR_SENSOR_X_RELEASE
4. Test: brak wody → retry → ERROR
5. Test: falowanie → debouncing filtruje

---

## 20. Oczekiwane rezultaty (finalne)

### Przed wdrożeniem

| Metryka | Stan |
|---------|------|
| `time_gap_1` rozrzut | 100s - 9000s+ (chaotyczny) |
| `GAP1_FAIL` | 20-30% (fałszywe alarmy) |
| `water_trigger_time` | Niepewny (brak debouncingu) |
| Diagnostyka fazy 2 | Brak informacji który czujnik zawiódł |

### Po wdrożeniu

| Metryka | Stan |
|---------|------|
| `time_gap_1` rozrzut | Powtarzalny, wąski zakres |
| `ERR_SENSOR_X` (faza 1) | Tylko prawdziwe awarie (<5%) |
| `ERR_FALSE_TRIGGER` | Sporadycznie (ślimaki, błędy mechaniczne) |
| `water_trigger_time` | Wiarygodny (3×HIGH debouncing) |
| `ERR_SENSOR1_RELEASE` / `ERR_SENSOR2_RELEASE` | Precyzyjna identyfikacja problemu |
| `ERR_NO_WATER` | Jasna informacja o braku wody + retry |

---

## 21. Historia zmian dokumentu

| Data | Wersja | Opis |
|------|--------|------|
| 2025-01-24 | 1.0 | Wersja początkowa - Faza 1 (debouncing opadania) |
| 2025-01-25 | 2.0 | Dodano Fazę 2 (Release Verification po pompowaniu) |
| 2025-01-25 | 2.1 | Połączono STATE_PUMP_ACTIVE i STATE_RELEASE_VERIFY w jeden stan STATE_PUMPING_AND_VERIFY - monitoring czujników startuje jednocześnie z pompą |
