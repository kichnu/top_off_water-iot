#include "logging.h"
#include <stdarg.h>

void initLogging() {
#if ENABLE_SERIAL_DEBUG || ENABLE_FULL_LOGGING
    Serial.begin(115200);
    delay(1000);
    
    #if ENABLE_FULL_LOGGING
        LOG_INFO("Logging system initialized");
    #else
        Serial.println("Serial debug initialized (limited logging)");
    #endif
#endif
}

void logInfo(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] %s\n", millis(), buffer);
#endif
}

void logWarning(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] %s\n", millis(), buffer);
#endif
}

void logError(const char* format, ...) {
#if ENABLE_FULL_LOGGING || ENABLE_SERIAL_DEBUG
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] %s\n", millis(), buffer);
#endif
}
