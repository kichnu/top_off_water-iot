
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
    
    lastError = ERROR_NONE;
    errorSignalActive = false;
    lastSensor1State = false;
    lastSensor2State = false;
    todayCycles.clear();

    framDataLoaded = false;
    lastFRAMCleanup = millis();
    framCycles.clear();
    
    // ============== SYSTEM DISABLE FLAG INIT ==============
    systemWasDisabled = false;
    
    loadCyclesFromStorage();

    ErrorStats stats;
    if (loadErrorStatsFromFRAM(stats)) {
        LOG_INFO("Error statistics loaded from FRAM");
    } else {
        LOG_WARNING("Could not load error stats from FRAM");
    }
    
    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    pinMode(RESET_PIN, INPUT_PULLUP);
    
    LOG_INFO("WaterAlgorithm constructor completed (minimal init)");
}

void WaterAlgorithm::resetCycle() {

    currentCycle = {};
    currentCycle.timestamp = getCurrentTimeSeconds(); 
    triggerStartTime = 0;
    sensor1TriggerTime = 0;
    sensor2TriggerTime = 0;
    sensor1ReleaseTime = 0;
    sensor2ReleaseTime = 0;
    pumpStartTime = 0;
    waitingForSecondSensor = false;
    pumpAttempts = 0;
    cycleLogged = false;
    permission_log = true;
    waterFailDetected = false;
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
        if (actualVolumeML > 0) {
            dailyVolumeML += actualVolumeML;
            saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay);
            LOG_INFO("Partial volume added: %dml, daily total: %dml", actualVolumeML, dailyVolumeML);
        }
        
        // Save to FRAM
        saveCycleToStorage(currentCycle);
        
        // Log to VPS
        uint32_t unixTime = getUnixTimestamp();
        logEventToVPS("SYSTEM_DISABLED_INTERRUPT", actualVolumeML, unixTime);
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
            // Sensors active - trigger immediate cycle start
            LOG_INFO("Sensors active - starting cycle immediately");
            LOG_INFO("====================================");
            
            // Simulate sensor trigger to start cycle
            uint32_t currentTime = getCurrentTimeSeconds();
            triggerStartTime = currentTime;
            currentCycle.trigger_time = currentTime;
            currentCycle.timestamp = currentTime;
            
            if (sensor1) {
                sensor1TriggerTime = currentTime;
                lastSensor1State = true;
            }
            if (sensor2) {
                sensor2TriggerTime = currentTime;
                lastSensor2State = true;
            }
            
            // Start TRYB_1 wait
            currentState = STATE_TRYB_1_WAIT;
            stateStartTime = currentTime;
            waitingForSecondSensor = !(sensor1 && sensor2);  // false if both already active
            
            // If both sensors already active, calculate TIME_GAP_1 immediately
            if (sensor1 && sensor2) {
                currentCycle.time_gap_1 = 0;  // Simultaneous activation
                LOG_INFO("Both sensors active - TIME_GAP_1 = 0");
                
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
            }
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
    
    // Execute delayed reset when pump finishes
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

    // RESZTA FUNKCJI BEZ ZMIAN (switch statement itd.)
    switch (currentState) {
        case STATE_IDLE:
            break;
        case STATE_TRYB_1_WAIT:
            if (stateElapsed >= TIME_GAP_1_MAX) {
                currentCycle.time_gap_1 = TIME_GAP_1_MAX;
                LOG_INFO("TRYB_1: TIME_GAP_1 timeout, using max: %ds", TIME_GAP_1_MAX);
                
                if (sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1)) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
                }
                
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
                LOG_INFO("TRYB_1: Starting TIME_TO_PUMP delay (%ds)", TIME_TO_PUMP);
            }
            break;
                    
        case STATE_TRYB_1_DELAY:
            if (currentTime - triggerStartTime >= TIME_TO_PUMP) {
                uint16_t pumpWorkTime = calculatePumpWorkTime(currentPumpSettings.volumePerSecond);
                
                if (!validatePumpWorkTime(pumpWorkTime)) {
                    LOG_ERROR("PUMP_WORK_TIME (%ds) exceeds WATER_TRIGGER_MAX_TIME (%ds)", 
                            pumpWorkTime, WATER_TRIGGER_MAX_TIME);
                    pumpWorkTime = WATER_TRIGGER_MAX_TIME - 10;
                }
                
                LOG_INFO("TRYB_2: Starting pump attempt %d/%d for %ds", 
                        pumpAttempts + 1, PUMP_MAX_ATTEMPTS, pumpWorkTime);
                
                pumpStartTime = currentTime;
                pumpAttempts++;
                
                triggerPump(pumpWorkTime, "AUTO_PUMP");
                
                currentCycle.pump_duration = pumpWorkTime;
                currentState = STATE_TRYB_2_PUMP;
                stateStartTime = currentTime;
            }
            break;
               
        case STATE_TRYB_2_PUMP:
            if (!isPumpActive()) {
                LOG_INFO("TRYB_2: Pump finished, checking sensors");
                currentState = STATE_TRYB_2_VERIFY;
                stateStartTime = currentTime;
            }
            break;

        case STATE_TRYB_2_VERIFY: {
            static uint32_t lastStatusLog = 0;
            if (currentTime - lastStatusLog >= 5) {
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                // LOG_INFO("TRYB_2_VERIFY: Waiting for sensors... %ds/%ds (attempt %d/%d)", 
                //         timeSincePumpStart, WATER_TRIGGER_MAX_TIME, pumpAttempts, PUMP_MAX_ATTEMPTS);###################################################################
                lastStatusLog = currentTime;
            }

            bool sensorsOK = !readWaterSensor1() && !readWaterSensor2();
            
            if (sensorsOK) {
                calculateWaterTrigger();
                LOG_INFO("TRYB_2: Sensors deactivated, water_trigger_time: %ds", 
                        currentCycle.water_trigger_time);
                
                uint8_t tryb1Result = sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1);
                if (tryb1Result == 0) {
                    currentState = STATE_TRYB_2_WAIT_GAP2;
                    stateStartTime = currentTime;
                    waitingForSecondSensor = true;
                    LOG_INFO("TRYB_2: TRYB_1_result=0, waiting for TIME_GAP_2");
                } else {
                    LOG_INFO("TRYB_2: TRYB_1_result=1, skipping TIME_GAP_2");
                    currentState = STATE_LOGGING;
                    stateStartTime = currentTime;
                }
            } else {
                uint32_t timeSincePumpStart = currentTime - pumpStartTime;
                
                if (timeSincePumpStart >= WATER_TRIGGER_MAX_TIME) {
                    currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;

                    if (WATER_TRIGGER_MAX_TIME >= THRESHOLD_WATER) {
                        waterFailDetected = true;
                        LOG_INFO("WATER fail detected in attempt %d/%d", pumpAttempts, PUMP_MAX_ATTEMPTS);
                    }
                    
                    LOG_WARNING("TRYB_2: Timeout after %ds (limit: %ds), attempt %d/%d", 
                            timeSincePumpStart, WATER_TRIGGER_MAX_TIME, 
                            pumpAttempts, PUMP_MAX_ATTEMPTS);
                    
                    if (pumpAttempts < PUMP_MAX_ATTEMPTS) {
                        LOG_WARNING("TRYB_2: Retrying pump attempt %d/%d", 
                                pumpAttempts + 1, PUMP_MAX_ATTEMPTS);
                        
                        currentState = STATE_TRYB_1_DELAY;
                        stateStartTime = currentTime - TIME_TO_PUMP;
                    } else {
                        LOG_ERROR("TRYB_2: All %d pump attempts failed!", PUMP_MAX_ATTEMPTS);
                        currentCycle.error_code = ERROR_PUMP_FAILURE;
                                        
                        LOG_INFO("Logging failed cycle before entering ERROR state");
                        logCycleComplete();
                                        
                        startErrorSignal(ERROR_PUMP_FAILURE);
                        currentState = STATE_ERROR;
                    }
                }
            }
            break;
        }

        case STATE_TRYB_2_WAIT_GAP2: 
            static bool debugOnce = true;
            if (debugOnce) {
                LOG_INFO("DEBUG GAP2: s1Release=%ds, s2Release=%ds, waiting=%d", 
                        sensor1ReleaseTime, sensor2ReleaseTime, waitingForSecondSensor);
                debugOnce = false;
            }
            
            if (sensor1ReleaseTime && sensor2ReleaseTime && waitingForSecondSensor) {
                calculateTimeGap2();
                waitingForSecondSensor = false;
                LOG_INFO("TRYB_2: TIME_GAP_2 calculated successfully");
                
                currentState = STATE_LOGGING;
                stateStartTime = currentTime;
            } else if (stateElapsed >= TIME_GAP_2_MAX) {
                currentCycle.time_gap_2 = TIME_GAP_2_MAX;

                uint8_t result = sensor_time_match_function(currentCycle.time_gap_2, THRESHOLD_2);
                if (result == 1) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP2_FAIL;
                }

                LOG_WARNING("TRYB_2: TIME_GAP_2 timeout - s1Release=%ds, s2Release=%ds", 
                        sensor1ReleaseTime, sensor2ReleaseTime);
              
                currentState = STATE_LOGGING;
                stateStartTime = currentTime;
            }
            break;
            
        case STATE_LOGGING:
            if(permission_log){
                LOG_INFO("==================case STATE_LOGGING");
                permission_log = false;      
            } 
        
            if (!cycleLogged) {
                logCycleComplete();
                cycleLogged = true;
                
                if (dailyVolumeML > FILL_WATER_MAX) {
                    LOG_ERROR("Daily limit exceeded! %dml > %dml", dailyVolumeML, FILL_WATER_MAX);
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
    
    LOG_INFO("INIT COMPLETE:");
    LOG_INFO("  dailyVolumeML: %dml", dailyVolumeML);
    LOG_INFO("  lastResetUTCDay: %lu", lastResetUTCDay);
    LOG_INFO("====================================");
}

void WaterAlgorithm::onSensorStateChange(uint8_t sensorNum, bool triggered) {
    uint32_t currentTime = getCurrentTimeSeconds(); // <-- ZMIANA: sekundy zamiast millis()
    
    // ============== BLOCK SENSOR EVENTS WHEN SYSTEM DISABLED ==============
    if (isSystemDisabled()) {
        // Ignore sensor changes when system is disabled
        return;
    }
    
    // Update sensor states
    if (sensorNum == 1) {
        lastSensor1State = triggered;
        if (triggered) {
            sensor1TriggerTime = currentTime;
        } else {
            sensor1ReleaseTime = currentTime;
        }
    } else if (sensorNum == 2) {
        lastSensor2State = triggered;
        if (triggered) {
            sensor2TriggerTime = currentTime;
        } else {
            sensor2ReleaseTime = currentTime;
        }
    }
    
    // Handle state transitions based on sensor changes
    switch (currentState) {
        case STATE_IDLE:
            if (triggered && (lastSensor1State || lastSensor2State)) {
                // TRIGGER detected!
                LOG_INFO("TRIGGER detected! Starting TRYB_1");
                triggerStartTime = currentTime;
                currentCycle.trigger_time = currentTime;
                currentState = STATE_TRYB_1_WAIT;
                stateStartTime = currentTime;
                waitingForSecondSensor = true;
            }
            break;
            
        case STATE_TRYB_1_WAIT:
            if (waitingForSecondSensor && sensor1TriggerTime && sensor2TriggerTime) {
                // Both sensors triggered, calculate TIME_GAP_1
                calculateTimeGap1();
                waitingForSecondSensor = false;
                
                // Evaluate result
                if (sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1)) {
                    currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
                }
                
                // Continue waiting for TIME_TO_PUMP
                currentState = STATE_TRYB_1_DELAY;
                stateStartTime = currentTime;
                LOG_INFO("TRYB_1: Both sensors triggered, TIME_GAP_1=%ds", 
                        currentCycle.time_gap_1);
            }
            break;
            
        case STATE_TRYB_2_WAIT_GAP2:
            if (!triggered && waitingForSecondSensor && 
                sensor1ReleaseTime && sensor2ReleaseTime) {
                // Both sensors released, calculate TIME_GAP_2
                calculateTimeGap2();
                waitingForSecondSensor = false;
                LOG_INFO("TRYB_2: TIME_GAP_2=%dms", currentCycle.time_gap_2);
            }
            break;
            
        default:
            // Ignore sensor changes in other states
            break;
    }
}

