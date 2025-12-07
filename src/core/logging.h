#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "../config/config.h"

void initLogging();
void logInfo(const char* format, ...);
void logWarning(const char* format, ...);
void logError(const char* format, ...);

// Warunkowe makra logowania - sprawdzają flagę konfiguracyjną
#if ENABLE_FULL_LOGGING
    #define LOG_INFO(format, ...) logInfo("[INFO] " format, ##__VA_ARGS__)
    #define LOG_WARNING(format, ...) logWarning("[WARN] " format, ##__VA_ARGS__)
    #define LOG_ERROR(format, ...) logError("[ERROR] " format, ##__VA_ARGS__)
#else
    #define LOG_INFO(format, ...) do {} while(0)
    #define LOG_WARNING(format, ...) do {} while(0)
    #define LOG_ERROR(format, ...) do {} while(0)
#endif

// Zawsze dostępne makra dla krytycznych błędów
#if ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x) do {} while(0)
    #define DEBUG_PRINTLN(x) do {} while(0)
    #define DEBUG_PRINTF(format, ...) do {} while(0)
#endif

#endif
