#include "water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../core/logging.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"

// ============== STAN PROCESU DETEKCJI ==============
static SensorPhase currentPhase = PHASE_IDLE;
static uint32_t phaseStartTime = 0;
static uint32_t lastCheckTime = 0;

// ============== STAN PRE-QUALIFICATION ==============
static struct {
    uint8_t counter;           // Licznik kolejnych LOW (0 do PRE_QUAL_CONFIRM_COUNT)
    bool anyLowDetected;       // Czy wykryto jakikolwiek LOW
} preQualState = {0, false};

// ============== STAN DEBOUNCING ==============
static struct {
    uint8_t counter;           // Licznik kolejnych LOW (0 do DEBOUNCE_COUNTER)
    bool complete;             // Czy zaliczony
    uint32_t completeTime;     // Czas zaliczenia (sekundy od boot)
} debounceState[2] = {{0, false, 0}, {0, false, 0}};

// ============== FUNKCJE POMOCNICZE ==============
static void resetPreQualState() {
    preQualState.counter = 0;
    preQualState.anyLowDetected = false;
}

static void resetDebounceState() {
    for (int i = 0; i < 2; i++) {
        debounceState[i].counter = 0;
        debounceState[i].complete = false;
        debounceState[i].completeTime = 0;
    }
}

static void transitionToPhase(SensorPhase newPhase) {
    LOG_INFO("Phase transition: %s -> %s", getPhaseString(),
             newPhase == PHASE_IDLE ? "IDLE" :
             newPhase == PHASE_PRE_QUALIFICATION ? "PRE_QUAL" :
             newPhase == PHASE_SETTLING ? "SETTLING" : "DEBOUNCING");

    currentPhase = newPhase;
    phaseStartTime = millis() / 1000;
    lastCheckTime = phaseStartTime;
}

// ============== INICJALIZACJA ==============
void initWaterSensors() {
    pinMode(WATER_SENSOR_1_PIN, INPUT_PULLUP);
    pinMode(WATER_SENSOR_2_PIN, INPUT_PULLUP);

    currentPhase = PHASE_IDLE;
    phaseStartTime = 0;
    lastCheckTime = 0;
    resetPreQualState();
    resetDebounceState();

    LOG_INFO("Water sensors initialized on pins %d and %d",
             WATER_SENSOR_1_PIN, WATER_SENSOR_2_PIN);
    LOG_INFO("Phase 1 config: PRE_QUAL=%ds/%d×%ds, SETTLING=%ds, DEBOUNCE=%ds/%d×%ds",
             PRE_QUAL_WINDOW, PRE_QUAL_CONFIRM_COUNT, PRE_QUAL_INTERVAL,
             SETTLING_TIME,
             TOTAL_DEBOUNCE_TIME, DEBOUNCE_COUNTER, DEBOUNCE_INTERVAL);
}

// ============== ODCZYT SUROWY ==============
bool readWaterSensor1() {
    return digitalRead(WATER_SENSOR_1_PIN) == LOW;
}

bool readWaterSensor2() {
    return digitalRead(WATER_SENSOR_2_PIN) == LOW;
}

// ============== RESET PROCESU ==============
void resetSensorProcess() {
    currentPhase = PHASE_IDLE;
    phaseStartTime = 0;
    lastCheckTime = 0;
    resetPreQualState();
    resetDebounceState();
    LOG_INFO("Sensor process reset to IDLE");
}

