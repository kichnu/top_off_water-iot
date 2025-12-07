

#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"
#include <vector>

// extern uint32_t getPumpRemainingTime();  // z pump_controller.h
// extern bool readWaterSensor1();          // z water_sensors.h  
// extern bool readWaterSensor2();          // z water_sensors.h

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

    // State control flags
    bool cycleLogged;

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

    // Status and data access
    AlgorithmState getState() const { return currentState; }
    const char *getStateString() const;
    bool isInCycle() const;
    uint16_t getDailyVolume() const { return dailyVolumeML; }
    ErrorCode getLastError() const { return lastError; }

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
};

extern WaterAlgorithm waterAlgorithm;

#endif