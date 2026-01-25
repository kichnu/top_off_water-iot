#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>

// ============== FAZY PROCESU DETEKCJI ==============
enum SensorPhase {
    PHASE_IDLE = 0,           // Czekanie na pierwszy LOW
    PHASE_PRE_QUALIFICATION,  // Szybki test (30s, 3×LOW)
    PHASE_SETTLING,           // Uspokojenie wody (60s)
    PHASE_DEBOUNCING          // Pełna weryfikacja (1200s, 4×LOW)
};

// ============== PODSTAWOWE FUNKCJE ==============
void initWaterSensors();
void updateWaterSensors();
void checkWaterSensors();

// ============== ODCZYT STANU CZUJNIKÓW ==============
bool readWaterSensor1();
bool readWaterSensor2();
String getWaterStatus();
bool shouldActivatePump();

// ============== ZARZĄDZANIE PROCESEM ==============
void resetSensorProcess();

// ============== GETTERY STANU (dla UI/diagnostyki) ==============
SensorPhase getCurrentPhase();
const char* getPhaseString();
uint8_t getPreQualCounter();
uint8_t getDebounceCounter(uint8_t sensorNum);   // sensorNum: 1 lub 2
bool isDebounceComplete(uint8_t sensorNum);      // sensorNum: 1 lub 2
uint32_t getPhaseElapsedTime();                  // sekundy od startu bieżącej fazy
uint32_t getPhaseRemainingTime();                // sekundy do timeout bieżącej fazy

// ============== LEGACY (kompatybilność) ==============
void resetDebounceProcess();
bool isDebounceProcessActive();
uint32_t getDebounceElapsedTime();

#endif
