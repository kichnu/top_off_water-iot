

#include "wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <WiFi.h>
#include "../config/credentials_manager.h"

unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30 seconds

void initWiFi() {
    WiFi.mode(WIFI_STA);
    
    // Use dynamic credentials
    const char* ssid = getWiFiSSID();
    const char* password = getWiFiPassword();
    
    LOG_INFO("Connecting to WiFi: %s", ssid);
    LOG_INFO("Credentials source: %s", areCredentialsLoaded() ? "FRAM" : "Hardcoded fallback");
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 50) {
        delay(500);
        attempts++;
        LOG_INFO("WiFi connection attempt %d/50", attempts);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("WiFi connected - IP: %s", WiFi.localIP().toString().c_str());
        LOG_INFO("Using %s credentials", areCredentialsLoaded() ? "FRAM" : "fallback");
    } else {
        LOG_ERROR("WiFi connection failed after 50 attempts");
        if (!areCredentialsLoaded()) {
            LOG_ERROR("Consider programming correct credentials via Captive Portal");
        }
    }
}

void updateWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            LOG_WARNING("WiFi reconnecting...");
            
            // Use dynamic credentials for reconnection
            WiFi.begin(getWiFiSSID(), getWiFiPassword());
            lastReconnectAttempt = now;
        }
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String getWiFiStatus() {
    switch (WiFi.status()) {
        case WL_CONNECTED: return "Connected";
        case WL_NO_SSID_AVAIL: return "SSID not found";
        case WL_CONNECT_FAILED: return "Connection failed";
        case WL_CONNECTION_LOST: return "Connection lost";
        case WL_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

IPAddress getLocalIP() {
    return WiFi.localIP();
}
