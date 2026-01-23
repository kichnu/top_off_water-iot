
// #include "water_sensors.h"
// #include "../hardware/hardware_pins.h"
// #include "../core/logging.h"
// #include "../algorithm/water_algorithm.h"
// #include "../algorithm/algorithm_config.h"

// void initWaterSensors() {
//     pinMode(WATER_SENSOR_1_PIN, INPUT_PULLUP);
//     pinMode(WATER_SENSOR_2_PIN, INPUT_PULLUP);
//     LOG_INFO("Water sensors initialized on pins %d and %d", 
//              WATER_SENSOR_1_PIN, WATER_SENSOR_2_PIN);
// }

// bool readWaterSensor1() {
//     return digitalRead(WATER_SENSOR_1_PIN) == LOW;
// }

// bool readWaterSensor2() {
//     return digitalRead(WATER_SENSOR_2_PIN) == LOW;
// }

// void checkWaterSensors() {
//     static bool lastSensor1 = false;
//     static bool lastSensor2 = false;
//     static uint32_t lastDebounce1 = 0;
//     static uint32_t lastDebounce2 = 0;
    
//     bool currentSensor1 = digitalRead(WATER_SENSOR_1_PIN) == LOW;
//     bool currentSensor2 = digitalRead(WATER_SENSOR_2_PIN) == LOW;
//     uint32_t currentTimeSeconds = millis() / 1000; // <-- Konwersja do sekund
    
//     // Sensor 1 with debouncing
//     if (currentSensor1 != lastSensor1) {
//         if (currentTimeSeconds - lastDebounce1 > SENSOR_DEBOUNCE_TIME) { // <-- USUŃ * 1000
//             lastDebounce1 = currentTimeSeconds;
//             lastSensor1 = currentSensor1;
            
//             // Notify algorithm
//             waterAlgorithm.onSensorStateChange(1, currentSensor1);
            
//             LOG_INFO("Sensor 1: %s", currentSensor1 ? "TRIGGERED" : "NORMAL");
//         }
//     }
    
//     // Sensor 2 with debouncing
//     if (currentSensor2 != lastSensor2) {
//         if (currentTimeSeconds - lastDebounce2 > SENSOR_DEBOUNCE_TIME) { // <-- USUŃ * 1000
//             lastDebounce2 = currentTimeSeconds;
//             lastSensor2 = currentSensor2;
            
//             // Notify algorithm
//             waterAlgorithm.onSensorStateChange(2, currentSensor2);
            
//             LOG_INFO("Sensor 2: %s", currentSensor2 ? "TRIGGERED" : "NORMAL");
//         }
//     }
// }
// // Compatibility functions for old code
// void updateWaterSensors() {
//     // This is now handled by checkWaterSensors
//     checkWaterSensors();
// }

// String getWaterStatus() {
//     bool sensor1 = readWaterSensor1();
//     bool sensor2 = readWaterSensor2();
    
//     if (sensor1 && sensor2) {
//         return "BOTH_LOW";
//     } else if (sensor1) {
//         return "SENSOR1_LOW";
//     } else if (sensor2) {
//         return "SENSOR2_LOW";
//     } else {
//         return "NORMAL";
//     }
// }

// bool shouldActivatePump() {
//     // Now handled by water algorithm - return false to disable old logic
//     // The algorithm will handle pump activation
//     return false;
// }










#include "water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../core/logging.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"

// ============== WYLICZONE PARAMETRY DEBOUNCINGU ==============
// debounce_total_time = TIME_GAP_1_MAX * DEBOUNCE_RATIO
// debounce_interval = debounce_total_time / (DEBOUNCE_COUNTER_1 - 1)

static uint32_t getDebounceInterval() {
    // Wyliczenie czasu między próbkami w sekundach
    uint32_t totalDebounceTime = (uint32_t)(TIME_GAP_1_MAX * DEBOUNCE_RATIO);
    return totalDebounceTime / (DEBOUNCE_COUNTER_1 - 1);
}

// ============== STAN DEBOUNCINGU ==============
static struct {
    uint8_t counter;              // Licznik kolejnych odczytów LOW (0 do DEBOUNCE_COUNTER_1)
    uint32_t lastCheckTime;       // Czas ostatniego pomiaru (sekundy)
    bool debounceComplete;        // Flaga: debouncing zaliczony
    bool firstLowDetected;        // Flaga: wykryto pierwszy LOW (start procesu)
} sensorDebounce[2] = {{0, 0, false, false}, {0, 0, false, false}};

