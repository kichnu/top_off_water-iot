
#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../config/config.h" 
#include "../hardware/hardware_pins.h"
#include "algorithm_config.h" 
#include "../hardware/fram_controller.h"  
#include "../network/vps_logger.h"
#include "../hardware/rtc_controller.h" 


WaterAlgorithm waterAlgorithm;

WaterAlgorithm::WaterAlgorithm() {
    currentState = STATE_IDLE;
    resetCycle();
    dayStartTime = millis();
    
    dailyVolumeML = 0;
    lastResetUTCDay = 0;
    resetPending = false;

    // 🆕 NEW: Initialize Available Volume and Fill Water Max
    availableVolumeMax = 10000;      // Default 10L
    availableVolumeCurrent = 10000;
    fillWaterMaxConfig = FILL_WATER_MAX;  // Default from config
    
    lastError = ERROR_NONE;
    errorSignalActive = false;
    lastSensor1State = false;
    lastSensor2State = false;
    todayCycles.clear();

    sensor1DebounceCompleteTime = 0;
    sensor2DebounceCompleteTime = 0;
    debouncePhaseActive = false;

    // ============== RELEASE VERIFICATION INIT ==============
    sensor1TriggeredCycle = false;
    sensor2TriggeredCycle = false;
    lastReleaseCheck = 0;
    for (int i = 0; i < 2; i++) {
        releaseDebounce[i].counter = 0;
        releaseDebounce[i].confirmed = false;
        releaseDebounce[i].confirmTime = 0;
    }

    framDataLoaded = false;
    framCycles.clear();

    // ============== SYSTEM DISABLE FLAG INIT ==============
    systemWasDisabled = false;

    // NOTE: FRAM data loaded later via initFromFRAM() — called from setup() after initNVS()

    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    pinMode(RESET_PIN, INPUT_PULLUP);
    
    LOG_INFO("WaterAlgorithm constructor completed (minimal init)");
}

void WaterAlgorithm::resetCycle() {
    currentCycle = {};
    currentCycle.timestamp = getUnixTimestamp();
    triggerStartTime = 0;
    sensor1TriggerTime = 0;
    sensor2TriggerTime = 0;
    pumpStartTime = 0;
    pumpAttempts = 0;
    cycleLogged = false;
    permission_log = true;
    waterFailDetected = false;
    sensor1DebounceCompleteTime = 0;
    sensor2DebounceCompleteTime = 0;
    debouncePhaseActive = false;

    // Release verification reset
    sensor1TriggeredCycle = false;
    sensor2TriggeredCycle = false;
    lastReleaseCheck = 0;
    resetReleaseDebounce();
}

// ============== SYSTEM DISABLE HANDLER ==============
void WaterAlgorithm::handleSystemDisable() {
    // Called when isSystemDisabled() returns true
    
    // If already in IDLE - nothing to do, just mark as disabled
    if (currentState == STATE_IDLE) {
        if (!systemWasDisabled) {
            LOG_INFO("System disabled while IDLE - safe state");
            systemWasDisabled = true;
        }
        return;
    }
    
    // If in ERROR state - keep error state, just mark
    if (currentState == STATE_ERROR) {
        if (!systemWasDisabled) {
            LOG_INFO("System disabled while in ERROR state");
            systemWasDisabled = true;
        }
        return;
    }
    
    // If in LOGGING state - let it complete first
    if (currentState == STATE_LOGGING) {
        // Don't interrupt logging - let update() handle it normally
        // It will transition to IDLE after LOGGING_TIME
        return;
    }
    
    // Active cycle in progress - need to interrupt safely
    LOG_WARNING("====================================");
    LOG_WARNING("SYSTEM DISABLE - INTERRUPTING CYCLE");
    LOG_WARNING("====================================");
    LOG_WARNING("Current state: %s", getStateString());
    
    // Stop pump if active
    if (isPumpActive()) {
        LOG_WARNING("Stopping active pump");
        stopPump();
    }
    
    // Log partial cycle data to FRAM and VPS
    if (currentState != STATE_IDLE && !cycleLogged) {
        LOG_INFO("Logging interrupted cycle data");
        
        // Mark as interrupted in cycle data
        currentCycle.error_code = ERROR_NONE;  // Not an error, just interrupted
        
        // Calculate actual volume if pump ran
        uint16_t actualVolumeML = 0;
        if (pumpStartTime > 0 && currentCycle.pump_duration > 0) {
            // Pump was running - estimate delivered volume
            uint32_t pumpedSeconds = getCurrentTimeSeconds() - pumpStartTime;
            if (pumpedSeconds > currentCycle.pump_duration) {
                pumpedSeconds = currentCycle.pump_duration;
            }
            actualVolumeML = (uint16_t)(pumpedSeconds * currentPumpSettings.volumePerSecond);
        }
        currentCycle.volume_dose = actualVolumeML;
        
        // Add to daily volume
        framBusy = true;
        if (actualVolumeML > 0) {
            dailyVolumeML += actualVolumeML;
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
            LOG_INFO("Partial volume added: %dml, daily total: %dml", actualVolumeML, dailyVolumeML);
        }

        // Save to FRAM
        saveCycleToStorage(currentCycle);
        framBusy = false;
    }
    
    // Reset to IDLE
    currentState = STATE_IDLE;
    resetCycle();
    systemWasDisabled = true;
    
    LOG_WARNING("Cycle interrupted - returned to IDLE");
    LOG_WARNING("====================================");
}

