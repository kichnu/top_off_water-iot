// #include "vps_logger.h"
// #include "../config/config.h"
// #include "../hardware/water_sensors.h"
// #include "../network/wifi_manager.h"
// #include "../core/logging.h"
// #include "../hardware/rtc_controller.h"
// #include "../hardware/fram_controller.h" 
// #include <HTTPClient.h>
// #include <ArduinoJson.h>
// #include "../algorithm/algorithm_config.h"
// #include "../algorithm/water_algorithm.h"
// #include "../config/credentials_manager.h"

// extern WaterAlgorithm waterAlgorithm;

// void initVPSLogger() {
//     LOG_INFO("VPS Logger initialized - endpoint: %s", getVPSURL());
//     LOG_INFO("Using %s credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
// }

// bool logEventToVPS(const String& eventType, uint16_t volumeML, uint32_t unixTime) {
//     if (!pumpGlobalEnabled) {
//         return false;
//     }

//     if (!isWiFiConnected()) {
//         LOG_WARNING("WiFi not connected, cannot log to VPS");
//         return false;
//     }
    
//     uint16_t timeoutMs;
//     bool isCritical;
    
//     if (eventType == "STATISTICS_RESET") {
//         timeoutMs = 3000;
//         isCritical = false;
//     } else if (eventType == "AUTO_CYCLE_COMPLETE") {
//         timeoutMs = 8000;
//         isCritical = true;
//     } else {
//         timeoutMs = 5000;
//         isCritical = true;
//     }
    
//     HTTPClient http;
    
//     const char* vpsUrl = getVPSURL();
//     http.begin(vpsUrl);
    
//     String authHeader = "Bearer " + String(getVPSAuthToken());
//     http.addHeader("Authorization", authHeader);
    
//     JsonDocument payload;
//     payload["device_id"] = getDeviceID();
    
//     LOG_INFO("Using dynamic VPS URL: %s (timeout: %dms)", vpsUrl, timeoutMs);
    
//     http.addHeader("Content-Type", "application/json");
//     http.setTimeout(timeoutMs);
    
//     payload["unix_time"] = unixTime;
//     payload["event_type"] = eventType;
//     payload["volume_ml"] = volumeML;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = "OK";

//     if (rtcNeedsSynchronization()) {
//         payload["time_uncertain"] = true;
//     }

//     if (eventType == "MANUAL_NORMAL") {
//         payload["daily_volume_ml"] = waterAlgorithm.getDailyVolume();
//         LOG_INFO("Including daily_volume_ml in VPS payload: %dml", 
//                  waterAlgorithm.getDailyVolume());
//     }
    
//     String jsonString;
//     serializeJson(payload, jsonString);
    
//     LOG_INFO("Sending to VPS (%s priority): %s", 
//              isCritical ? "HIGH" : "LOW", 
//              jsonString.c_str());
    
//     unsigned long startTime = millis();
//     int httpCode = http.POST(jsonString);
//     unsigned long duration = millis() - startTime;
    
//     if (httpCode == 200) {
//         LOG_INFO("✅ VPS log successful: %s (%lums)", eventType.c_str(), duration);
//         http.end();
//         return true;
//     } else {
//         if (isCritical) {
//             LOG_ERROR("❌ VPS log failed: %s HTTP %d (%lums)", eventType.c_str(), httpCode, duration);
//         } else {
//             LOG_WARNING("⚠️ VPS log failed (non-critical): %s HTTP %d (%lums)", eventType.c_str(), httpCode, duration);
//         }
        
//         http.end();
//         return false;
//     }
// }

// bool logCycleToVPS(const PumpCycle& cycle, uint32_t unixTime) {
//     if (!pumpGlobalEnabled) {
//         return false;
//     }

//     if (!isWiFiConnected()) {
//         LOG_WARNING("WiFi not connected, cannot log cycle to VPS");
//         return false;
//     }
    
//     HTTPClient http;
    
//     const char* vpsUrl = getVPSURL();
//     http.begin(vpsUrl);
    
