#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H
#include <Arduino.h>

#define LOGGING_TIME            5      // czas na logowanie po cyklu

// ============== PARAMETRY PRE-QUALIFICATION (szybki test pierwszego LOW) ==============
#define PRE_QUAL_WINDOW         30     // sekundy - okno czasowe na potwierdzenie
#define PRE_QUAL_INTERVAL       10     // sekundy - interwał między pomiarami
#define PRE_QUAL_CONFIRM_COUNT  3      // wymagana liczba kolejnych LOW

// ============== PARAMETRY SETTLING (uspokojenie wody) ==============
#define SETTLING_TIME           60     // sekundy - czas pasywnego czekania

// ============== PARAMETRY DEBOUNCING FAZY 1 (weryfikacja opadania wody) ==============
#define TOTAL_DEBOUNCE_TIME     1200   // sekundy - maksymalny czas fazy debouncing (20 minut)
#define DEBOUNCE_INTERVAL       60     // sekundy - interwał między pomiarami
#define DEBOUNCE_COUNTER        4      // wymagana liczba kolejnych LOW dla zaliczenia

// ============== PARAMETRY RELEASE VERIFICATION (faza 2 - podnoszenie wody) ==============
#define RELEASE_CHECK_INTERVAL  2      // sekundy między sprawdzeniami czujników
#define RELEASE_DEBOUNCE_COUNT  3      // wymagana liczba kolejnych HIGH dla potwierdzenia
#define WATER_TRIGGER_MAX_TIME  240    // max czas na reakcję czujników po starcie pompy (sekundy)

// ============== PARAMETRY POMPY ==============
#define PUMP_MAX_ATTEMPTS       3      // Maksymalna liczba prób pompy
#define SINGLE_DOSE_VOLUME      200    // ml - objętość jednej dolewki
#define FILL_WATER_MAX          2000   // ml - max dolewka na dobę

// ============== SYGNALIZACJA BŁĘDÓW ==============
#define ERROR_PULSE_HIGH        100    // ms - czas impulsu HIGH
#define ERROR_PULSE_LOW         100    // ms - czas przerwy między impulsami
#define ERROR_PAUSE             2000   // ms - pauza przed powtórzeniem sekwencji

// ============== LEGACY (dla kompatybilności) ==============
#define TIME_GAP_1_MAX          TOTAL_DEBOUNCE_TIME  // alias dla starych odwołań
#define DEBOUNCE_COUNTER_1      DEBOUNCE_COUNTER     // alias dla starych odwołań

// ============== SPRAWDZENIA INTEGRALNOŚCI ==============
static_assert(PRE_QUAL_WINDOW >= 20 && PRE_QUAL_WINDOW <= 60, "PRE_QUAL_WINDOW must be 20-60s");
static_assert(PRE_QUAL_INTERVAL >= 5 && PRE_QUAL_INTERVAL <= 15, "PRE_QUAL_INTERVAL must be 5-15s");
static_assert(PRE_QUAL_CONFIRM_COUNT >= 2 && PRE_QUAL_CONFIRM_COUNT <= 5, "PRE_QUAL_CONFIRM_COUNT must be 2-5");
static_assert(SETTLING_TIME >= 30 && SETTLING_TIME <= 120, "SETTLING_TIME must be 30-120s");
static_assert(TOTAL_DEBOUNCE_TIME >= 600 && TOTAL_DEBOUNCE_TIME <= 2400, "TOTAL_DEBOUNCE_TIME must be 600-2400s");
static_assert(DEBOUNCE_INTERVAL >= 30 && DEBOUNCE_INTERVAL <= 120, "DEBOUNCE_INTERVAL must be 30-120s");
static_assert(DEBOUNCE_COUNTER >= 2 && DEBOUNCE_COUNTER <= 10, "DEBOUNCE_COUNTER must be 2-10");
static_assert(SINGLE_DOSE_VOLUME >= 100 && SINGLE_DOSE_VOLUME <= 800, "SINGLE_DOSE_VOLUME must be 100-300ml");
static_assert(FILL_WATER_MAX >= 1000 && FILL_WATER_MAX <= 3000, "FILL_WATER_MAX must be 1000-3000ml");
static_assert(LOGGING_TIME == 5, "LOGGING_TIME must be 5 seconds");