void WaterAlgorithm::update() {
    checkResetButton();
    updateErrorSignal();
    
    // ============== SYSTEM DISABLE CHECK (PRIORITY) ==============
    // Check at the very beginning - before any other processing
    if (isSystemDisabled()) {
        handleSystemDisable();
        return;  // Skip all algorithm processing when disabled
    }
    
    // ============== SYSTEM RE-ENABLE CHECK ==============
    // If system was disabled and is now enabled, check sensors
    if (systemWasDisabled) {
        LOG_INFO("====================================");
        LOG_INFO("SYSTEM RE-ENABLED - SENSOR CHECK");
        LOG_INFO("====================================");
        
        // Clear the flag first
        systemWasDisabled = false;
        
        // Check if sensors are currently active
        bool sensor1 = readWaterSensor1();
        bool sensor2 = readWaterSensor2();
        
        LOG_INFO("Sensor 1: %s", sensor1 ? "ACTIVE" : "inactive");
        LOG_INFO("Sensor 2: %s", sensor2 ? "ACTIVE" : "inactive");
        
        if (sensor1 || sensor2) {
            // Sensors active - let debounce process handle it naturally
            LOG_INFO("Sensors active - debounce process will start");
            LOG_INFO("====================================");
            // checkWaterSensors() w main loop wykryje aktywne czujniki i uruchomi debouncing
        } else {
            LOG_INFO("Sensors inactive - waiting for trigger");
            LOG_INFO("====================================");
        }
    }
    
    // UTC day check - throttled to 1x/second
    static uint32_t lastDateCheck = 0;
    
    if (millis() - lastDateCheck >= 1000) {
        // uint32_t currentTime = getCurrentTimeSeconds();
        lastDateCheck = millis();
        
        if (!isRTCWorking()) {
            static uint32_t lastWarning = 0;
            if (millis() - lastWarning > 30000) {
                LOG_ERROR("RTC not working - skipping date check");
                lastWarning = millis();
            }
            goto skip_date_check;
        }
        
        uint32_t currentUTCDay = getUnixTimestamp() / 86400;
        
        // ✅ SANITY CHECK: Sprawdź czy UTC day jest sensowny (2024-2035)
        // 2024-01-01 = 19723 days, 2035-12-31 = 24106 days
        if (currentUTCDay < 19723 || currentUTCDay > 24106) {
            static uint32_t lastInvalidWarning = 0;
            if (millis() - lastInvalidWarning > 10000) {
                LOG_ERROR("Invalid UTC day from RTC: %lu (expected 19723-24106)", currentUTCDay);
                LOG_ERROR("Skipping date check - RTC data corrupted");
                lastInvalidWarning = millis();
            }
            goto skip_date_check;
        }
        
        // ✅ DATE REGRESSION PROTECTION: Jeśli nowy < stary, ignoruj (RTC error)
        if (currentUTCDay < lastResetUTCDay) {
            static uint32_t lastRegressionWarning = 0;
            if (millis() - lastRegressionWarning > 10000) {
                LOG_ERROR("DATE REGRESSION DETECTED - IGNORING!");
                LOG_ERROR("Current UTC day: %lu, Last: %lu (diff: %ld days BACK)", 
                         currentUTCDay, lastResetUTCDay, 
                         (long)(lastResetUTCDay - currentUTCDay));
                LOG_ERROR("This indicates RTC read error - skipping reset");
                lastRegressionWarning = millis();
            }
            goto skip_date_check;
        }
        
        if (currentUTCDay != lastResetUTCDay) {
            LOG_WARNING("===========================================");
            LOG_WARNING("UTC DAY CHANGE DETECTED - RESET TRIGGERED!");
            LOG_WARNING("===========================================");
            LOG_WARNING("Previous UTC day: %lu", lastResetUTCDay);
            LOG_WARNING("Current UTC day:  %lu", currentUTCDay);
            LOG_WARNING("Difference: +%lu days", currentUTCDay - lastResetUTCDay);
            LOG_WARNING("Daily volume BEFORE: %dml", dailyVolumeML);
            LOG_WARNING("===========================================");
            
            if (isPumpActive()) {
                if (!resetPending) {
                    LOG_INFO("Reset delayed - pump active");
                    resetPending = true;
                }
            } else {
                dailyVolumeML = 0;
                todayCycles.clear();
                lastResetUTCDay = currentUTCDay;
                saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
                resetPending = false;
                
                LOG_WARNING("RESET EXECUTED: new UTC day = %lu", lastResetUTCDay);
                LOG_WARNING("===========================================");
            }
        }
    }
       
    skip_date_check:
    uint32_t currentTime = getCurrentTimeSeconds();
    
    if (resetPending && !isPumpActive() && currentState == STATE_IDLE) {
        LOG_INFO("Executing delayed reset (pump finished)");
        
        uint32_t currentUTCDay = getUnixTimestamp() / 86400;
        dailyVolumeML = 0;
        todayCycles.clear();
        lastResetUTCDay = currentUTCDay;
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
        resetPending = false;
        
        LOG_INFO("Delayed reset complete: 0ml (UTC day: %lu)", lastResetUTCDay);
    }
    
    uint32_t stateElapsed = currentTime - stateStartTime;

    switch (currentState) {
        case STATE_IDLE:
            // Oczekiwanie na pierwszy LOW - obsługiwane przez water_sensors.cpp
            break;

        case STATE_PRE_QUALIFICATION:
            // Pre-qualification - obsługiwane przez water_sensors.cpp
            // Callbacki: onPreQualificationSuccess() lub onPreQualificationFail()
            break;

        case STATE_SETTLING:
            // Settling (uspokojenie wody) - obsługiwane przez water_sensors.cpp
            // Callback: onSettlingComplete()
            break;

        case STATE_DEBOUNCING:
            // Debouncing - obsługiwane przez water_sensors.cpp
            // Callbacki: onDebounceBothComplete() lub onDebounceTimeout()
            break;

        // ============== FAZA 2: POMPOWANIE + RELEASE VERIFICATION ==============
        case STATE_PUMPING_AND_VERIFY: {
            // Aktualizuj release debounce (co 2s sprawdzaj czujniki)
            updateReleaseDebounce();

            // Status log co 10s
            static uint32_t lastStatusLog = 0;
            if (currentTime - lastStatusLog >= 10) {
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                LOG_INFO("PUMPING_AND_VERIFY: %ds/%ds, pump=%s, S1=%d/%d, S2=%d/%d",
                        timeSincePumpStart, WATER_TRIGGER_MAX_TIME,
                        isPumpActive() ? "ON" : "OFF",
                        releaseDebounce[0].counter, releaseDebounce[0].confirmed ? 1 : 0,
                        releaseDebounce[1].counter, releaseDebounce[1].confirmed ? 1 : 0);
                lastStatusLog = currentTime;
            }

            // Sprawdź warunki zakończenia
            bool pumpFinished = !isPumpActive();
            bool allConfirmed = checkAllReleaseConfirmed();

            // SUKCES: Pompa skończyła + wszystkie wymagane potwierdzone
            if (pumpFinished && allConfirmed) {
                LOG_INFO("====================================");
                LOG_INFO("SUCCESS: Pump finished + all sensors confirmed");
                LOG_INFO("====================================");
                calculateWaterTrigger();
                calculateTimeGap2();
                currentState = STATE_LOGGING;
                stateStartTime = currentTime;
                break;
            }

            // TIMEOUT: 240s od startu pompy
            uint32_t timeSincePumpStart = currentTime - pumpStartTime;
            if (timeSincePumpStart >= WATER_TRIGGER_MAX_TIME) {
                handleReleaseTimeout();
            }
            break;
        }

        case STATE_LOGGING:
            if(permission_log){
                LOG_INFO("==================case STATE_LOGGING");
                permission_log = false;      
            } 
        
            if (!cycleLogged) {
                logCycleComplete();
                cycleLogged = true;
                
                if (dailyVolumeML > fillWaterMaxConfig) {
                    LOG_ERROR("Daily limit exceeded! %dml > %dml", dailyVolumeML, fillWaterMaxConfig);
                    currentCycle.error_code = ERROR_DAILY_LIMIT;
                    startErrorSignal(ERROR_DAILY_LIMIT);
                    currentState = STATE_ERROR;
                    break;
                }
            }
            
            if (stateElapsed >= LOGGING_TIME) { 
                LOG_INFO("Cycle complete, returning to IDLE######################################################");
                currentState = STATE_IDLE;
                resetCycle();
            }
            break;
    }
}
   

void WaterAlgorithm::initFromFRAM() {
    LOG_INFO("Loading cycle history and error stats from FRAM...");
    loadCyclesFromStorage();

    ErrorStats stats;
    if (loadErrorStatsFromFRAM(stats)) {
        LOG_INFO("Error statistics loaded from FRAM");
    } else {
        LOG_WARNING("Could not load error stats from FRAM");
    }
}