//     String authHeader = "Bearer " + String(getVPSAuthToken());
//     http.addHeader("Authorization", authHeader);
    
//     JsonDocument payload;
//     payload["device_id"] = getDeviceID();
    
//     http.addHeader("Content-Type", "application/json");
//     http.setTimeout(10000);
    
//     ErrorStats currentStats;
//     bool statsLoaded = loadErrorStatsFromFRAM(currentStats);
    
//     if (!statsLoaded) {
//         LOG_WARNING("Failed to load error stats from FRAM, using zeros");
//         currentStats.gap1_fail_sum = 0;
//         currentStats.gap2_fail_sum = 0;
//         currentStats.water_fail_sum = 0;
//         currentStats.last_reset_timestamp = millis() / 1000;
//     }
    
//     payload["unix_time"] = unixTime;
//     payload["event_type"] = "AUTO_CYCLE_COMPLETE";
//     payload["volume_ml"] = cycle.volume_dose;
//     payload["water_status"] = getWaterStatus();
//     payload["system_status"] = (cycle.error_code == 0) ? "OK" : "ERROR";
    
//     if (rtcNeedsSynchronization()) {
//         payload["time_uncertain"] = true;
//     }
    
//     payload["time_gap_1"] = cycle.time_gap_1;
//     payload["time_gap_2"] = cycle.time_gap_2;
//     payload["water_trigger_time"] = cycle.water_trigger_time;
//     payload["pump_duration"] = cycle.pump_duration;
//     payload["pump_attempts"] = cycle.pump_attempts;
    
//     payload["gap1_fail_sum"] = currentStats.gap1_fail_sum;
//     payload["gap2_fail_sum"] = currentStats.gap2_fail_sum;
//     payload["water_fail_sum"] = currentStats.water_fail_sum;
//     payload["last_reset_timestamp"] = currentStats.last_reset_timestamp;

//     payload["daily_volume_ml"] = waterAlgorithm.getDailyVolume();
    
//     payload["vps_endpoint"] = getVPSURL();
//     payload["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
    
//     String algorithmSummary = "";
//     algorithmSummary += "LIMITS(GAP1_MAX:" + String(TIME_GAP_1_MAX) + "s,";
//     // algorithmSummary += "GAP2_MAX:" + String(TIME_GAP_2_MAX) + "s,";
//     algorithmSummary += "WATER_MAX:" + String(WATER_TRIGGER_MAX_TIME) + "s) ";
//     // algorithmSummary += "THRESHOLDS(GAP1:" + String(THRESHOLD_1) + "s,";
//     // algorithmSummary += "GAP2:" + String(THRESHOLD_2) + "s,";
//     // algorithmSummary += "WATER:" + String(THRESHOLD_WATER) + "s) ";
//     algorithmSummary += "CURRENT(" + String((cycle.sensor_results & PumpCycle::RESULT_GAP1_FAIL) ? 1 : 0) + "-";
//     algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_GAP2_FAIL) ? 1 : 0) + "-";
//     algorithmSummary += String((cycle.sensor_results & PumpCycle::RESULT_WATER_FAIL) ? 1 : 0) + ") ";
//     algorithmSummary += "SUMS(" + String(currentStats.gap1_fail_sum) + "-";
//     algorithmSummary += String(currentStats.gap2_fail_sum) + "-";
//     algorithmSummary += String(currentStats.water_fail_sum) + ")";
//     payload["algorithm_data"] = algorithmSummary;
    
//     String jsonString;
//     serializeJson(payload, jsonString);
    
//     LOG_INFO("JSON size: %d bytes", jsonString.length());
//     LOG_INFO("Sending cycle to VPS (unix time: %lu)", (unsigned long)unixTime);
    
//     int httpCode = http.POST(jsonString);
    
//     if (httpCode == 200) {
//         LOG_INFO("VPS cycle log successful");
//         http.end();
//         return true;
//     } else {
//         LOG_ERROR("VPS cycle log failed: HTTP %d", httpCode);
//         http.end();
//         return false;
//     }
// }
