
#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"
#include <vector>

class WaterAlgorithm
{
private:
    AlgorithmState currentState;
    PumpCycle currentCycle;

    // Timing variables
    uint32_t stateStartTime;
    uint32_t triggerStartTime;
    uint32_t sensor1TriggerTime;
    uint32_t sensor2TriggerTime;
    uint32_t sensor1ReleaseTime;
    uint32_t sensor2ReleaseTime;
    uint32_t pumpStartTime;
    uint32_t lastPumpTime;
    bool permission_log;

    bool waterFailDetected = false;

    // FRAM cycle management
    std::vector<PumpCycle> framCycles;
    uint32_t lastFRAMCleanup;
    bool framDataLoaded;

    // Sensor states
    bool lastSensor1State;
    bool lastSensor2State;
    bool waitingForSecondSensor;
    uint8_t pumpAttempts;

    uint32_t sensor1DebounceCompleteTime;   // Czas zaliczenia debouncingu czujnika 1
    uint32_t sensor2DebounceCompleteTime;   // Czas zaliczenia debouncingu czujnika 2
    bool debouncePhaseActive;               // Czy jesteśmy w fazie debouncingu

    // State control flags
    bool cycleLogged;

    // ============== SYSTEM DISABLE FLAG ==============
    // Tracks if system was disabled - used for sensor re-check on re-enable
    bool systemWasDisabled;

    // Error handling
    ErrorCode lastError;
    bool errorSignalActive;
    uint32_t errorSignalStart;
    uint8_t errorPulseCount;
    bool errorPulseState;

    // Daily volume tracking
    std::vector<PumpCycle> todayCycles;
    uint32_t dayStartTime;
    uint16_t dailyVolumeML;
    uint32_t lastResetUTCDay;
    bool resetPending;

    // 🆕 NEW: Available Volume tracking
    uint32_t availableVolumeMax;      // Ustawiona pojemność zbiornika (ml)
    uint32_t availableVolumeCurrent;  // Aktualna ilość wody w zbiorniku (ml)
    
    // 🆕 NEW: Configurable daily limit
    uint16_t fillWaterMaxConfig;      // Konfigurowalny limit dzienny (zastępuje FILL_WATER_MAX)

    // Private methods
    void resetCycle();
    void calculateTimeGap1();
    void calculateTimeGap2();
    void calculateWaterTrigger();
    void logCycleComplete();
    uint16_t calculateDailyVolume();
    void startErrorSignal(ErrorCode error);
    void updateErrorSignal();
    void checkResetButton();

    // ============== SYSTEM DISABLE HANDLER ==============
    // Handles safe shutdown when system is disabled
    void handleSystemDisable();

    // FRAM integration methods
    void loadCyclesFromStorage();
    void saveCycleToStorage(const PumpCycle &cycle);

public:
    WaterAlgorithm();

    // 🆕 NEW: Initialize daily volume AFTER RTC is ready
    void initDailyVolume();

    bool resetDailyVolume();

    uint32_t getCurrentTimeSeconds() const { return millis() / 1000; }

    // Main algorithm update - call this from loop()
    void update();

    // Sensor inputs
    void onSensorStateChange(uint8_t sensorNum, bool triggered);

    // ============== NOWE CALLBACKI DEBOUNCINGU ==============
    void onDebounceProcessStart();                          // Wykryto pierwszy LOW
    void onSensorDebounceComplete(uint8_t sensorNum);       // Czujnik zaliczył debouncing
    void onDebounceBothComplete();                          // Oba czujniki zaliczone
    void onDebounceTimeout(bool sensor1OK, bool sensor2OK); // Timeout TIME_GAP_1_MAX

    // Status and data access
    AlgorithmState getState() const { return currentState; }
    const char *getStateString() const;
    bool isInCycle() const;
    uint16_t getDailyVolume() const { return dailyVolumeML; }
    ErrorCode getLastError() const { return lastError; }

    // 🆕 NEW: Available Volume methods
    void setAvailableVolume(uint32_t maxMl);      // Ustawia max i resetuje current do max
    void refillAvailableVolume();                  // Przywraca current do max
    uint32_t getAvailableVolumeMax() const;
    uint32_t getAvailableVolumeCurrent() const;
    bool isAvailableVolumeEmpty() const;
    
    // 🆕 NEW: Configurable daily limit methods
    void setFillWaterMax(uint16_t maxMl);
    uint16_t getFillWaterMax() const;

    // Get recent cycles for debugging
    std::vector<PumpCycle> getRecentCycles(size_t count = 10);

    // ============== UI STATUS GETTERS ==============
    uint8_t getPumpAttempts() const { return pumpAttempts; }
    String getStateDescription() const;
    uint32_t getRemainingSeconds() const;

    // Reset after error
    void resetFromError();

    // Reset statistics and get current stats for display
    bool resetErrorStatistics();
    bool getErrorStatistics(uint16_t &gap1_sum, uint16_t &gap2_sum, uint16_t &water_sum, uint32_t &last_reset);

    // Manual pump interface
    bool requestManualPump(uint16_t duration_ms);
    void onManualPumpComplete();

    uint32_t getLastResetUTCDay() const { return lastResetUTCDay; }

    void addManualVolume(uint16_t volumeML);

    // ============== SYSTEM DISABLE STATUS ==============
    // Returns true if system was recently re-enabled (for UI feedback)
    bool wasSystemDisabled() const { return systemWasDisabled; }
    void clearSystemWasDisabled() { systemWasDisabled = false; }
};

extern WaterAlgorithm waterAlgorithm;

#endif