void WaterAlgorithm::initDailyVolume() {
    LOG_INFO("====================================");
    LOG_INFO("INITIALIZING DAILY VOLUME");
    LOG_INFO("====================================");
    
    if (!isRTCWorking()) {
        LOG_ERROR("RTC not working at initDailyVolume!");
        dailyVolumeML = 0;
        lastResetUTCDay = 0;
        LOG_ERROR("Daily volume tracking SUSPENDED until RTC ready");
        return;
    }
    
    uint32_t currentUTCDay = getUnixTimestamp() / 86400;
    LOG_INFO("Current UTC day: %lu", currentUTCDay);
    
    // Load from FRAM
    uint32_t loadedUTCDay = 0;
    uint16_t loadedVolume = 0;
    
    if (loadDailyVolumeFromFRAM(loadedVolume, loadedUTCDay)) {
        LOG_INFO("FRAM data: %dml, UTC day=%lu", loadedVolume, loadedUTCDay);
        LOG_INFO("Current UTC day: %lu", currentUTCDay);
        
        if (currentUTCDay == loadedUTCDay) {
            // Same day - restore volume
            dailyVolumeML = loadedVolume;
            lastResetUTCDay = loadedUTCDay;
            LOG_INFO("✅ Same day - restored: %dml", dailyVolumeML);
        } else {
            // Different day - reset
            LOG_INFO("🔄 Day changed from %lu to %lu", loadedUTCDay, currentUTCDay);
            dailyVolumeML = 0;
            lastResetUTCDay = currentUTCDay;
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
            LOG_INFO("✅ New day - reset to 0ml");
        }
    } else {
        // No valid data in FRAM
        LOG_INFO("No valid FRAM data - initializing");
        dailyVolumeML = 0;
        lastResetUTCDay = currentUTCDay;
        saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
        LOG_INFO("✅ Initialized to 0ml");
    }

    // 🆕 NEW: Load Available Volume from FRAM
    uint32_t loadedMax, loadedCurrent;
    if (loadAvailableVolumeFromFRAM(loadedMax, loadedCurrent)) {
        availableVolumeMax = loadedMax;
        availableVolumeCurrent = loadedCurrent;
        LOG_INFO("Available volume restored: %lu/%lu ml", availableVolumeCurrent, availableVolumeMax);
    } else {
        // First run - save defaults
        saveAvailableVolumeToFRAM(availableVolumeMax, availableVolumeCurrent);
        LOG_INFO("Available volume initialized with defaults: %lu ml", availableVolumeMax);
    }
    
    // 🆕 NEW: Load Fill Water Max from FRAM
    uint16_t loadedFillMax;
    if (loadFillWaterMaxFromFRAM(loadedFillMax)) {
        fillWaterMaxConfig = loadedFillMax;
        LOG_INFO("Fill water max restored: %d ml", fillWaterMaxConfig);
    } else {
        // First run - save default
        saveFillWaterMaxToFRAM(fillWaterMaxConfig);
        LOG_INFO("Fill water max initialized with default: %d ml", fillWaterMaxConfig);
    }
    
    LOG_INFO("INIT COMPLETE:");
    LOG_INFO("  dailyVolumeML: %dml", dailyVolumeML);
    LOG_INFO("  lastResetUTCDay: %lu", lastResetUTCDay);
    LOG_INFO("====================================");
}

// ============== CALLBACKI FAZY 1 (Pre-qual + Settling + Debouncing) ==============

void WaterAlgorithm::onPreQualificationStart() {
    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Pre-qualification started");
    LOG_INFO("====================================");

    // Rozpocznij cykl - przejdź do stanu PRE_QUALIFICATION
    if (currentState == STATE_IDLE) {
        uint32_t currentTime = getCurrentTimeSeconds();
        triggerStartTime = currentTime;
        currentCycle.trigger_time = currentTime;
        currentCycle.timestamp = getUnixTimestamp();
        currentState = STATE_PRE_QUALIFICATION;
        stateStartTime = currentTime;

        // Reset czasów zaliczenia
        sensor1DebounceCompleteTime = 0;
        sensor2DebounceCompleteTime = 0;
        debouncePhaseActive = false;

        LOG_INFO("State changed: IDLE -> PRE_QUALIFICATION");
    }
}

void WaterAlgorithm::onPreQualificationSuccess() {
    // Safety check - only proceed if in expected state
    if (currentState != STATE_PRE_QUALIFICATION) {
        LOG_WARNING("onPreQualificationSuccess ignored - state is %s, not PRE_QUALIFICATION",
                    getStateString());
        return;
    }

    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Pre-qualification SUCCESS");
    LOG_INFO("====================================");

    uint32_t currentTime = getCurrentTimeSeconds();
    currentState = STATE_SETTLING;
    stateStartTime = currentTime;

    LOG_INFO("State changed: PRE_QUALIFICATION -> SETTLING (%ds)", SETTLING_TIME);
}

void WaterAlgorithm::onPreQualificationFail() {
    // Safety check - only proceed if in expected state
    if (currentState != STATE_PRE_QUALIFICATION) {
        LOG_WARNING("onPreQualificationFail ignored - state is %s, not PRE_QUALIFICATION",
                    getStateString());
        return;
    }

    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Pre-qualification FAIL (silent reset)");
    LOG_INFO("====================================");

    // Cichy powrót do IDLE - bez błędu
    currentState = STATE_IDLE;
    resetCycle();

    LOG_INFO("State changed: PRE_QUALIFICATION -> IDLE (no error)");
}

void WaterAlgorithm::onSettlingComplete() {
    // Safety check - only proceed if in expected state
    if (currentState != STATE_SETTLING) {
        LOG_WARNING("onSettlingComplete ignored - state is %s, not SETTLING",
                    getStateString());
        return;
    }

    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Settling complete");
    LOG_INFO("====================================");

    uint32_t currentTime = getCurrentTimeSeconds();
    currentState = STATE_DEBOUNCING;
    stateStartTime = currentTime;
    debouncePhaseActive = true;

    // Reset czasów zaliczenia dla fazy debouncing
    sensor1DebounceCompleteTime = 0;
    sensor2DebounceCompleteTime = 0;

    LOG_INFO("State changed: SETTLING -> DEBOUNCING (%ds timeout)", TOTAL_DEBOUNCE_TIME);
}

void WaterAlgorithm::onSensorDebounceComplete(uint8_t sensorNum) {
    uint32_t currentTime = getCurrentTimeSeconds();
    
    LOG_INFO("ALGORITHM: Sensor %d debounce complete at %lu", sensorNum, currentTime);
    
    if (sensorNum == 1) {
        sensor1DebounceCompleteTime = currentTime;
    } else if (sensorNum == 2) {
        sensor2DebounceCompleteTime = currentTime;
    }
}