// ============== GŁÓWNA LOGIKA ==============
void checkWaterSensors() {
    uint32_t currentTime = millis() / 1000;
    bool sensor1Low = readWaterSensor1();
    bool sensor2Low = readWaterSensor2();
    bool anyLow = sensor1Low || sensor2Low;

    switch (currentPhase) {

        // ===============================================
        // PHASE_IDLE: Czekanie na pierwszy LOW
        // ===============================================
        case PHASE_IDLE: {
            if (anyLow) {
                LOG_INFO("====================================");
                LOG_INFO("FIRST LOW DETECTED - Starting PRE_QUAL");
                LOG_INFO("====================================");
                LOG_INFO("S1=%s, S2=%s", sensor1Low ? "LOW" : "HIGH", sensor2Low ? "LOW" : "HIGH");

                transitionToPhase(PHASE_PRE_QUALIFICATION);
                resetPreQualState();
                preQualState.anyLowDetected = true;
                preQualState.counter = 1;  // Pierwszy LOW już wykryty

                // Powiadom algorytm o starcie procesu
                waterAlgorithm.onPreQualificationStart();
            }
            break;
        }

        // ===============================================
        // PHASE_PRE_QUALIFICATION: Szybki test (30s, 3×LOW co 10s)
        // ===============================================
        case PHASE_PRE_QUALIFICATION: {
            uint32_t elapsed = currentTime - phaseStartTime;

            // Sprawdź timeout
            if (elapsed >= PRE_QUAL_WINDOW) {
                LOG_INFO("====================================");
                LOG_INFO("PRE_QUAL TIMEOUT - returning to IDLE");
                LOG_INFO("====================================");
                LOG_INFO("Counter was: %d/%d (needed %d consecutive)",
                         preQualState.counter, PRE_QUAL_CONFIRM_COUNT, PRE_QUAL_CONFIRM_COUNT);

                // Cichy powrót do IDLE (bez błędu)
                waterAlgorithm.onPreQualificationFail();
                resetSensorProcess();
                break;
            }

            // Sprawdź czy czas na kolejny pomiar
            if (currentTime - lastCheckTime < PRE_QUAL_INTERVAL) {
                break;
            }
            lastCheckTime = currentTime;

            // Pomiar - wymagamy LOW na którymkolwiek czujniku
            if (anyLow) {
                preQualState.counter++;
                LOG_INFO("PRE_QUAL: LOW confirmed, counter=%d/%d (elapsed %lus)",
                         preQualState.counter, PRE_QUAL_CONFIRM_COUNT, elapsed);

                // Sprawdź czy osiągnięto wymaganą liczbę
                if (preQualState.counter >= PRE_QUAL_CONFIRM_COUNT) {
                    LOG_INFO("====================================");
                    LOG_INFO("PRE_QUAL SUCCESS - %d consecutive LOWs", PRE_QUAL_CONFIRM_COUNT);
                    LOG_INFO("====================================");
                    LOG_INFO("Starting SETTLING phase (%ds)", SETTLING_TIME);

                    waterAlgorithm.onPreQualificationSuccess();
                    transitionToPhase(PHASE_SETTLING);
                }
            } else {
                // HIGH - reset licznika
                if (preQualState.counter > 0) {
                    LOG_INFO("PRE_QUAL: HIGH detected, counter reset (was %d)", preQualState.counter);
                }
                preQualState.counter = 0;
            }
            break;
        }

        // ===============================================
        // PHASE_SETTLING: Uspokojenie wody (60s pasywne)
        // ===============================================
        case PHASE_SETTLING: {
            uint32_t elapsed = currentTime - phaseStartTime;

            // Status log co 15s
            static uint32_t lastSettlingLog = 0;
            if (currentTime - lastSettlingLog >= 15) {
                LOG_INFO("SETTLING: %lu/%ds", elapsed, SETTLING_TIME);
                lastSettlingLog = currentTime;
            }

            // Sprawdź czy minęło SETTLING_TIME
            if (elapsed >= SETTLING_TIME) {
                LOG_INFO("====================================");
                LOG_INFO("SETTLING COMPLETE - Starting DEBOUNCING");
                LOG_INFO("====================================");
                LOG_INFO("Debounce config: %ds timeout, %ds interval, %d×LOW needed",
                         TOTAL_DEBOUNCE_TIME, DEBOUNCE_INTERVAL, DEBOUNCE_COUNTER);

                waterAlgorithm.onSettlingComplete();
                transitionToPhase(PHASE_DEBOUNCING);
                resetDebounceState();
            }
            break;
        }

        // ===============================================
        // PHASE_DEBOUNCING: Pełna weryfikacja (1200s, 4×LOW co 60s)
        // ===============================================
        case PHASE_DEBOUNCING: {
            uint32_t elapsed = currentTime - phaseStartTime;

            // Sprawdź timeout
            if (elapsed >= TOTAL_DEBOUNCE_TIME) {
                LOG_INFO("====================================");
                LOG_INFO("DEBOUNCE TIMEOUT (%ds)", TOTAL_DEBOUNCE_TIME);
                LOG_INFO("====================================");

                bool s1OK = debounceState[0].complete;
                bool s2OK = debounceState[1].complete;

                LOG_INFO("S1: %s (counter=%d)", s1OK ? "COMPLETE" : "FAILED", debounceState[0].counter);
                LOG_INFO("S2: %s (counter=%d)", s2OK ? "COMPLETE" : "FAILED", debounceState[1].counter);

                waterAlgorithm.onDebounceTimeout(s1OK, s2OK);
                resetSensorProcess();
                break;
            }

            // Sprawdź czy oba czujniki zaliczone (wczesne zakończenie)
            if (debounceState[0].complete && debounceState[1].complete) {
                LOG_INFO("====================================");
                LOG_INFO("DEBOUNCE SUCCESS - BOTH SENSORS OK");
                LOG_INFO("====================================");
                LOG_INFO("Elapsed: %lus (early completion)", elapsed);

                waterAlgorithm.onDebounceBothComplete();
                resetSensorProcess();
                break;
            }

            // Sprawdź czy czas na kolejny pomiar
            if (currentTime - lastCheckTime < DEBOUNCE_INTERVAL) {
                break;
            }
            lastCheckTime = currentTime;

            // Pomiar dla każdego czujnika niezależnie
            bool sensors[2] = {sensor1Low, sensor2Low};

            for (int i = 0; i < 2; i++) {
                if (debounceState[i].complete) {
                    continue;  // Już zaliczony
                }

                if (sensors[i]) {
                    // LOW - zwiększ licznik
                    debounceState[i].counter++;
                    LOG_INFO("S%d: LOW, counter=%d/%d", i + 1, debounceState[i].counter, DEBOUNCE_COUNTER);

                    if (debounceState[i].counter >= DEBOUNCE_COUNTER) {
                        debounceState[i].complete = true;
                        debounceState[i].completeTime = currentTime;
                        LOG_INFO("S%d: DEBOUNCE COMPLETE at %lus!", i + 1, currentTime);

                        waterAlgorithm.onSensorDebounceComplete(i + 1);
                    }
                } else {
                    // HIGH - reset licznika
                    if (debounceState[i].counter > 0) {
                        LOG_INFO("S%d: HIGH, counter reset (was %d)", i + 1, debounceState[i].counter);
                    }
                    debounceState[i].counter = 0;
                }
            }
            break;
        }
    }
}

