// #ifndef WATER_SENSORS_H
// #define WATER_SENSORS_H

// #include <Arduino.h>

// struct SensorState {
//     bool currentReading;
//     bool stableReading;
//     unsigned long lastChangeTime;
//     bool isStable;
// };

// void initWaterSensors();
// void updateWaterSensors();
// String getWaterStatus();
// bool isWaterLevelLow();
// bool shouldActivatePump();
// void checkWaterSensors();
// bool readWaterSensor1();
// bool readWaterSensor2();

// #endif



#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>

// ============== PODSTAWOWE FUNKCJE ==============
void initWaterSensors();
void updateWaterSensors();
void checkWaterSensors();

// ============== ODCZYT STANU CZUJNIKÓW ==============
bool readWaterSensor1();
bool readWaterSensor2();
String getWaterStatus();
bool shouldActivatePump();

// ============== ZARZĄDZANIE DEBOUNCINGIEM ==============
void resetDebounceProcess();

// ============== GETTERY STANU DEBOUNCINGU (dla UI/diagnostyki) ==============
bool isDebounceProcessActive();
uint8_t getDebounceCounter(uint8_t sensorNum);      // sensorNum: 1 lub 2
bool isDebounceComplete(uint8_t sensorNum);         // sensorNum: 1 lub 2
uint32_t getDebounceElapsedTime();                  // sekundy od startu procesu

#endif