void WaterAlgorithm::onDebounceBothComplete() {
    // Safety check - only proceed if in expected state
    if (currentState != STATE_DEBOUNCING) {
        LOG_WARNING("onDebounceBothComplete ignored - state is %s, not DEBOUNCING",
                    getStateString());
        return;
    }

    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Both sensors debounce OK");
    LOG_INFO("====================================");

    uint32_t currentTime = getCurrentTimeSeconds();
    debouncePhaseActive = false;

    // Oblicz time_gap_1 jako różnicę między zaliczeniami
    if (sensor1DebounceCompleteTime > 0 && sensor2DebounceCompleteTime > 0) {
        currentCycle.time_gap_1 = abs((int32_t)sensor2DebounceCompleteTime -
                                      (int32_t)sensor1DebounceCompleteTime);
        LOG_INFO("TIME_GAP_1 (debounce diff): %lu seconds", currentCycle.time_gap_1);
    } else {
        currentCycle.time_gap_1 = 0;
    }

    // ============== USTAW KONTEKST DLA FAZY 2 ==============
    // Oba czujniki zaliczyły - oba wymagane w release verification
    sensor1TriggeredCycle = true;
    sensor2TriggeredCycle = true;
    resetReleaseDebounce();

    LOG_INFO("Release context: S1=required, S2=required");

    // Sukces - nie ustawiamy flagi błędu GAP1
    // Przechodzimy do uruchomienia pompy + release verification
    LOG_INFO("Starting pump + release verification");

    currentState = STATE_PUMPING_AND_VERIFY;
    stateStartTime = currentTime;
    pumpStartTime = currentTime;
    pumpAttempts = 1;

    uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
    if (!validatePumpWorkTime(pumpWorkTime)) {
        LOG_ERROR("PUMP_WORK_TIME (%ds) exceeds WATER_TRIGGER_MAX_TIME (%ds)",
                pumpWorkTime, WATER_TRIGGER_MAX_TIME);
        pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;
    }

    triggerPump(pumpWorkTime, "AUTO_PUMP");
    currentCycle.pump_duration = pumpWorkTime;

    LOG_INFO("Pump started for %d seconds", pumpWorkTime);
}

void WaterAlgorithm::onDebounceTimeout(bool sensor1OK, bool sensor2OK) {
    // Safety check - only proceed if in expected state
    if (currentState != STATE_DEBOUNCING) {
        LOG_WARNING("onDebounceTimeout ignored - state is %s, not DEBOUNCING",
                    getStateString());
        return;
    }

    LOG_INFO("====================================");
    LOG_INFO("ALGORITHM: Debounce timeout");
    LOG_INFO("====================================");
    LOG_INFO("Sensor1: %s, Sensor2: %s",
             sensor1OK ? "OK" : "FAIL",
             sensor2OK ? "OK" : "FAIL");

    uint32_t currentTime = getCurrentTimeSeconds();
    debouncePhaseActive = false;

    if (sensor1OK || sensor2OK) {
        // Przynajmniej jeden czujnik OK - uruchamiamy pompę ale z błędem
        LOG_WARNING("Only one sensor OK - pump will start with GAP1_FAIL flag");

        // Oblicz time_gap_1
        if (sensor1DebounceCompleteTime > 0 && sensor2DebounceCompleteTime > 0) {
            currentCycle.time_gap_1 = abs((int32_t)sensor2DebounceCompleteTime -
                                          (int32_t)sensor1DebounceCompleteTime);
        } else {
            currentCycle.time_gap_1 = TIME_GAP_1_MAX;  // Timeout value
        }

        // Ustaw flagi bledu
        currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
        if (!sensor1OK) {
            currentCycle.sensor_results |= PumpCycle::RESULT_SENSOR1_DEBOUNCE_FAIL;
        }
        if (!sensor2OK) {
            currentCycle.sensor_results |= PumpCycle::RESULT_SENSOR2_DEBOUNCE_FAIL;
        }

        // ============== USTAW KONTEKST DLA FAZY 2 ==============
        // Tylko te czujniki które zaliczyły będą wymagane w release verification
        sensor1TriggeredCycle = sensor1OK;
        sensor2TriggeredCycle = sensor2OK;
        resetReleaseDebounce();

        LOG_INFO("Release context: S1=%s, S2=%s",
                 sensor1OK ? "required" : "NOT required",
                 sensor2OK ? "required" : "NOT required");

        // Uruchom pompę + release verification
        currentState = STATE_PUMPING_AND_VERIFY;
        stateStartTime = currentTime;
        pumpStartTime = currentTime;
        pumpAttempts = 1;

        uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
        if (!validatePumpWorkTime(pumpWorkTime)) {
            pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;
        }

        triggerPump(pumpWorkTime, "AUTO_PUMP");
        currentCycle.pump_duration = pumpWorkTime;

        LOG_INFO("Pump started for %d seconds (with GAP1_FAIL)", pumpWorkTime);

    } else {
        // Żaden czujnik nie zaliczył - ERR_FALSE_TRIGGER
        LOG_ERROR("====================================");
        LOG_ERROR("ERR_FALSE_TRIGGER: No sensor passed debounce");
        LOG_ERROR("====================================");
        LOG_ERROR("Pre-qualification passed, but debounce failed for both sensors");
        LOG_ERROR("Possible causes: snail, temporary blockage, sensor noise");

        // Ustaw flagę błędu FALSE_TRIGGER
        currentCycle.sensor_results |= PumpCycle::RESULT_FALSE_TRIGGER;

        // Loguj cykl z błędem
        currentCycle.time_gap_1 = TOTAL_DEBOUNCE_TIME;
        currentCycle.error_code = ERROR_NONE;  // Nie jest to krytyczny błąd
        logCycleComplete();

        currentState = STATE_IDLE;
        resetCycle();

        LOG_INFO("Returned to IDLE with FALSE_TRIGGER flag");
    }
}

void WaterAlgorithm::calculateTimeGap2() {
    // ============== NOWA LOGIKA: używamy czasów z release debounce ==============
    if (releaseDebounce[0].confirmed && releaseDebounce[1].confirmed) {
        // Oba potwierdzone - oblicz różnicę
        currentCycle.time_gap_2 = abs((int32_t)releaseDebounce[0].confirmTime -
                                      (int32_t)releaseDebounce[1].confirmTime);

        LOG_INFO("TIME_GAP_2: %ds (S1@%lus, S2@%lus)",
                currentCycle.time_gap_2,
                releaseDebounce[0].confirmTime,
                releaseDebounce[1].confirmTime);
    } else {
        // Tylko jeden czujnik potwierdzony lub żaden - brak gap2
        currentCycle.time_gap_2 = 0;
        LOG_INFO("TIME_GAP_2: 0 (only one sensor confirmed or none)");
    }
}

void WaterAlgorithm::calculateWaterTrigger() {
    // ============== NOWA LOGIKA: używamy czasów z release debounce ==============
    uint32_t earliestConfirm = UINT32_MAX;

    if (releaseDebounce[0].confirmed && releaseDebounce[0].confirmTime > pumpStartTime) {
        earliestConfirm = releaseDebounce[0].confirmTime;
    }
    if (releaseDebounce[1].confirmed && releaseDebounce[1].confirmTime > pumpStartTime) {
        if (releaseDebounce[1].confirmTime < earliestConfirm) {
            earliestConfirm = releaseDebounce[1].confirmTime;
        }
    }

    if (earliestConfirm != UINT32_MAX) {
        currentCycle.water_trigger_time = earliestConfirm - pumpStartTime;

        // Sanity check
        if (currentCycle.water_trigger_time > WATER_TRIGGER_MAX_TIME) {
            currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        }

        LOG_INFO("WATER_TRIGGER_TIME: %ds (from release debounce)",
                currentCycle.water_trigger_time);
    } else {
        // No valid release detected - timeout
        currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        LOG_WARNING("No sensor release confirmed - water_trigger_time = MAX");
    }
}

// ============== RELEASE VERIFICATION METHODS ==============

void WaterAlgorithm::resetReleaseDebounce() {
    for (int i = 0; i < 2; i++) {
        releaseDebounce[i].counter = 0;
        releaseDebounce[i].confirmed = false;
        releaseDebounce[i].confirmTime = 0;
    }
    lastReleaseCheck = 0;
}