void WaterAlgorithm::calculateTimeGap1() {
    if (sensor1TriggerTime && sensor2TriggerTime) {
        currentCycle.time_gap_1 = abs((int32_t)sensor2TriggerTime - 
                                      (int32_t)sensor1TriggerTime);
        
        // Wywołaj funkcję oceniającą zgodnie ze specyfikacją
        uint8_t result = sensor_time_match_function(currentCycle.time_gap_1, THRESHOLD_1);
        if (result == 1) {
            currentCycle.sensor_results |= PumpCycle::RESULT_GAP1_FAIL;
        }
        
        LOG_INFO("TIME_GAP_1: %ds, result: %d (threshold: %ds)", 
                currentCycle.time_gap_1, result, THRESHOLD_1);
    } else {
        LOG_WARNING("TIME_GAP_1 not calculated: s1Time=%ds, s2Time=%ds", 
                   sensor1TriggerTime, sensor2TriggerTime);
    }
}

void WaterAlgorithm::calculateTimeGap2() {
    if (sensor1ReleaseTime && sensor2ReleaseTime) {
        // Oblicz różnicę w sekundach (bez dzielenia przez 1000!)
        currentCycle.time_gap_2 = abs((int32_t)sensor2ReleaseTime - 
                                      (int32_t)sensor1ReleaseTime);
        
        // Wywołaj funkcję oceniającą zgodnie ze specyfikacją
        uint8_t result = sensor_time_match_function(currentCycle.time_gap_2, THRESHOLD_2);
        if (result == 1) {
            currentCycle.sensor_results |= PumpCycle::RESULT_GAP2_FAIL;
        }
        
        LOG_INFO("TIME_GAP_2: %ds, result: %d (threshold: %ds)", 
                currentCycle.time_gap_2, result, THRESHOLD_2);
    } else {
        LOG_WARNING("TIME_GAP_2 not calculated: s1Release=%ds, s2Release=%ds", 
                   sensor1ReleaseTime, sensor2ReleaseTime);
    }
}

