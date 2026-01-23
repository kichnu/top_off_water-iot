#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H
#include <Arduino.h>

// #define TIME_TO_PUMP            2400    // czas od TRIGGER do startu pompy
// #define TIME_GAP_1_MAX          2300    // max oczekiwanie na drugi czujnik (TRYB_1)
// #define TIME_GAP_2_MAX          30    //  max oczekiwanie na drugi czujnik (TRYB_2)
// #define THRESHOLD_1             1000    // próg dla TIME_GAP_1
// #define THRESHOLD_2             15     // próg dla TIME_GAP_2
// #define WATER_TRIGGER_MAX_TIME  240    // max czas na reakcję czujników po starcie pompy
// #define THRESHOLD_WATER         120     // próg dla WATER_TRIGGER_TIME
// #define SENSOR_DEBOUNCE_TIME    1      // debouncing czujników


#define LOGGING_TIME            5      // czas na logowanie po cyklu

// ============== PARAMETRY CZASOWE ALGORYTMU ==============
#define TIME_GAP_1_MAX          2300    // max czas na zaliczenie debouncingu obu czujników (sekundy)
#define TIME_GAP_2_MAX          30      // max oczekiwanie na drugi czujnik przy podnoszeniu (sekundy)
#define WATER_TRIGGER_MAX_TIME  240     // max czas na reakcję czujników po starcie pompy (sekundy)

// ============== PARAMETRY DEBOUNCINGU FAZY 1 ==============
#define DEBOUNCE_COUNTER_1      4       // liczba wymaganych pomiarów LOW dla zaliczenia
#define DEBOUNCE_RATIO          0.6     // współczynnik czasu debouncingu względem TIME_GAP_1_MAX

// ============== PARAMETRY POMPY ==============
#define PUMP_MAX_ATTEMPTS       3      // Maksymalna liczba prób pompy w TRYB_2
#define SINGLE_DOSE_VOLUME      200    // ml - objętość jednej dolewki
// #define SINGLE_DOSE_VOLUME      20    // ml - objętość jednej dolewki
#define FILL_WATER_MAX          2000   // ml - max dolewka na dobę

// ============== SYGNALIZACJA BŁĘDÓW ==============
#define ERROR_PULSE_HIGH        100    // ms - czas impulsu HIGH
#define ERROR_PULSE_LOW         100    // ms - czas przerwy między impulsami
#define ERROR_PAUSE             2000   // ms - pauza przed powtórzeniem sekwencji

// ============== SPRAWDZENIA INTEGRALNOŚCI ==============
static_assert(TIME_GAP_1_MAX > 100, "TIME_GAP_1_MAX must be > 100 seconds");
static_assert(DEBOUNCE_COUNTER_1 >= 2, "DEBOUNCE_COUNTER_1 must be >= 2");
static_assert(DEBOUNCE_COUNTER_1 <= 10, "DEBOUNCE_COUNTER_1 must be <= 10");
static_assert(SINGLE_DOSE_VOLUME >= 100 && SINGLE_DOSE_VOLUME <= 300, "SINGLE_DOSE_VOLUME must be 100-300ml");
static_assert(FILL_WATER_MAX >= 1000 && FILL_WATER_MAX <= 3000, "FILL_WATER_MAX must be 1000-3000ml");
static_assert(LOGGING_TIME == 5, "LOGGING_TIME must be 5 seconds");



// ============== STANY ALGORYTMU ==============
enum AlgorithmState {
    STATE_IDLE = 0,           // Oczekiwanie na TRIGGER
    STATE_TRYB_1_WAIT,        // Czekanie na TIME_GAP_1
    STATE_TRYB_1_DELAY,       // Odliczanie TIME_TO_PUMP
    STATE_TRYB_2_PUMP,        // Pompa pracuje
    STATE_TRYB_2_VERIFY,      // Weryfikacja reakcji czujników
    STATE_TRYB_2_WAIT_GAP2,   // Czekanie na TIME_GAP_2
    STATE_LOGGING,            // Logowanie wyników
    STATE_ERROR,              // Stan błędu
    STATE_MANUAL_OVERRIDE     // Manual pump przerwał cykl
};

// ============== KODY BŁĘDÓW ==============
enum ErrorCode {
    ERROR_NONE = 0,
    ERROR_DAILY_LIMIT = 1,    // ERR1: przekroczono FILL_WATER_MAX
    ERROR_PUMP_FAILURE = 2,   // ERR2: 3 nieudane próby pompy
    ERROR_BOTH = 3            // ERR0: oba błędy
};

// ============== STRUKTURA CYKLU ==============
struct PumpCycle {
    uint32_t timestamp;        // Unix timestamp rozpoczęcia
    uint32_t trigger_time;     // Czas aktywacji TRIGGER
    uint32_t time_gap_1;       // Czas między T1 i T2 (opadanie)
    uint32_t time_gap_2;       // Czas między T1 i T2 (podnoszenie)
    uint32_t water_trigger_time; // Czas reakcji czujników na pompę
    uint16_t pump_duration;    // Rzeczywisty czas pracy pompy
    uint8_t  pump_attempts;    // Liczba prób pompy
    uint8_t  sensor_results;   // Flagi wyników (bity 0-2)
    uint8_t  error_code;       // Kod błędu
    uint16_t  volume_dose;      // Objętość w ml
    
    // Sensor results bit flags
    static const uint8_t RESULT_GAP1_FAIL = 0x01;  // TIME_GAP_1 >= THRESHOLD_1
    static const uint8_t RESULT_GAP2_FAIL = 0x02;  // TIME_GAP_2 >= THRESHOLD_2
    static const uint8_t RESULT_WATER_FAIL = 0x04; // WATER_TRIGGER >= THRESHOLD_WATER
};

// ============== FUNKCJE POMOCNICZE ==============
inline uint8_t sensor_time_match_function(uint16_t time_ms, uint16_t threshold_ms) {
    return (time_ms >= threshold_ms) ? 1 : 0;
}

// ============== OBLICZANIE CZASU POMPY ==============
inline uint16_t calculatePumpWorkTime(float volumePerSecond) {
    // PUMP_WORK_TIME = SINGLE_DOSE_VOLUME / volumePerSecond
    return (uint16_t)(SINGLE_DOSE_VOLUME / volumePerSecond);
}

// Sprawdzenie zgodności z ograniczeniami
inline bool validatePumpWorkTime(uint16_t pumpWorkTime) {
    return pumpWorkTime <= WATER_TRIGGER_MAX_TIME;
}

#endif