void WaterAlgorithm::updateReleaseDebounce() {
    uint32_t currentTime = getCurrentTimeSeconds();

    // Sprawdzaj co RELEASE_CHECK_INTERVAL (2s)
    if (currentTime - lastReleaseCheck < RELEASE_CHECK_INTERVAL) {
        return;
    }
    lastReleaseCheck = currentTime;

    // Odczytaj czujniki (HIGH = woda podniesiona = !readWaterSensorX())
    bool sensor1High = !readWaterSensor1();
    bool sensor2High = !readWaterSensor2();

    // Aktualizuj release debounce dla S1 (jeśli wymagany)
    if (sensor1TriggeredCycle && !releaseDebounce[0].confirmed) {
        if (sensor1High) {
            releaseDebounce[0].counter++;
            if (releaseDebounce[0].counter >= RELEASE_DEBOUNCE_COUNT) {
                releaseDebounce[0].confirmed = true;
                releaseDebounce[0].confirmTime = currentTime;
                LOG_INFO("Sensor1 release CONFIRMED at %lus (3x HIGH)", currentTime);
            }
        } else {
            if (releaseDebounce[0].counter > 0) {
                LOG_INFO("Sensor1 release reset (was %d)", releaseDebounce[0].counter);
            }
            releaseDebounce[0].counter = 0;
        }
    }

    // Aktualizuj release debounce dla S2 (jeśli wymagany)
    if (sensor2TriggeredCycle && !releaseDebounce[1].confirmed) {
        if (sensor2High) {
            releaseDebounce[1].counter++;
            if (releaseDebounce[1].counter >= RELEASE_DEBOUNCE_COUNT) {
                releaseDebounce[1].confirmed = true;
                releaseDebounce[1].confirmTime = currentTime;
                LOG_INFO("Sensor2 release CONFIRMED at %lus (3x HIGH)", currentTime);
            }
        } else {
            if (releaseDebounce[1].counter > 0) {
                LOG_INFO("Sensor2 release reset (was %d)", releaseDebounce[1].counter);
            }
            releaseDebounce[1].counter = 0;
        }
    }
}

bool WaterAlgorithm::checkAllReleaseConfirmed() {
    // Sprawdź czy wszystkie WYMAGANE czujniki potwierdziły
    if (sensor1TriggeredCycle && !releaseDebounce[0].confirmed) {
        return false;
    }
    if (sensor2TriggeredCycle && !releaseDebounce[1].confirmed) {
        return false;
    }
    return true;
}

void WaterAlgorithm::handleReleaseTimeout() {
    LOG_WARNING("====================================");
    LOG_WARNING("RELEASE VERIFICATION TIMEOUT (240s)");
    LOG_WARNING("====================================");

    // Zatrzymaj pompę jeśli jeszcze pracuje
    if (isPumpActive()) {
        stopPump();
        LOG_WARNING("Pump stopped due to timeout");
    }

    bool s1Required = sensor1TriggeredCycle;
    bool s2Required = sensor2TriggeredCycle;
    bool s1OK = releaseDebounce[0].confirmed;
    bool s2OK = releaseDebounce[1].confirmed;

    LOG_INFO("S1: required=%d, confirmed=%d, counter=%d", s1Required, s1OK, releaseDebounce[0].counter);
    LOG_INFO("S2: required=%d, confirmed=%d, counter=%d", s2Required, s2OK, releaseDebounce[1].counter);

    // Przypadek 1: Przynajmniej jeden wymagany potwierdził
    if ((s1Required && s1OK) || (s2Required && s2OK)) {
        // Sukces częściowy - loguj błąd dla niepotwierdzonego
        if (s1Required && !s1OK) {
            currentCycle.sensor_results |= PumpCycle::RESULT_SENSOR1_RELEASE_FAIL;
            LOG_ERROR("ERR_SENSOR1_RELEASE: S1 did not confirm");
        }
        if (s2Required && !s2OK) {
            currentCycle.sensor_results |= PumpCycle::RESULT_SENSOR2_RELEASE_FAIL;
            LOG_ERROR("ERR_SENSOR2_RELEASE: S2 did not confirm");
        }

        calculateWaterTrigger();
        calculateTimeGap2();
        currentState = STATE_LOGGING;
        stateStartTime = getCurrentTimeSeconds();
        return;
    }

    // Przypadek 2: Żaden wymagany nie potwierdził = ERR_NO_WATER
    currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
    waterFailDetected = true;
    LOG_ERROR("ERR_NO_WATER: No sensor confirmed water delivery");

    if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
        // Retry
        pumpAttempts++;
        LOG_WARNING("Retrying pump, attempt %d/%d", pumpAttempts, PUMP_MAX_ATTEMPTS);

        // Reset release debounce dla nowej próby
        resetReleaseDebounce();

        // Uruchom pompę ponownie
        uint32_t currentTime = getCurrentTimeSeconds();
        pumpStartTime = currentTime;

        uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
        if (!validatePumpWorkTime(pumpWorkTime)) {
            pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;
        }

        triggerPump(pumpWorkTime, "AUTO_PUMP_RETRY");
        currentCycle.pump_duration = pumpWorkTime;

        // Pozostajemy w STATE_PUMPING_AND_VERIFY
        stateStartTime = currentTime;
    } else {
        // Wszystkie próby wyczerpane
        LOG_ERROR("All %d pump attempts failed!", PUMP_MAX_ATTEMPTS);
        currentCycle.error_code = ERROR_PUMP_FAILURE;

        LOG_INFO("Logging failed cycle before entering ERROR state");
        logCycleComplete();

        startErrorSignal(ERROR_PUMP_FAILURE);
        currentState = STATE_ERROR;
    }
}

