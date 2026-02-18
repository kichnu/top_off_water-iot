
#include "web_handlers.h"
#include "web_server.h"
#include "html_pages.h"
#include "../security/auth_manager.h"
#include "../security/session_manager.h"
#include "../security/rate_limiter.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/rtc_controller.h"
#include "../network/wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <ArduinoJson.h>
#include "../config/credentials_manager.h"
#include "../algorithm/water_algorithm.h"

void handleDashboard(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->redirect("login");
        return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html", (const uint8_t*)DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    request->send(response);
}

void handleLoginPage(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html", (const uint8_t*)LOGIN_HTML, strlen(LOGIN_HTML));
    request->send(response);
}

void handleLogin(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    // Whitelist check - block login from unknown IPs (proxy allowed)
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        LOG_WARNING("Login attempt from non-whitelisted IP: %s", clientIP.toString().c_str());
        request->send(403, "application/json", "{\"success\":false,\"error\":\"Access denied\"}");
        return;
    }

    if (isRateLimited(clientIP) || isIPBlocked(clientIP)) {
        request->send(429, "text/plain", "Too Many Requests");
        return;
    }

    // Check if FRAM credentials are loaded
    if (!areCredentialsLoaded()) {
        recordFailedAttempt(clientIP);
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "System not configured";
        error_response["message"] = "FRAM credentials required. Use Captive Portal to configure.";
        error_response["setup_instructions"] = "1. Hold button 5s during boot  2. Connect to ESP32-WATER-SETUP  3. Configure credentials in browser";
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(503, "application/json", response_str);  // Service Unavailable
        return;
    }
 
    if (!request->hasParam("password", true)) {
        recordFailedAttempt(clientIP);
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing password\"}");
        return;
    }
    
    String password = request->getParam("password", true)->value();
    
    if (verifyPassword(password)) {
        String token = createSession(clientIP);
        String cookie = "session_token=" + token + "; Path=/; HttpOnly; Max-Age=" + String(SESSION_TIMEOUT_MS / 1000);
        
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
        response->addHeader("Set-Cookie", cookie);
        request->send(response);
    } else {
        recordFailedAttempt(clientIP);
                
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "Invalid password";
        
        if (!areCredentialsLoaded()) {
            error_response["message"] = "System requires FRAM credential programming";
        }
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(401, "application/json", response_str);
    }
}

void handleLogout(AsyncWebServerRequest* request) {
    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            
            String token = cookie.substring(tokenStart, tokenEnd);
            destroySession(token);
        }
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
    response->addHeader("Set-Cookie", "session_token=; Path=/; HttpOnly; Max-Age=0");
    request->send(response);
}