// Compatibility function
void updateWaterSensors() {
    checkWaterSensors();
}

String getWaterStatus() {
    bool sensor1 = readWaterSensor1();
    bool sensor2 = readWaterSensor2();

    if (sensor1 && sensor2) {
        return "BOTH_LOW";
    } else if (sensor1) {
        return "SENSOR1_LOW";
    } else if (sensor2) {
        return "SENSOR2_LOW";
    } else {
        return "NORMAL";
    }
}

bool shouldActivatePump() {
    return false;  // Handled by water algorithm
}

// ============== GETTERY STANU ==============

SensorPhase getCurrentPhase() {
    return currentPhase;
}

const char* getPhaseString() {
    switch (currentPhase) {
        case PHASE_IDLE: return "IDLE";
        case PHASE_PRE_QUALIFICATION: return "PRE_QUAL";
        case PHASE_SETTLING: return "SETTLING";
        case PHASE_DEBOUNCING: return "DEBOUNCING";
        default: return "UNKNOWN";
    }
}

uint8_t getPreQualCounter() {
    return preQualState.counter;
}

uint8_t getDebounceCounter(uint8_t sensorNum) {
    if (sensorNum >= 1 && sensorNum <= 2) {
        return debounceState[sensorNum - 1].counter;
    }
    return 0;
}

bool isDebounceComplete(uint8_t sensorNum) {
    if (sensorNum >= 1 && sensorNum <= 2) {
        return debounceState[sensorNum - 1].complete;
    }
    return false;
}

uint32_t getPhaseElapsedTime() {
    if (currentPhase == PHASE_IDLE) return 0;
    return (millis() / 1000) - phaseStartTime;
}

uint32_t getPhaseRemainingTime() {
    if (currentPhase == PHASE_IDLE) return 0;

    uint32_t elapsed = getPhaseElapsedTime();
    uint32_t timeout = 0;

    switch (currentPhase) {
        case PHASE_PRE_QUALIFICATION:
            timeout = PRE_QUAL_WINDOW;
            break;
        case PHASE_SETTLING:
            timeout = SETTLING_TIME;
            break;
        case PHASE_DEBOUNCING:
            timeout = TOTAL_DEBOUNCE_TIME;
            break;
        default:
            return 0;
    }

    return (elapsed < timeout) ? (timeout - elapsed) : 0;
}

// ============== LEGACY FUNCTIONS ==============

void resetDebounceProcess() {
    resetSensorProcess();
}

bool isDebounceProcessActive() {
    return currentPhase != PHASE_IDLE;
}

uint32_t getDebounceElapsedTime() {
    return getPhaseElapsedTime();
}