void WaterAlgorithm::logCycleComplete() {
    // SPRAWDZENIE: czy currentCycle zostało gdzieś wyzerowane
    if (currentCycle.time_gap_1 == 0 && sensor1TriggerTime == 0 && sensor2TriggerTime == 0) {
        LOG_ERROR("CRITICAL: currentCycle.time_gap_1 was RESET! Reconstructing...");
        
        if (triggerStartTime > 0) {
            LOG_INFO("Attempting to reconstruct TIME_GAP_1 from available data");
        }
    }

    // Calculate volume based on actual pump duration
    // uint16_t actualVolumeML = (uint16_t)(currentCycle.pump_duration * currentPumpSettings.volumePerSecond);
    // currentCycle.volume_dose = actualVolumeML;

        uint16_t actualVolumeML;
    
    if (currentCycle.error_code == ERROR_PUMP_FAILURE) {
        // All pump attempts failed - NO water delivered
        actualVolumeML = 0;
        
        LOG_ERROR("====================================");
        LOG_ERROR("PUMP FAILURE - NO WATER DELIVERED");
        LOG_ERROR("====================================");
        LOG_ERROR("Total attempts: %d", pumpAttempts);
        LOG_ERROR("All attempts timed out (no sensor confirmation)");
        LOG_ERROR("Volume counted: 0ml (no confirmed delivery)");
        LOG_ERROR("Possible causes:");
        LOG_ERROR("  - Pump malfunction");
        LOG_ERROR("  - Tube blockage");
        LOG_ERROR("  - Water source empty");
        LOG_ERROR("  - Sensor malfunction");
        LOG_ERROR("====================================");
        
    } else {
        // Manual cycle - water confirmed by sensors
        actualVolumeML = (uint16_t)(currentCycle.pump_duration * 
                                     currentPumpSettings.volumePerSecond);
        
        LOG_INFO("Water delivery confirmed by sensors");
        LOG_INFO("Volume delivered: %dml", actualVolumeML);
    }


    currentCycle.volume_dose = actualVolumeML;
    currentCycle.pump_attempts = pumpAttempts;

    // SET final fail flag based on any failure
    if (waterFailDetected) {
        currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        LOG_INFO("Final WATER fail flag set due to timeout in any attempt");
    }
    
    // Add to daily volume (use actual volume, not fixed SINGLE_DOSE_VOLUME)
    dailyVolumeML += actualVolumeML;

    // --- FRAM write section (block HTTP reads during writes) ---
    framBusy = true;

    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay)) {
        LOG_WARNING("Failed to save daily volume to FRAM");
    }

    if (availableVolumeCurrent >= actualVolumeML) {
        availableVolumeCurrent -= actualVolumeML;
    } else {
        availableVolumeCurrent = 0;
    }

    if (!saveAvailableVolumeToFRAM(availableVolumeMax, availableVolumeCurrent)) {
        LOG_WARNING("Failed to save available volume to FRAM");
    }

    // Store in today's cycles (RAM)
    todayCycles.push_back(currentCycle);

    // Keep only last 50 cycles in RAM
    if (todayCycles.size() > 50) {
        todayCycles.erase(todayCycles.begin());
    }

    // Save cycle to FRAM (for debugging and history)
    saveCycleToStorage(currentCycle);

    // *** Update error statistics in FRAM ***
    uint8_t gap1_increment = (currentCycle.sensor_results & PumpCycle::RESULT_GAP1_FAIL) ? 1 : 0;
    uint8_t gap2_increment = (currentCycle.sensor_results & PumpCycle::RESULT_GAP2_FAIL) ? 1 : 0;
    uint8_t water_increment = (currentCycle.sensor_results & PumpCycle::RESULT_WATER_FAIL) ? 1 : 0;

    if (gap1_increment || gap2_increment || water_increment) {
        if (incrementErrorStats(gap1_increment, gap2_increment, water_increment)) {
            LOG_INFO("Error stats updated: GAP1+%d, GAP2+%d, WATER+%d",
                    gap1_increment, gap2_increment, water_increment);
        } else {
            LOG_WARNING("Failed to update error stats in FRAM");
        }
    }

    framBusy = false;
    // --- End FRAM write section ---
    

    uint32_t unixTime = getUnixTimestamp();
    
    // if (logCycleToVPS(currentCycle, unixTime)) {
    //     LOG_INFO("Cycle data sent to VPS successfully");
    // } else {
    //     LOG_WARNING("Failed to send cycle data to VPS");
    // }
    
    LOG_INFO("=== CYCLE COMPLETE ===");
    LOG_INFO("Actual volume: %dml (pump_duration: %ds)", actualVolumeML, currentCycle.pump_duration);
    LOG_INFO("TIME_GAP_1: %ds (fail=%d)", currentCycle.time_gap_1, gap1_increment);
    LOG_INFO("TIME_GAP_2: %ds (fail=%d)", currentCycle.time_gap_2, gap2_increment);
    LOG_INFO("WATER_TRIGGER_TIME: %ds (fail=%d)", currentCycle.water_trigger_time, water_increment);
    LOG_INFO("Daily: %dml/%dml | Available: %lu/%lu ml", 
             dailyVolumeML, fillWaterMaxConfig, availableVolumeCurrent, availableVolumeMax);
}

bool WaterAlgorithm::requestManualPump(uint16_t duration_ms) {

    // ============== BLOCK MANUAL PUMP WHEN SYSTEM DISABLED ==============
    if (isSystemDisabled()) {
        LOG_WARNING("❌ Manual pump blocked: System is disabled");
        return false;
    }

    if (dailyVolumeML >= fillWaterMaxConfig) {
        LOG_ERROR("❌ Manual pump blocked: Daily limit reached (%dml / %dml)", 
                  dailyVolumeML, fillWaterMaxConfig);
        return false;  // Block manual pump when limit reached
    }

    if (currentState == STATE_ERROR) {
        LOG_WARNING("Cannot start manual pump in error state");
        return false;
    }
    
    // SPRAWDŹ czy to AUTO_PUMP podczas automatycznego cyklu
    if (currentState != STATE_IDLE) {
        // Jeśli jesteśmy w fazie pompowania, nie resetuj danych!
        if (currentState == STATE_PUMPING_AND_VERIFY) {
            LOG_INFO("AUTO_PUMP during automatic cycle - preserving cycle data");
            // NIE wywołuj resetCycle() - zachowaj zebrane dane!
            return true; // Pozwól na pompę, ale nie resetuj
        } else {
            // Manual interrupt w innych stanach
            LOG_INFO("Manual pump interrupting current cycle");
            currentState = STATE_MANUAL_OVERRIDE;
            resetCycle();
        }
    }
    
    return true;
}

void WaterAlgorithm::onManualPumpComplete() {
    if (currentState == STATE_MANUAL_OVERRIDE) {
        LOG_INFO("Manual pump complete, returning to IDLE");
        currentState = STATE_IDLE;
        resetCycle();
    }
}

const char* WaterAlgorithm::getStateString() const {
    switch (currentState) {
        case STATE_IDLE: return "IDLE";
        case STATE_PRE_QUALIFICATION: return "PRE_QUAL";
        case STATE_SETTLING: return "SETTLING";
        case STATE_DEBOUNCING: return "DEBOUNCING";
        case STATE_PUMPING_AND_VERIFY: return "PUMP+VERIFY";
        case STATE_LOGGING: return "LOGGING";
        case STATE_ERROR: return "ERROR";
        case STATE_MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        default: return "UNKNOWN";
    }
}

bool WaterAlgorithm::isInCycle() const {
    return currentState != STATE_IDLE && currentState != STATE_ERROR;
}

std::vector<PumpCycle> WaterAlgorithm::getRecentCycles(size_t count) {
    size_t start = todayCycles.size() > count ? todayCycles.size() - count : 0;
    return std::vector<PumpCycle>(todayCycles.begin() + start, todayCycles.end());
}

void WaterAlgorithm::startErrorSignal(ErrorCode error) {
    lastError = error;
    errorSignalActive = true;
    errorSignalStart = millis();
    errorPulseCount = 0;
    errorPulseState = false;
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    
    LOG_ERROR("Starting error signal: %s", 
             error == ERROR_DAILY_LIMIT ? "ERR1" :
             error == ERROR_PUMP_FAILURE ? "ERR2" : "ERR0");
}

void WaterAlgorithm::updateErrorSignal() {
    if (!errorSignalActive) return;
    
    uint32_t elapsed = millis() - errorSignalStart;
    uint8_t pulsesNeeded = (lastError == ERROR_DAILY_LIMIT) ? 1 :
                           (lastError == ERROR_PUMP_FAILURE) ? 2 : 3;
    
    // Calculate current position in signal pattern
    uint32_t cycleTime = 0;
    for (uint8_t i = 0; i < pulsesNeeded; i++) {
        cycleTime += ERROR_PULSE_HIGH + ERROR_PULSE_LOW;
    }
    cycleTime += ERROR_PAUSE - ERROR_PULSE_LOW; // Remove last LOW, add PAUSE
    
    uint32_t posInCycle = elapsed % cycleTime;
    
    // Determine if we should be HIGH or LOW
    bool shouldBeHigh = false;
    uint32_t currentPos = 0;
    
    for (uint8_t i = 0; i < pulsesNeeded; i++) {
        if (posInCycle >= currentPos && posInCycle < currentPos + ERROR_PULSE_HIGH) {
            shouldBeHigh = true;
            break;
        }
        currentPos += ERROR_PULSE_HIGH + ERROR_PULSE_LOW;
    }
    
    // Update pin state
    if (shouldBeHigh != errorPulseState) {
        errorPulseState = shouldBeHigh;
        pinMode(ERROR_SIGNAL_PIN, OUTPUT);
        digitalWrite(ERROR_SIGNAL_PIN, errorPulseState ? HIGH : LOW);
    }
}