// ============== STANY ALGORYTMU ==============
enum AlgorithmState {
    STATE_IDLE = 0,             // Oczekiwanie na pierwszy LOW

    // Faza 1: Debouncing opadania wody
    STATE_PRE_QUALIFICATION,    // Szybki test pierwszego LOW (30s, 3×LOW)
    STATE_SETTLING,             // Czas uspokojenia wody (60s)
    STATE_DEBOUNCING,           // Pełna weryfikacja LOW (1200s, 4×LOW)

    // Faza 2: Pompowanie i weryfikacja
    STATE_PUMPING_AND_VERIFY,   // Pompa + monitoring czujników (240s timeout)

    // Zakończenie
    STATE_LOGGING,              // Logowanie wyników (5s)
    STATE_ERROR,                // Stan błędu
    STATE_MANUAL_OVERRIDE       // Manual pump przerwał cykl
};

// Legacy state aliases (dla kompatybilności wstecznej)
#define STATE_TRYB_1_WAIT       STATE_DEBOUNCING
#define STATE_TRYB_1_DELAY      STATE_SETTLING
#define STATE_TRYB_2_PUMP       STATE_PUMPING_AND_VERIFY
#define STATE_TRYB_2_VERIFY     STATE_PUMPING_AND_VERIFY
#define STATE_TRYB_2_WAIT_GAP2  STATE_LOGGING

// ============== KODY BŁĘDÓW ==============
enum ErrorCode {
    ERROR_NONE = 0,
    ERROR_DAILY_LIMIT = 1,      // ERR1: przekroczono FILL_WATER_MAX
    ERROR_PUMP_FAILURE = 2,     // ERR2: 3 nieudane próby pompy
    ERROR_BOTH = 3              // ERR0: oba błędy
};

// ============== STRUKTURA CYKLU ==============
struct PumpCycle {
    uint32_t timestamp;         // Unix timestamp rozpoczęcia
    uint32_t trigger_time;      // Czas aktywacji TRIGGER
    uint32_t time_gap_1;        // Czas między zaliczeniem S1 i S2 (opadanie)
    uint32_t time_gap_2;        // Czas między potwierdzeniem S1 i S2 (podnoszenie)
    uint32_t water_trigger_time;// Czas reakcji czujników na pompę
    uint16_t pump_duration;     // Rzeczywisty czas pracy pompy
    uint8_t  pump_attempts;     // Liczba prób pompy
    uint8_t  sensor_results;    // Flagi wyników
    uint8_t  error_code;        // Kod błędu
    uint16_t volume_dose;       // Objętość w ml

    // Sensor results bit flags - Faza 1 (debouncing opadania)
    static const uint8_t RESULT_GAP1_FAIL = 0x01;            // Timeout debounce (tylko 1 czujnik zaliczył)
    static const uint8_t RESULT_GAP2_FAIL = 0x02;            // Legacy: nieużywane
    static const uint8_t RESULT_FALSE_TRIGGER = 0x20;        // Fałszywy alarm (pre-qual OK, debounce FAIL)
    static const uint8_t RESULT_SENSOR1_DEBOUNCE_FAIL = 0x40;// S1 nie zaliczył debounce (S2 OK)
    static const uint8_t RESULT_SENSOR2_DEBOUNCE_FAIL = 0x80;// S2 nie zaliczył debounce (S1 OK)

    // Sensor results bit flags - Faza 2 (release verification)
    static const uint8_t RESULT_WATER_FAIL = 0x04;           // Żaden czujnik nie potwierdził
    static const uint8_t RESULT_SENSOR1_RELEASE_FAIL = 0x08; // S1 nie potwierdził (S2 OK)
    static const uint8_t RESULT_SENSOR2_RELEASE_FAIL = 0x10; // S2 nie potwierdził (S1 OK)
};

// ============== OBLICZANIE CZASU POMPY ==============
inline uint16_t calculatePumpWorkTime(float volumePerSecond) {
    return (uint16_t)(SINGLE_DOSE_VOLUME / volumePerSecond);
}

inline bool validatePumpWorkTime(uint16_t pumpWorkTime) {
    return pumpWorkTime <= WATER_TRIGGER_MAX_TIME;
}

#endif