void handleStatus(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    JsonDocument json;
    
    // ============================================
    // HARDWARE STATUS (for badges)
    // ============================================
    json["sensor1_active"] = readWaterSensor1();
    json["sensor2_active"] = readWaterSensor2();
    json["pump_active"] = isPumpActive();
    json["pump_attempt"] = waterAlgorithm.getPumpAttempts();
    json["system_error"] = (waterAlgorithm.getState() == STATE_ERROR);
    
    // ============================================
    // SYSTEM DISABLE STATUS (NEW)
    // ============================================
    json["system_disabled"] = isSystemDisabled();
    
    
    // ============================================
    // PROCESS STATUS (for description + remaining time)
    // ============================================
    json["state_description"] = waterAlgorithm.getStateDescription();
    json["remaining_seconds"] = waterAlgorithm.getRemainingSeconds();
    
    // ============================================
    // EXISTING STATUS FIELDS
    // ============================================
    json["water_status"] = getWaterStatus();
    json["pump_running"] = isPumpActive();  // kept for backwards compatibility
    json["pump_remaining"] = getPumpRemainingTime();  // kept for backwards compatibility
    json["wifi_status"] = getWiFiStatus();
    json["wifi_connected"] = isWiFiConnected();
    json["rtc_time"] = getCurrentTimestamp();
    json["rtc_working"] = isRTCWorking();
    json["rtc_info"] = getTimeSourceInfo();
    json["rtc_hardware"] = isRTCHardware(); 
    json["rtc_needs_sync"] = rtcNeedsSynchronization();
    json["rtc_battery_issue"] = isBatteryIssueDetected();
    json["free_heap"] = ESP.getFreeHeap();
    json["uptime"] = millis();
    
    // ============================================
    // DEVICE INFO
    // ============================================
    json["device_id"] = getDeviceID();
    json["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
    json["vps_url"] = getVPSURL();
    json["authentication_enabled"] = areCredentialsLoaded();
    
    if (!areCredentialsLoaded()) {
        json["setup_required"] = true;
        json["setup_message"] = "Use Captive Portal to configure FRAM credentials";
    }
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOn(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    uint16_t duration = currentPumpSettings.manualCycleSeconds;
    String mode = "bistable";
    if (request->hasParam("mode", true) && request->getParam("mode", true)->value() == "monostable") {
        duration = DIRECT_PUMP_SAFETY_TIMEOUT_S;
        mode = "monostable";
    }

    bool success = directPumpOn(duration);

    JsonDocument json;
    json["success"] = success;
    json["duration"] = duration;
    json["mode"] = mode;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOff(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    directPumpOff();

    JsonDocument json;
    json["success"] = true;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handlePumpStop(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    stopPump();
    
    JsonDocument json;
    json["success"] = true;
    json["message"] = "Pump stopped";
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
    
    LOG_INFO("Pump manually stopped via web");
}

void handlePumpSettings(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        // Return current settings
        JsonDocument json;
        json["success"] = true;
        json["volume_per_second"] = currentPumpSettings.volumePerSecond;
        json["normal_cycle"] = currentPumpSettings.manualCycleSeconds;
        json["extended_cycle"] = currentPumpSettings.calibrationCycleSeconds;
        json["auto_mode"] = currentPumpSettings.autoModeEnabled;
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
        
    } else if (request->method() == HTTP_POST) {
        if (!request->hasParam("volume_per_second", true)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"No volume_per_second parameter\"}");
            return;
        }
        
        String volumeStr = request->getParam("volume_per_second", true)->value();
        float newVolume = volumeStr.toFloat();
        
        if (newVolume < 0.1 || newVolume > 20) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be between 0.1-20\"}");
            return;
        }
        
        currentPumpSettings.volumePerSecond = newVolume;

        // Save to NVS (non-volatile storage)
        saveVolumeToNVS();
        
        LOG_INFO("Volume per second updated to %.1f ml/s", newVolume);
        
        JsonDocument response;
        response["success"] = true;
        response["volume_per_second"] = newVolume;
        response["message"] = "Volume per second updated successfully";
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    }
}

// ============================================
// SYSTEM TOGGLE HANDLER (NEW)
// Replaces old pump toggle with full system control
// ============================================
void handleSystemToggle(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = !isSystemDisabled();

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) {
        bool currentlyDisabled = isSystemDisabled();
        setSystemState(currentlyDisabled);  // toggle

        JsonDocument json;
        json["success"] = true;
        json["enabled"] = currentlyDisabled;  // was disabled → now enabled, and vice versa

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

        LOG_INFO("System %s via web interface", currentlyDisabled ? "ENABLED" : "DISABLED");
    }
}

// ============================================
// LEGACY PUMP TOGGLE (kept for compatibility)
// Will be removed after frontend migration
// ============================================
void handlePumpToggle(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        // Return current state
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = pumpGlobalEnabled;
        
        if (!pumpGlobalEnabled && pumpDisabledTime > 0) {
            unsigned long remaining = PUMP_AUTO_ENABLE_MS - (millis() - pumpDisabledTime);
            json["remaining_seconds"] = remaining / 1000;
        } else {
            json["remaining_seconds"] = 0;
        }
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
        
    } else if (request->method() == HTTP_POST) {
        // Toggle pump state
        setPumpGlobalState(!pumpGlobalEnabled);
        
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = pumpGlobalEnabled;
        json["message"] = pumpGlobalEnabled ? "Pump enabled" : "Pump disabled for 30 minutes";
        
        if (!pumpGlobalEnabled) {
            json["remaining_seconds"] = PUMP_AUTO_ENABLE_MS / 1000;
        } else {
            json["remaining_seconds"] = 0;
        }
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    }
}

void handleResetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    // Reset statistics
    bool success = waterAlgorithm.resetErrorStatistics();
    
    JsonDocument json;
    json["success"] = success;
    json["message"] = success ? "Statistics reset successfully" : "Failed to reset statistics";
    
    if (success) {
        // Get current timestamp for display
        uint16_t gap1, gap2, water;
        uint32_t resetTime;
        if (waterAlgorithm.getErrorStatistics(gap1, gap2, water, resetTime)) {
            json["reset_timestamp"] = resetTime;
        }
    }
    
    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);
    
    LOG_INFO("Statistics reset requested via web interface - success: %s", success ? "true" : "false");
}

void handleGetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    // Get current statistics
    uint16_t gap1_sum, gap2_sum, water_sum;
    uint32_t last_reset;
    bool success = waterAlgorithm.getErrorStatistics(gap1_sum, gap2_sum, water_sum, last_reset);
    
    JsonDocument json;
    json["success"] = success;
    
    if (success) {
        json["gap1_fail_sum"] = gap1_sum;
        json["gap2_fail_sum"] = gap2_sum; 
        json["water_fail_sum"] = water_sum;
        json["last_reset_timestamp"] = last_reset;
        
        // Convert timestamp to readable format
        time_t resetTime = (time_t)last_reset;
        struct tm* timeinfo = localtime(&resetTime);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M", timeinfo);
        json["last_reset_formatted"] = String(timeStr);
    } else {
        json["error"] = "Failed to load statistics";
    }
    
    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);
}

// ========================================
// DAILY VOLUME HANDLERS
// ========================================

void handleGetDailyVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    String response = "{";
    response += "\"success\":true,";
    response += "\"daily_volume\":" + String(waterAlgorithm.getDailyVolume()) + ",";
    response += "\"max_volume\":" + String(waterAlgorithm.getFillWaterMax()) + ",";
    response += "\"last_reset_utc_day\":" + String(waterAlgorithm.getLastResetUTCDay());
    response += "}";

    request->send(200, "application/json", response);
}

void handleResetDailyVolume(AsyncWebServerRequest* request) {
    // Check authentication
    if (!checkAuthentication(request)) {
        LOG_WARNING("Unauthorized daily volume reset attempt from %s", resolveClientIP(request).toString().c_str());
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Check rate limiting
    IPAddress clientIP = resolveClientIP(request);
    if (isRateLimited(clientIP)) {
        request->send(429, "application/json", "{\"success\":false,\"error\":\"Too many requests\"}");
        return;
    }

    LOG_INFO("Daily volume reset requested from %s", clientIP.toString().c_str());
    
    // Perform reset
    bool success = waterAlgorithm.resetDailyVolume();
    
    if (success) {
        String response = "{";
        response += "\"success\":true,";
        response += "\"daily_volume\":" + String(waterAlgorithm.getDailyVolume()) + ",";
        response += "\"last_reset_utc_day\":" + String(waterAlgorithm.getLastResetUTCDay());
        response += "}";
        
        request->send(200, "application/json", response);
        LOG_INFO("✅ Daily volume reset successful via web interface");
    } else {
        request->send(400, "application/json", 
                     "{\"success\":false,\"error\":\"Cannot reset while pump is active\"}");
        LOG_WARNING("⚠️ Daily volume reset blocked - pump is active");
    }
}

// ===============================
// 🆕 NEW: AVAILABLE VOLUME HANDLERS
// ===============================

void handleGetAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    String response = "{";
    response += "\"success\":true,";
    response += "\"max_ml\":" + String(waterAlgorithm.getAvailableVolumeMax()) + ",";
    response += "\"current_ml\":" + String(waterAlgorithm.getAvailableVolumeCurrent()) + ",";
    response += "\"is_empty\":" + String(waterAlgorithm.isAvailableVolumeEmpty() ? "true" : "false");
    response += "}";

    request->send(200, "application/json", response);
}

void handleSetAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }
    
    int value = request->getParam("value", true)->value().toInt();
    
    if (value < 100 || value > 10000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 100-10000 ml\"}");
        return;
    }
    
    waterAlgorithm.setAvailableVolume(value);
    
    String response = "{";
    response += "\"success\":true,";
    response += "\"max_ml\":" + String(waterAlgorithm.getAvailableVolumeMax()) + ",";
    response += "\"current_ml\":" + String(waterAlgorithm.getAvailableVolumeCurrent());
    response += "}";
    
    request->send(200, "application/json", response);
}

void handleRefillAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    waterAlgorithm.refillAvailableVolume();
    
    String response = "{";
    response += "\"success\":true,";
    response += "\"max_ml\":" + String(waterAlgorithm.getAvailableVolumeMax()) + ",";
    response += "\"current_ml\":" + String(waterAlgorithm.getAvailableVolumeCurrent());
    response += "}";
    
    request->send(200, "application/json", response);
}

// ===============================
// 🆕 NEW: FILL WATER MAX HANDLERS
// ===============================

void handleGetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    String response = "{";
    response += "\"success\":true,";
    response += "\"fill_water_max\":" + String(waterAlgorithm.getFillWaterMax());
    response += "}";

    request->send(200, "application/json", response);
}

void handleSetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }
    
    int value = request->getParam("value", true)->value().toInt();
    
    if (value < 100 || value > 10000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 100-10000 ml\"}");
        return;
    }
    
    waterAlgorithm.setFillWaterMax(value);

    String response = "{";
    response += "\"success\":true,";
    response += "\"fill_water_max\":" + String(waterAlgorithm.getFillWaterMax());
    response += "}";

    request->send(200, "application/json", response);
}

// ===============================
// CYCLE HISTORY HANDLER
// ===============================

extern volatile bool framBusy;

void handleGetCycleHistory(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (framBusy) {
        request->send(503, "application/json", "{\"success\":false,\"error\":\"System busy\"}");
        return;
    }

    const std::vector<PumpCycle>& cycles = waterAlgorithm.getCycleHistory();

    JsonDocument doc;
    doc["success"] = true;
    doc["total"] = cycles.size();

    JsonArray arr = doc["cycles"].to<JsonArray>();

    // Iterate newest-first (vector is ordered oldest-first from FRAM load)
    for (int i = (int)cycles.size() - 1; i >= 0; i--) {
        const PumpCycle& c = cycles[i];
        JsonObject obj = arr.add<JsonObject>();

        obj["ts"] = c.timestamp;
        obj["gap1_s"] = c.time_gap_1;
        obj["gap2_s"] = c.time_gap_2;
        obj["wt_s"] = c.water_trigger_time;
        obj["pump_s"] = c.pump_duration;
        obj["attempts"] = c.pump_attempts;
        obj["volume_ml"] = c.volume_dose;
        obj["alarm"] = c.error_code;

        // Decoded sensor flags
        bool s1_deb_fail = c.sensor_results & PumpCycle::RESULT_SENSOR1_DEBOUNCE_FAIL;
        bool s2_deb_fail = c.sensor_results & PumpCycle::RESULT_SENSOR2_DEBOUNCE_FAIL;

        obj["s1_deb"] = !s1_deb_fail;
        obj["s2_deb"] = !s2_deb_fail;
        obj["deb_ok"] = !(c.sensor_results & PumpCycle::RESULT_FALSE_TRIGGER);

        // Release: 1=OK, 0=N/A (nie sprawdzany), -1=FAIL
        if (s1_deb_fail) {
            obj["s1_rel"] = 0;
        } else if (c.sensor_results & (PumpCycle::RESULT_SENSOR1_RELEASE_FAIL | PumpCycle::RESULT_WATER_FAIL)) {
            obj["s1_rel"] = -1;
        } else {
            obj["s1_rel"] = 1;
        }

        if (s2_deb_fail) {
            obj["s2_rel"] = 0;
        } else if (c.sensor_results & (PumpCycle::RESULT_SENSOR2_RELEASE_FAIL | PumpCycle::RESULT_WATER_FAIL)) {
            obj["s2_rel"] = -1;
        } else {
            obj["s2_rel"] = 1;
        }
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// ===============================
// SYSTEM RESET HANDLER
// ===============================

void handleSystemReset(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = waterAlgorithm.resetSystem();

    JsonDocument json;
    json["success"] = success;
    json["state"] = waterAlgorithm.getStateString();
    json["message"] = success ? "System reset to IDLE" : "Reset blocked - logging in progress";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// HEALTH CHECK HANDLER
// ===============================

void handleHealth(AsyncWebServerRequest *request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
        request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
        return;
    }

    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"device_name\":\"" + String(getDeviceID()) + "\",";
    json += "\"uptime\":" + String(millis());
    json += "}";

    request->send(200, "application/json", json);
}