void WaterAlgorithm::resetFromError() {
    lastError = ERROR_NONE;
    errorSignalActive = false;
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    currentState = STATE_IDLE;
    resetCycle();
    LOG_INFO("System reset from error state");
}

void WaterAlgorithm::loadCyclesFromStorage() {
    LOG_INFO("Loading cycles from FRAM...");

    framBusy = true;

    if (loadCyclesFromFRAM(framCycles, FRAM_MAX_CYCLES)) {
        framDataLoaded = true;
        LOG_INFO("Loaded %d cycles from FRAM", framCycles.size());
        LOG_INFO("Daily volume already loaded from FRAM: %dml", dailyVolumeML);

        // Load today's cycles for display
        uint32_t todayStart = (millis() / 1000) - (millis() / 1000) % 86400;

        for (const auto& cycle : framCycles) {
            if (cycle.timestamp >= todayStart) {
                todayCycles.push_back(cycle);
            }
        }

        LOG_INFO("Loaded %d cycles from today", todayCycles.size());
    } else {
        LOG_WARNING("Failed to load cycles from FRAM, starting fresh");
        framDataLoaded = false;
    }

    framBusy = false;
}

void WaterAlgorithm::saveCycleToStorage(const PumpCycle& cycle) {
    if (saveCycleToFRAM(cycle)) {
        LOG_INFO("Cycle saved to FRAM successfully");

        // Add to framCycles for immediate access
        framCycles.push_back(cycle);

        // Keep framCycles size matching FRAM ring buffer
        if (framCycles.size() > FRAM_MAX_CYCLES) {
            framCycles.erase(framCycles.begin());
        }
    } else {
        LOG_ERROR("Failed to save cycle to FRAM");
    }
}

bool WaterAlgorithm::resetErrorStatistics() {
    bool success = resetErrorStatsInFRAM();
    if (success) {
        LOG_INFO("Error statistics reset requested via web interface");
        
        // ✅ PRZYWRÓĆ VPS logging z short timeout (3 seconds max)
        // uint32_t unixTime = getUnixTimestamp();
        // String timestamp = getCurrentTimestamp();
        // bool vpsSuccess = logEventToVPS("STATISTICS_RESET", 0, unixTime);
        
        // if (vpsSuccess) {
        //     LOG_INFO("✅ Statistics reset + VPS logging: SUCCESS");
        // } else {
        //     LOG_WARNING("⚠️ Statistics reset: SUCCESS, VPS logging: FAILED (non-critical)");
        // }
    }
    return success;
}

bool WaterAlgorithm::getErrorStatistics(uint16_t& gap1_sum, uint16_t& gap2_sum, uint16_t& water_sum, uint32_t& last_reset) {
    ErrorStats stats;
    bool success = loadErrorStatsFromFRAM(stats);
    
    if (success) {
        gap1_sum = stats.gap1_fail_sum;
        gap2_sum = stats.gap2_fail_sum;
        water_sum = stats.water_fail_sum;
        last_reset = stats.last_reset_timestamp;
    } else {
        // Return defaults on failure
        gap1_sum = gap2_sum = water_sum = 0;
        last_reset = millis() / 1000;
    }
    
    return success;
}

void WaterAlgorithm::addManualVolume(uint16_t volumeML) {
    // 🆕 MODIFIED: Manual pump decreases ONLY available volume, NOT daily volume
    
    // Decrease available volume
    if (availableVolumeCurrent >= volumeML) {
        availableVolumeCurrent -= volumeML;
    } else {
        availableVolumeCurrent = 0;
    }
    
    // Save to FRAM
    if (!saveAvailableVolumeToFRAM(availableVolumeMax, availableVolumeCurrent)) {
        LOG_WARNING("Failed to save available volume to FRAM after manual pump");
    }
    
    LOG_INFO("Manual volume: %dml | Available: %lu/%lu ml", 
             volumeML, availableVolumeCurrent, availableVolumeMax);
    
    // Check if available volume empty
    if (availableVolumeCurrent == 0) {
        LOG_WARNING("Available volume empty after manual pump!");
    }
}


// ============================================
// 🆕 CHECK RESET BUTTON (Pin 8 - Active LOW)
// Funkcja: TYLKO reset z błędu (resetFromError)
// ============================================

void WaterAlgorithm::checkResetButton() {
    static bool lastButtonState = HIGH;           // Poprzedni stan (HIGH = not pressed)
    static uint32_t lastDebounceTime = 0;         // Czas ostatniej zmiany
    static bool buttonPressed = false;            // Czy przycisk jest wciśnięty
    
    const uint32_t DEBOUNCE_DELAY = 50;           // 50ms debouncing
    
    // Read current button state (INPUT_PULLUP, więc LOW = pressed)
    bool currentButtonState = digitalRead(RESET_PIN);
    
    // Sprawdź czy stan się zmienił
    if (currentButtonState != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    // Debouncing - sprawdź czy stan jest stabilny przez DEBOUNCE_DELAY
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        
        // Przycisk został naciśnięty (HIGH → LOW)
        if (currentButtonState == LOW && !buttonPressed) {
            buttonPressed = true;
            LOG_INFO("🔘 Reset button pressed");
        }
        
        // Przycisk został zwolniony (LOW → HIGH)
        else if (currentButtonState == HIGH && buttonPressed) {
            buttonPressed = false;
            LOG_INFO("🔘 Reset button released");
            
            // Sprawdź czy system jest w stanie błędu
            if (currentState == STATE_ERROR) {
                LOG_INFO("====================================");
                LOG_INFO("✅ RESET FROM ERROR STATE");
                LOG_INFO("====================================");
                LOG_INFO("Previous error: %s", 
                         lastError == ERROR_DAILY_LIMIT ? "ERR1 (Daily Limit)" :
                         lastError == ERROR_PUMP_FAILURE ? "ERR2 (Pump Failure)" : 
                         "ERR0 (Both)");
                
                // Wywołaj reset z błędu
                resetFromError();
                
                LOG_INFO("System state: %s", getStateString());
                LOG_INFO("Error signal: CLEARED");
                LOG_INFO("====================================");
                
                // Visual feedback - krótkie mignięcie LED (potwierdzenie)
                digitalWrite(ERROR_SIGNAL_PIN, HIGH);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, LOW);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, HIGH);
                delay(100);
                digitalWrite(ERROR_SIGNAL_PIN, LOW);
                
            } else {
                LOG_INFO("ℹ️ System not in error state");
                LOG_INFO("Current state: %s", getStateString());
                LOG_INFO("Reset button ignored (no error to clear)");
            }
        }
    }
    
    lastButtonState = currentButtonState;
}

