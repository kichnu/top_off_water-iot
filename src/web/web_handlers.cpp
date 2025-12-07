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
        request->redirect("/login");
        return;
    }
    request->send(200, "text/html", getDashboardHTML());
}

void handleLoginPage(AsyncWebServerRequest* request) {
    request->send(200, "text/html", getLoginHTML());
}

void handleLogin(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    
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

void handlePumpNormal(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    bool success = triggerPump(currentPumpSettings.manualCycleSeconds, "MANUAL_NORMAL");
    
    JsonDocument json;
    json["success"] = success;
    json["duration"] = currentPumpSettings.manualCycleSeconds;
    json["volume_ml"] = currentPumpSettings.manualCycleSeconds * currentPumpSettings.volumePerSecond;
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
    
    LOG_INFO("Manual normal pump triggered via web");
}

void handlePumpExtended(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    bool success = triggerPump(currentPumpSettings.calibrationCycleSeconds, "MANUAL_EXTENDED");
    
    JsonDocument json;
    json["success"] = success;
    json["duration"] = currentPumpSettings.calibrationCycleSeconds;
    json["type"] = "extended";
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
    
    LOG_INFO("Manual extended pump triggered via web");
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
    String response = "{";
    response += "\"success\":true,";
    response += "\"daily_volume\":" + String(waterAlgorithm.getDailyVolume()) + ",";
    response += "\"max_volume\":" + String(FILL_WATER_MAX) + ",";
    response += "\"last_reset_utc_day\":" + String(waterAlgorithm.getLastResetUTCDay());
    response += "}";
    
    request->send(200, "application/json", response);
}

void handleResetDailyVolume(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    
    // Check authentication
    if (!checkAuthentication(request)) {
        LOG_WARNING("Unauthorized daily volume reset attempt from %s", clientIP.toString().c_str());
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Check rate limiting
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