// Czas rozpoczęcia całego procesu (dla timeout TIME_GAP_1_MAX)
static uint32_t debounceProcessStartTime = 0;
static bool debounceProcessActive = false;

void initWaterSensors() {
    pinMode(WATER_SENSOR_1_PIN, INPUT_PULLUP);
    pinMode(WATER_SENSOR_2_PIN, INPUT_PULLUP);
    
    // Reset stanu debouncingu
    for (int i = 0; i < 2; i++) {
        sensorDebounce[i].counter = 0;
        sensorDebounce[i].lastCheckTime = 0;
        sensorDebounce[i].debounceComplete = false;
        sensorDebounce[i].firstLowDetected = false;
    }
    debounceProcessStartTime = 0;
    debounceProcessActive = false;
    
    uint32_t debounceInterval = getDebounceInterval();
    uint32_t totalDebounceTime = (uint32_t)(TIME_GAP_1_MAX * DEBOUNCE_RATIO);
    
    LOG_INFO("Water sensors initialized on pins %d and %d", 
             WATER_SENSOR_1_PIN, WATER_SENSOR_2_PIN);
    LOG_INFO("Debounce config: interval=%lus, counter=%d, total_time=%lus", 
             debounceInterval, DEBOUNCE_COUNTER_1, totalDebounceTime);
}

bool readWaterSensor1() {
    return digitalRead(WATER_SENSOR_1_PIN) == LOW;
}

bool readWaterSensor2() {
    return digitalRead(WATER_SENSOR_2_PIN) == LOW;
}

// ============== RESET DEBOUNCINGU ==============
void resetDebounceProcess() {
    for (int i = 0; i < 2; i++) {
        sensorDebounce[i].counter = 0;
        sensorDebounce[i].lastCheckTime = 0;
        sensorDebounce[i].debounceComplete = false;
        sensorDebounce[i].firstLowDetected = false;
    }
    debounceProcessStartTime = 0;
    debounceProcessActive = false;
    LOG_INFO("Debounce process reset");
}