void WaterAlgorithm::calculateWaterTrigger() {
    uint32_t earliestRelease = 0;
    
    // Znajdź najwcześniejszą deaktywację po starcie pompy
    if (sensor1ReleaseTime > pumpStartTime) {
        earliestRelease = sensor1ReleaseTime;
    }
    if (sensor2ReleaseTime > pumpStartTime && 
        (earliestRelease == 0 || sensor2ReleaseTime < earliestRelease)) {
        earliestRelease = sensor2ReleaseTime;
    }
    
    if (earliestRelease > 0) {
        // Różnica już w sekundach - bez dzielenia przez 1000!
        currentCycle.water_trigger_time = earliestRelease - pumpStartTime;
        
        // Sanity check
        if (currentCycle.water_trigger_time > WATER_TRIGGER_MAX_TIME) {
            currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        }
        
        LOG_INFO("WATER_TRIGGER_TIME: %ds", currentCycle.water_trigger_time);
        
        // Evaluate result
        if (sensor_time_match_function(currentCycle.water_trigger_time, THRESHOLD_WATER)) {
            currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        }
    } else {
        // No valid release detected
        currentCycle.water_trigger_time = WATER_TRIGGER_MAX_TIME;
        currentCycle.sensor_results |= PumpCycle::RESULT_WATER_FAIL;
        LOG_WARNING("No sensor release detected after pump start");
    }

        if (currentCycle.water_trigger_time >= THRESHOLD_WATER) {
        waterFailDetected = true;
        LOG_INFO("WATER fail detected in successful attempt");
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

    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay)) {
        LOG_WARNING("⚠️ Failed to save daily volume to FRAM");
    }
    
    // Store in today's cycles (RAM)
    todayCycles.push_back(currentCycle);
    
    // Keep only last 50 cycles in RAM (FRAM will store more)
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
    

    uint32_t unixTime = getUnixTimestamp();
    
    if (logCycleToVPS(currentCycle, unixTime)) {
        LOG_INFO("Cycle data sent to VPS successfully");
    } else {
        LOG_WARNING("Failed to send cycle data to VPS");
    }
    
    LOG_INFO("=== CYCLE COMPLETE ===");
    LOG_INFO("Actual volume: %dml (pump_duration: %ds)", actualVolumeML, currentCycle.pump_duration);
    LOG_INFO("TIME_GAP_1: %ds (fail=%d)", currentCycle.time_gap_1, gap1_increment);
    LOG_INFO("TIME_GAP_2: %ds (fail=%d)", currentCycle.time_gap_2, gap2_increment);
    LOG_INFO("WATER_TRIGGER_TIME: %ds (fail=%d)", currentCycle.water_trigger_time, water_increment);
    LOG_INFO("Daily volume: %dml / %dml", dailyVolumeML, FILL_WATER_MAX);
}

