
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
    
    Serial.println();
    Serial.println("=== ESP32-C3 Water System Starting ===");
    Serial.println("Single Mode - Captive Portal Provisioning");
    
    // Check ESP32 resources
    Serial.print("ESP32 Flash size: ");
    Serial.print(ESP.getFlashChipSize());
    Serial.println(" bytes");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    Serial.println();
    Serial.println("=== CHECKING PROVISIONING BUTTON ===");
    
    // Check if provisioning button is held
    if (checkProvisioningButton()) {
        Serial.println();
        Serial.println("╔════════════════════════════════════════╗");
        Serial.println("║    ENTERING PROVISIONING MODE          ║");
        Serial.println("╚════════════════════════════════════════╝");
        Serial.println();

        Serial.println("Initializing FRAM for credential storage...");
        if (!initFRAM()) {
            Serial.println("WARNING: FRAM initialization failed!");
            Serial.println("Credentials may not be saved properly.");
            delay(2000);
        } else {
            Serial.println("✓ FRAM ready");
        }
        Serial.println();
        
        // Start Access Point
        if (!startAccessPoint()) {
            Serial.println("FATAL: Failed to start AP - halting");
            while(1) delay(1000);
        }
        
        // Start DNS Server for captive portal
        if (!startDNSServer()) {
            Serial.println("FATAL: Failed to start DNS - halting");
            while(1) delay(1000);
        }
        
        Serial.println();
        Serial.println("=== PROVISIONING MODE ACTIVE ===");
        Serial.println("Connect to WiFi network:");
        Serial.println("  SSID: ESP32-WATER-SETUP");
        Serial.println("  Password: setup12345");
        Serial.print("  URL: http://");
        Serial.println(getAPIPAddress().toString());
        Serial.println();
        
        // Start Web Server
        if (!startWebServer()) {
            Serial.println("FATAL: Failed to start web server - halting");
            while(1) delay(1000);
        }
        
        Serial.println("=== CAPTIVE PORTAL READY ===");
        Serial.println("Open a browser or wait for captive portal popup");
        Serial.println();
        
        // Enter blocking loop - never returns
        runProvisioningLoop();
        
        // This line will never be reached
    }
    
    // === PRODUCTION MODE SETUP ===
    Serial.println();
    Serial.println("=== Production Mode - Full Water System ===");

    initWaterSensors();
    initPumpController();

    initNVS();
    loadVolumeFromNVS();

    bool credentials_loaded = initCredentialsManager();
    
    Serial.print("Device ID: ");
    if (credentials_loaded) {
        Serial.println(getDeviceID());
    } else {
        Serial.println("FALLBACK_MODE");
    }
    
    Serial.println("[INIT] Initializing network...");
    initWiFi();

    LOG_INFO("[INIT] Initializing RTC...");
    initializeRTC();
    LOG_INFO("RTC Status: %s", getRTCInfo().c_str());
    
    LOG_INFO("[INIT] Waiting for RTC to stabilize...");
    delay(2000);
    
    if (!isRTCWorking()) {
        LOG_WARNING("⚠️ WARNING: RTC not working properly");
        LOG_WARNING("⚠️ Daily volume tracking may be affected");
    }
    
    LOG_INFO("[INIT] Initializing daily volume tracking...");
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
    LOG_INFO("====================================");
    LOG_INFO("SYSTEM POST-INIT STATUS");
    LOG_INFO("====================================");
    LOG_INFO("RTC Working: %s", isRTCWorking() ? "YES" : "NO");
    LOG_INFO("RTC Info: %s", getRTCInfo().c_str());
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("Water Algorithm:");
    LOG_INFO("  State: %s", waterAlgorithm.getStateString());
    LOG_INFO("  Daily Volume: %d / %d ml", 
             waterAlgorithm.getDailyVolume(), FILL_WATER_MAX);
    LOG_INFO("  UTC Day: %lu", waterAlgorithm.getLastResetUTCDay());
    LOG_INFO("====================================");
    Serial.println();
    
    Serial.println("=== System initialization complete ===");
    if (isWiFiConnected()) {
        Serial.print("Dashboard: http://");
        Serial.println(getLocalIP().toString());
    }
    Serial.print("Current time: ");
    Serial.println(getCurrentTimestamp());
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
            Serial.println("Auto pump triggered - water level low");
            triggerPump(currentPumpSettings.manualCycleSeconds, "AUTO_PUMP");
        }
        
        lastUpdate = now;
    }

    delay(100);
}