// ============== GŁÓWNA LOGIKA DEBOUNCINGU ==============
void checkWaterSensors() {
    uint32_t currentTimeSeconds = millis() / 1000;
    uint32_t debounceInterval = getDebounceInterval();
    
    bool currentSensor1 = digitalRead(WATER_SENSOR_1_PIN) == LOW;
    bool currentSensor2 = digitalRead(WATER_SENSOR_2_PIN) == LOW;
    
    // === STAN IDLE: Czekanie na pierwszy LOW ===
    if (!debounceProcessActive) {
        if (currentSensor1 || currentSensor2) {
            // Pierwszy LOW wykryty - start procesu
            debounceProcessActive = true;
            debounceProcessStartTime = currentTimeSeconds;
            
            LOG_INFO("====================================");
            LOG_INFO("DEBOUNCE PROCESS STARTED");
            LOG_INFO("====================================");
            LOG_INFO("Sensor1: %s, Sensor2: %s", 
                     currentSensor1 ? "LOW" : "HIGH",
                     currentSensor2 ? "LOW" : "HIGH");
            LOG_INFO("Timeout: %d seconds", TIME_GAP_1_MAX);
            
            // Inicjalizuj pierwszy pomiar dla aktywnych czujników
            if (currentSensor1) {
                sensorDebounce[0].firstLowDetected = true;
                sensorDebounce[0].counter = 1;
                sensorDebounce[0].lastCheckTime = currentTimeSeconds;
                LOG_INFO("Sensor1: first LOW detected, counter=1");
            }
            if (currentSensor2) {
                sensorDebounce[1].firstLowDetected = true;
                sensorDebounce[1].counter = 1;
                sensorDebounce[1].lastCheckTime = currentTimeSeconds;
                LOG_INFO("Sensor2: first LOW detected, counter=1");
            }
            
            // Powiadom algorytm o starcie procesu
            waterAlgorithm.onDebounceProcessStart();
        }
        return;  // Nic więcej do roboty w IDLE
    }
    
    // === PROCES AKTYWNY: Sprawdź timeout ===
    uint32_t elapsedTime = currentTimeSeconds - debounceProcessStartTime;
    
    if (elapsedTime >= TIME_GAP_1_MAX) {
        // TIMEOUT - sprawdź wyniki
        bool sensor1OK = sensorDebounce[0].debounceComplete;
        bool sensor2OK = sensorDebounce[1].debounceComplete;
        
        LOG_INFO("====================================");
        LOG_INFO("DEBOUNCE TIMEOUT REACHED");
        LOG_INFO("====================================");
        LOG_INFO("Sensor1 debounce: %s (counter=%d)", 
                 sensor1OK ? "COMPLETE" : "FAILED", sensorDebounce[0].counter);
        LOG_INFO("Sensor2 debounce: %s (counter=%d)", 
                 sensor2OK ? "COMPLETE" : "FAILED", sensorDebounce[1].counter);
        
        // Powiadom algorytm o wyniku
        waterAlgorithm.onDebounceTimeout(sensor1OK, sensor2OK);
        
        // Reset procesu
        resetDebounceProcess();
        return;
    }
    
    // === PROCES AKTYWNY: Sprawdź oba czujniki zaliczone ===
    if (sensorDebounce[0].debounceComplete && sensorDebounce[1].debounceComplete) {
        // Oba zaliczone przed timeout - sukces!
        LOG_INFO("====================================");
        LOG_INFO("DEBOUNCE SUCCESS - BOTH SENSORS OK");
        LOG_INFO("====================================");
        LOG_INFO("Time elapsed: %lu seconds", elapsedTime);
        
        // Powiadom algorytm
        waterAlgorithm.onDebounceBothComplete();
        
        // Reset procesu
        resetDebounceProcess();
        return;
    }
    
    // === PROCES AKTYWNY: Aktualizuj debouncingu dla każdego czujnika ===
    bool sensors[2] = {currentSensor1, currentSensor2};
    
    for (int i = 0; i < 2; i++) {
        // Pomiń jeśli już zaliczony
        if (sensorDebounce[i].debounceComplete) {
            continue;
        }
        
        // Sprawdź czy wykryto pierwszy LOW dla tego czujnika
        if (!sensorDebounce[i].firstLowDetected) {
            if (sensors[i]) {
                // Pierwszy LOW dla tego czujnika
                sensorDebounce[i].firstLowDetected = true;
                sensorDebounce[i].counter = 1;
                sensorDebounce[i].lastCheckTime = currentTimeSeconds;
                LOG_INFO("Sensor%d: first LOW detected, counter=1", i + 1);
            }
            continue;
        }
        
        // Sprawdź czy minął czas na kolejny pomiar
        if (currentTimeSeconds - sensorDebounce[i].lastCheckTime < debounceInterval) {
            continue;  // Za wcześnie na kolejny pomiar
        }
        
        // Czas na pomiar
        sensorDebounce[i].lastCheckTime = currentTimeSeconds;
        
        if (sensors[i]) {
            // LOW - zaliczenie, zwiększ licznik
            sensorDebounce[i].counter++;
            LOG_INFO("Sensor%d: LOW confirmed, counter=%d/%d", 
                     i + 1, sensorDebounce[i].counter, DEBOUNCE_COUNTER_1);
            
            // Sprawdź czy osiągnięto wymaganą liczbę
            if (sensorDebounce[i].counter >= DEBOUNCE_COUNTER_1) {
                sensorDebounce[i].debounceComplete = true;
                LOG_INFO("Sensor%d: DEBOUNCE COMPLETE!", i + 1);
                
                // Powiadom algorytm o zaliczeniu pojedynczego czujnika
                waterAlgorithm.onSensorDebounceComplete(i + 1);
            }
        } else {
            // HIGH - reset licznika
            if (sensorDebounce[i].counter > 0) {
                LOG_INFO("Sensor%d: HIGH detected, counter reset (was %d)", 
                         i + 1, sensorDebounce[i].counter);
            }
            sensorDebounce[i].counter = 0;
            // Uwaga: firstLowDetected pozostaje true - czekamy na kolejny LOW
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

// ============== GETTERY STANU DEBOUNCINGU (dla UI/diagnostyki) ==============
bool isDebounceProcessActive() {
    return debounceProcessActive;
}

uint8_t getDebounceCounter(uint8_t sensorNum) {
    if (sensorNum >= 1 && sensorNum <= 2) {
        return sensorDebounce[sensorNum - 1].counter;
    }
    return 0;
}

bool isDebounceComplete(uint8_t sensorNum) {
    if (sensorNum >= 1 && sensorNum <= 2) {
        return sensorDebounce[sensorNum - 1].debounceComplete;
    }
    return false;
}

uint32_t getDebounceElapsedTime() {
    if (!debounceProcessActive) return 0;
    return (millis() / 1000) - debounceProcessStartTime;
}