bool WaterAlgorithm::requestManualPump(uint16_t duration_ms) {

    // ============== BLOCK MANUAL PUMP WHEN SYSTEM DISABLED ==============
    if (isSystemDisabled()) {
        LOG_WARNING("❌ Manual pump blocked: System is disabled");
        return false;
    }

    if (dailyVolumeML >= FILL_WATER_MAX) {
        LOG_ERROR("❌ Manual pump blocked: Daily limit reached (%dml / %dml)", 
                  dailyVolumeML, FILL_WATER_MAX);
        return false;  // Block manual pump when limit reached
    }

    if (currentState == STATE_ERROR) {
        LOG_WARNING("Cannot start manual pump in error state");
        return false;
    }
    
    // SPRAWDŹ czy to AUTO_PUMP podczas automatycznego cyklu
    if (currentState != STATE_IDLE) {
        // Jeśli jesteśmy w automatycznym cyklu, nie resetuj danych!
        if (currentState == STATE_TRYB_1_DELAY || 
            currentState == STATE_TRYB_2_PUMP ||
            currentState == STATE_TRYB_2_VERIFY) {
            
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
        case STATE_TRYB_1_WAIT: return "TRYB_1_WAIT";
        case STATE_TRYB_1_DELAY: return "TRYB_1_DELAY";
        case STATE_TRYB_2_PUMP: return "TRYB_2_PUMP";
        case STATE_TRYB_2_VERIFY: return "TRYB_2_VERIFY";
        case STATE_TRYB_2_WAIT_GAP2: return "TRYB_2_WAIT_GAP2";
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
    
    // Load recent cycles from FRAM
    if (loadCyclesFromFRAM(framCycles, 200)) { // Load max 200 cycles
        framDataLoaded = true;
        LOG_INFO("Loaded %d cycles from FRAM", framCycles.size());


        LOG_INFO("Daily volume already loaded from FRAM: %dml", dailyVolumeML);

                // Just load today's cycles for display
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
}

void WaterAlgorithm::saveCycleToStorage(const PumpCycle& cycle) {
    if (saveCycleToFRAM(cycle)) {
        LOG_INFO("Cycle saved to FRAM successfully");
        
        // Add to framCycles for immediate access
        framCycles.push_back(cycle);
        
        // Keep framCycles size reasonable
        if (framCycles.size() > 200) {
            framCycles.erase(framCycles.begin());
        }
        
        // Periodic cleanup of old data (once per day)
        if (millis() - lastFRAMCleanup > 86400000UL) { // 24 hours
            clearOldCyclesFromFRAM(14); // Keep 14 days
            lastFRAMCleanup = millis();
            loadCyclesFromStorage(); // Reload after cleanup
            LOG_INFO("FRAM cleanup completed");
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
        uint32_t unixTime = getUnixTimestamp();
        String timestamp = getCurrentTimestamp();
        bool vpsSuccess = logEventToVPS("STATISTICS_RESET", 0, unixTime);
        
        if (vpsSuccess) {
            LOG_INFO("✅ Statistics reset + VPS logging: SUCCESS");
        } else {
            LOG_WARNING("⚠️ Statistics reset: SUCCESS, VPS logging: FAILED (non-critical)");
        }
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
    // 🆕 NEW: Add manual pump volume to daily total
    dailyVolumeML += volumeML;
    
    // Save to FRAM
    if (!saveDailyVolumeToFRAM(dailyVolumeML, lastResetUTCDay)) {
        LOG_WARNING("⚠️ Failed to save daily volume to FRAM after manual pump");
    }
    
    LOG_INFO("✅ Manual volume added: +%dml → Total: %dml / %dml", 
             volumeML, dailyVolumeML, FILL_WATER_MAX);
    
    // 🆕 NEW: Check if limit exceeded after manual pump
    if (dailyVolumeML >= FILL_WATER_MAX) {
        LOG_ERROR("❌ Daily limit reached after manual pump: %dml / %dml", 
                  dailyVolumeML, FILL_WATER_MAX);
        
        // Trigger error state
        currentCycle.error_code = ERROR_DAILY_LIMIT;
        startErrorSignal(ERROR_DAILY_LIMIT);
        currentState = STATE_ERROR;
        
        LOG_ERROR("System entering ERROR state - press reset button to clear");
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
    uint32_t unixTime = getUnixTimestamp();
    bool vpsSuccess = logEventToVPS("STATISTICS_RESET", 0, unixTime);
    
    if (vpsSuccess) {
        LOG_INFO("✅ Volume reset + VPS logging: SUCCESS");
    } else {
        LOG_WARNING("⚠️ Volume reset: SUCCESS, VPS logging: FAILED (non-critical)");
    }
    
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
            
        case STATE_TRYB_1_WAIT:
            return "Analyzing drain pattern...";
            
        case STATE_TRYB_1_DELAY:
            return "Waiting before pump activation";
            
        case STATE_TRYB_2_PUMP:
            return "Pump operating";
            
        case STATE_TRYB_2_VERIFY:
            return "Verifying sensor response";
            
        case STATE_TRYB_2_WAIT_GAP2:
            return "Measuring recovery time";
            
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
            
        case STATE_TRYB_1_WAIT:
            // Waiting for second sensor (TIME_GAP_1_MAX)
            elapsed = currentTime - stateStartTime;
            if (elapsed >= TIME_GAP_1_MAX) {
                return 0;
            }
            return TIME_GAP_1_MAX - elapsed;
            
        case STATE_TRYB_1_DELAY:
            // Waiting from TRIGGER to pump start (TIME_TO_PUMP)
            elapsed = currentTime - triggerStartTime;
            if (elapsed >= TIME_TO_PUMP) {
                return 0;
            }
            return TIME_TO_PUMP - elapsed;
            
        case STATE_TRYB_2_PUMP:
            // Pump is running - return pump remaining time
            // Use getPumpRemainingTime() from pump_controller
            return getPumpRemainingTime();
            
        case STATE_TRYB_2_VERIFY:
            // Waiting for sensors to respond (WATER_TRIGGER_MAX_TIME)
            elapsed = currentTime - pumpStartTime;
            if (elapsed >= WATER_TRIGGER_MAX_TIME) {
                return 0;
            }
            return WATER_TRIGGER_MAX_TIME - elapsed;
            
        case STATE_TRYB_2_WAIT_GAP2:
            // Waiting for TIME_GAP_2 measurement
            elapsed = currentTime - stateStartTime;
            if (elapsed >= TIME_GAP_2_MAX) {
                return 0;
            }
            return TIME_GAP_2_MAX - elapsed;
            
        default:
            return 0;
    }
}