// ===============================
// 🆕 NEW: AVAILABLE VOLUME METHODS
// ===============================

void WaterAlgorithm::setAvailableVolume(uint32_t maxMl) {
    if (maxMl < 100 || maxMl > 10000) {
        LOG_ERROR("Invalid available volume: %lu (range: 100-10000ml)", maxMl);
        return;
    }
    
    availableVolumeMax = maxMl;
    availableVolumeCurrent = maxMl;
    
    if (saveAvailableVolumeToFRAM(availableVolumeMax, availableVolumeCurrent)) {
        LOG_INFO("Available volume set to %lu ml", maxMl);
    }
}

void WaterAlgorithm::refillAvailableVolume() {
    availableVolumeCurrent = availableVolumeMax;
    
    if (saveAvailableVolumeToFRAM(availableVolumeMax, availableVolumeCurrent)) {
        LOG_INFO("Available volume refilled to %lu ml", availableVolumeMax);
    }
}

uint32_t WaterAlgorithm::getAvailableVolumeMax() const {
    return availableVolumeMax;
}

uint32_t WaterAlgorithm::getAvailableVolumeCurrent() const {
    return availableVolumeCurrent;
}

bool WaterAlgorithm::isAvailableVolumeEmpty() const {
    return availableVolumeCurrent == 0;
}

// ===============================
// 🆕 NEW: CONFIGURABLE FILL_WATER_MAX METHODS
// ===============================

void WaterAlgorithm::setFillWaterMax(uint16_t maxMl) {
    if (maxMl < 100 || maxMl > 10000) {
        LOG_ERROR("Invalid fill water max: %d (range: 100-10000ml)", maxMl);
        return;
    }
    
    fillWaterMaxConfig = maxMl;
    
    if (saveFillWaterMaxToFRAM(fillWaterMaxConfig)) {
        LOG_INFO("Fill water max set to %d ml", maxMl);
    }
}

uint16_t WaterAlgorithm::getFillWaterMax() const {
    return fillWaterMaxConfig;
}

bool WaterAlgorithm::resetDailyVolume() {
    LOG_INFO("====================================");
    LOG_INFO("MANUAL DAILY VOLUME RESET REQUESTED");
    LOG_INFO("====================================");
    LOG_INFO("Previous volume: %dml", dailyVolumeML);
    LOG_INFO("Current UTC day: %lu", lastResetUTCDay);
    
    if (isPumpActive()) {
        LOG_WARNING("❌ Reset blocked - pump is active");
        return false;
    }
    
    dailyVolumeML = 0;
    todayCycles.clear();
    
    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay)) {
        LOG_ERROR("⚠️ Failed to save reset volume to FRAM");
        return false;
    }
    
    LOG_INFO("✅ Daily volume reset to 0ml");
    LOG_INFO("UTC day remains: %lu", lastResetUTCDay);
    LOG_INFO("====================================");
    
    // Log to VPS
    // uint32_t unixTime = getUnixTimestamp();
    // bool vpsSuccess = logEventToVPS("STATISTICS_RESET", 0, unixTime);
    
    // if (vpsSuccess) {
    //     LOG_INFO("✅ Volume reset + VPS logging: SUCCESS");
    // } else {
    //     LOG_WARNING("⚠️ Volume reset: SUCCESS, VPS logging: FAILED (non-critical)");
    // }
    
    return true;
}

// ===============================================
// UI STATUS FUNCTIONS - User-friendly descriptions
// ===============================================

String WaterAlgorithm::getStateDescription() const {
    // ============== SYSTEM DISABLED STATE ==============
    if (isSystemDisabled()) {
        unsigned long remaining = 0;
        if (systemDisabledTime > 0) {
            unsigned long elapsed = millis() - systemDisabledTime;
            if (elapsed < SYSTEM_AUTO_ENABLE_MS) {
                remaining = (SYSTEM_AUTO_ENABLE_MS - elapsed) / 1000;
            }
        }
        
        if (remaining > 0) {
            uint32_t minutes = remaining / 60;
            uint32_t seconds = remaining % 60;
            return "SYSTEM OFF - Auto-enable in " + String(minutes) + ":" + 
                   (seconds < 10 ? "0" : "") + String(seconds);
        }
        return "SYSTEM OFF";
    }
    
    switch (currentState) {
        case STATE_IDLE:
            return "IDLE - Waiting for sensors";

        case STATE_PRE_QUALIFICATION:
            return "Pre-qualification (quick test)";

        case STATE_SETTLING:
            return "Settling - water calming down";

        case STATE_DEBOUNCING:
            return "Debouncing - verifying drain";

        case STATE_PUMPING_AND_VERIFY:
            if (isPumpActive()) {
                return "Pump operating + monitoring sensors";
            } else {
                return "Verifying sensor response";
            }

        case STATE_LOGGING:
            return "Logging cycle data";

        case STATE_ERROR:
            // Detailed error message
            switch (lastError) {
                case ERROR_DAILY_LIMIT:
                    return "ERROR - Daily limit exceeded";
                case ERROR_PUMP_FAILURE:
                    return "ERROR - Pump failure (3 attempts failed)";
                case ERROR_BOTH:
                    return "ERROR - Multiple errors detected";
                default:
                    return "ERROR - Unknown error";
            }
            
        case STATE_MANUAL_OVERRIDE:
            return "Manual pump operation";
            
        default:
            return "Unknown state";
    }
}

uint32_t WaterAlgorithm::getRemainingSeconds() const {
    // ============== SYSTEM DISABLED COUNTDOWN ==============
    if (isSystemDisabled() && systemDisabledTime > 0) {
        unsigned long elapsed = millis() - systemDisabledTime;
        if (elapsed < SYSTEM_AUTO_ENABLE_MS) {
            return (SYSTEM_AUTO_ENABLE_MS - elapsed) / 1000;
        }
        return 0;
    }
    
    uint32_t currentTime = getCurrentTimeSeconds();
    uint32_t elapsed = 0;
    uint32_t total = 0;
    int32_t remaining = 0;
    
    switch (currentState) {
        case STATE_IDLE:
        case STATE_LOGGING:
        case STATE_ERROR:
        case STATE_MANUAL_OVERRIDE:
            // No countdown for these states
            return 0;

        case STATE_PRE_QUALIFICATION:
            // Pre-qualification timeout
            elapsed = currentTime - stateStartTime;
            if (elapsed >= PRE_QUAL_WINDOW) {
                return 0;
            }
            return PRE_QUAL_WINDOW - elapsed;

        case STATE_SETTLING:
            // Settling countdown
            elapsed = currentTime - stateStartTime;
            if (elapsed >= SETTLING_TIME) {
                return 0;
            }
            return SETTLING_TIME - elapsed;

        case STATE_DEBOUNCING:
            // Debouncing timeout
            elapsed = currentTime - stateStartTime;
            if (elapsed >= TOTAL_DEBOUNCE_TIME) {
                return 0;
            }
            return TOTAL_DEBOUNCE_TIME - elapsed;

        case STATE_PUMPING_AND_VERIFY:
            // Pump + verify - return time until timeout (WATER_TRIGGER_MAX_TIME)
            elapsed = currentTime - pumpStartTime;
            if (elapsed >= WATER_TRIGGER_MAX_TIME) {
                return 0;
            }
            return WATER_TRIGGER_MAX_TIME - elapsed;

        default:
            return 0;
    }
}

