#include "web_server.h"
#include "web_handlers.h"
#include "../security/session_manager.h"
#include "../security/rate_limiter.h"
#include "../security/auth_manager.h"
#include "../core/logging.h"

AsyncWebServer server(80);

void initWebServer() {
    // Static pages
    server.on("/", HTTP_GET, handleDashboard);
    server.on("/login", HTTP_GET, handleLoginPage);
    
    // Authentication
    server.on("/api/login", HTTP_POST, handleLogin);
    server.on("/api/logout", HTTP_POST, handleLogout);
    
    // API endpoints
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/pump/normal", HTTP_POST, handlePumpNormal);
    server.on("/api/pump/extended", HTTP_POST, handlePumpExtended);
    server.on("/api/pump/stop", HTTP_POST, handlePumpStop);
    server.on("/api/pump-settings", HTTP_GET | HTTP_POST, handlePumpSettings);
    server.on("/api/pump-toggle", HTTP_GET | HTTP_POST, handlePumpToggle);
    server.on("/api/reset-statistics", HTTP_POST, handleResetStatistics);
    server.on("/api/get-statistics", HTTP_GET, handleGetStatistics);

    server.on("/api/daily-volume", HTTP_GET, handleGetDailyVolume);
    server.on("/api/reset-daily-volume", HTTP_POST, handleResetDailyVolume);
    
    // 404 handler
    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });
    
    server.begin();
    LOG_INFO("Web server started on port 80");
}

bool checkAuthentication(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    
    // Check if IP is blocked
    if (isIPBlocked(clientIP)) {
        return false;
    }
    
    // Check rate limiting
    if (isRateLimited(clientIP)) {
        recordFailedAttempt(clientIP);
        return false;
    }
    
    recordRequest(clientIP);
    
    // Check session cookie
    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            
            String token = cookie.substring(tokenStart, tokenEnd);
            if (validateSession(token, clientIP)) {
                return true;
            }
        }
    }
    
    recordFailedAttempt(clientIP);
    return false;
}
