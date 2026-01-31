
#include <Arduino.h>
#include "core/logging.h"
#include "config/config.h"
#include "config/credentials_manager.h"
#include "hardware/rtc_controller.h"
#include "hardware/fram_controller.h"
#include "hardware/hardware_pins.h"
#include "hardware/water_sensors.h"
#include "hardware/pump_controller.h"
#include "network/wifi_manager.h"
#include "network/vps_logger.h"
#include "security/auth_manager.h"
#include "security/session_manager.h"
#include "security/rate_limiter.h"
#include "web/web_server.h"
#include "algorithm/water_algorithm.h"
#include "provisioning/prov_detector.h"
#include "provisioning/ap_core.h"
#include "provisioning/ap_server.h"

void setup() {
    // Initialize core systems
    initLogging();
    delay(5000); // Wait for serial monitor

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("=== ESP32-C3 Water System Starting ===");
    LOG_INFO("Single Mode - Captive Portal Provisioning");
    LOG_INFO("ESP32 Flash size: %d", ESP.getFlashChipSize());
    LOG_INFO("Free heap: %d", ESP.getFreeHeap());
    LOG_INFO("CHECKING PROVISIONING BUTTON");

    // Check if provisioning button is held
    if (checkProvisioningButton()) {
        LOG_INFO("");
        LOG_INFO("====================================");
        LOG_INFO("    ENTERING PROVISIONING MODE");
        LOG_INFO("====================================");
        LOG_INFO("Initializing FRAM for credential storage");
     
        if (!initFRAM()) {
            LOG_INFO("WARNING: FRAM initialization failed!");
            LOG_INFO("Credentials may not be saved properly");
            delay(2000);
        } else {
            LOG_INFO("");
            LOG_INFO("FRAM ready");
        }

        if (!startAccessPoint()) {
            LOG_INFO("");
            LOG_INFO("FATAL: Failed to start AP - halting");
            while(1) delay(1000);
        }
        
        // Start DNS Server for captive portal
        if (!startDNSServer()) {
            LOG_INFO("");
            LOG_INFO("FATAL: Failed to start DNS - halting");
            while(1) delay(1000);
        }
        
        LOG_INFO("");
        LOG_INFO("====================================");
        LOG_INFO("=== PROVISIONING MODE ACTIVE ===");
        LOG_INFO("Connect to WiFi network:");
        LOG_INFO("SSID: ESP32-WATER-SETUP");
        LOG_INFO("Password: setup12345");
        LOG_INFO("URL: http://");
        LOG_INFO("%s", getAPIPAddress().toString());

        // Start Web Server
        if (!startWebServer()) {
            Serial.println("FATAL: Failed to start web server - halting");
            while(1) delay(1000);
        }
        
        LOG_INFO("");
        LOG_INFO("=== CAPTIVE PORTAL READY ===");
        LOG_INFO("Open a browser or wait for captive portal popup");
  
        // Enter blocking loop - never returns
        runProvisioningLoop();
        
        // This line will never be reached
    }
    
    // === PRODUCTION MODE SETUP ===
    LOG_INFO("");
    LOG_INFO("=== Production Mode - Full Water System ===");

    initWaterSensors();
    initPumpController();

    initNVS();
    loadVolumeFromNVS();
    waterAlgorithm.initFromFRAM();

    bool credentials_loaded = initCredentialsManager();
    LOG_INFO("");
    if (credentials_loaded) {
        LOG_INFO("Device ID: %s", getDeviceID());
    } else {
        LOG_INFO("FALLBACK_MODE");
    }
    initWiFi();
    initializeRTC();
    delay(2000);

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("Initializing network...");
    LOG_INFO("[INIT] Initializing RTC...");
    LOG_INFO("RTC Status: %s", getRTCInfo().c_str());
    LOG_INFO("[INIT] Waiting for RTC to stabilize...");
    LOG_INFO("====================================");
    
    if (!isRTCWorking()) {
        LOG_WARNING("⚠️ WARNING: RTC not working properly");
        LOG_WARNING("⚠️ Daily volume tracking may be affected");
        LOG_WARNING("");
    }
    LOG_INFO("Initializing daily volume tracking...");
    waterAlgorithm.initDailyVolume();

    // Initialize security
    initAuthManager();
    initSessionManager();
    initRateLimiter();
    
    // Initialize VPS logger
    // initVPSLogger();
    
    // Initialize web server
    initWebServer();
    
    // Post-init diagnostics
    Serial.println();
    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("SYSTEM POST-INIT STATUS");
    LOG_INFO("RTC Working: %s", isRTCWorking() ? "YES" : "NO");
    LOG_INFO("RTC Info: %s", getRTCInfo().c_str());
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("Water Algorithm:");
    LOG_INFO("  State: %s", waterAlgorithm.getStateString());
    LOG_INFO("  Daily Volume: %d / %d ml", 
             waterAlgorithm.getDailyVolume(), FILL_WATER_MAX);
    LOG_INFO("  UTC Day: %lu", waterAlgorithm.getLastResetUTCDay());
    
    if (isWiFiConnected()) {
        LOG_INFO("Dashboard: http://");
        LOG_INFO("%s", getLocalIP().toString());
    }
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("=== System initialization complete ===");
    LOG_INFO("====================================");
}

void loop() {
    // Production mode loop - full water system
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // DAILY RESTART CHECK - 24 godziny
    if (now > 86400000UL) { // 24h w ms
        Serial.println("=== DAILY RESTART: 24h uptime reached ===");
        
        if (isPumpActive()) {
            stopPump();
            Serial.println("Pump stopped before restart");
            delay(1000);
        }
        
        Serial.println("System restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    // Update water sensors every loop
    updateWaterSensors();
    waterAlgorithm.update();
    
    // ============== SYSTEM AUTO-ENABLE CHECK ==============
    // Check if 30-minute timeout has elapsed for system re-enable
    checkSystemAutoEnable();

    // Update other systems every 100ms
    if (now - lastUpdate >= 100) {
        updatePumpController();
        updateSessionManager();
        updateRateLimiter();
        updateWiFi();
        
        // ============== AUTO PUMP TRIGGER (with system disable check) ==============
        // Only trigger auto pump if:
        // - System is NOT disabled
        // - Auto mode is enabled
        // - Water level is low (shouldActivatePump)
        // - Pump is not already running
        if (!isSystemDisabled() && 
            currentPumpSettings.autoModeEnabled && 
            shouldActivatePump() && 
            !isPumpActive()) {
            LOG_INFO("");
            LOG_INFO("Auto pump triggered - water level low");    
            triggerPump(currentPumpSettings.manualCycleSeconds, "AUTO_PUMP");
        }
        
        lastUpdate = now;
    }

    delay(100);
}