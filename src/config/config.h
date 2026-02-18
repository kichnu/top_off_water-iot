

#ifndef CONFIG_H
#define CONFIG_H

#include <IPAddress.h>

// ================= DEBUG & LOGGING CONFIG =================
// Ustaw na false jeśli masz problemy z stabilnością
#define ENABLE_FULL_LOGGING true
#define ENABLE_SERIAL_DEBUG true

// TYLKO DEKLARACJE (extern) - NIE DEFINICJE!
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* ADMIN_PASSWORD_HASH;
extern const IPAddress ALLOWED_IPS[];
extern const int ALLOWED_IPS_COUNT;
extern const char* VPS_URL;
extern const char* VPS_AUTH_TOKEN;
extern const char* DEVICE_ID;

// ================= TRUSTED PROXY (WireGuard VPS) =================
extern const IPAddress TRUSTED_PROXY_IP;

// ============== SYSTEM DISABLE/ENABLE ==============
extern bool systemDisabled;

// System state functions (declare BEFORE any mode_config includes)
void setSystemState(bool enabled);
bool isSystemDisabled();

// Global pump control (LEGACY - kept for compatibility, will be removed)
extern bool pumpGlobalEnabled;
extern unsigned long pumpDisabledTime;
extern const unsigned long PUMP_AUTO_ENABLE_MS;


// Direct manual pump safety timeout (monostable mode)
#define DIRECT_PUMP_SAFETY_TIMEOUT_S 120

// Stałe mogą być tutaj
const unsigned long SESSION_TIMEOUT_MS = 1800000; 
const unsigned long RATE_LIMIT_WINDOW_MS = 1000;
const int MAX_REQUESTS_PER_SECOND = 5;
const int MAX_FAILED_ATTEMPTS = 10;
const unsigned long BLOCK_DURATION_MS = 60000;

struct PumpSettings {
    uint16_t manualCycleSeconds = 60;
    uint16_t calibrationCycleSeconds = 30;
    float volumePerSecond = 1.0;
    bool autoModeEnabled = true;
};

extern PumpSettings currentPumpSettings;

// Functions (LEGACY - to be removed)
void checkPumpAutoEnable();
void setPumpGlobalState(bool enabled);

void loadVolumeFromNVS();
void saveVolumeToNVS();
void initNVS();

// Include credentials manager for dynamic loading
#include "credentials_manager.h"

// Dynamic credential accessors
#define WIFI_SSID_DYNAMIC getWiFiSSID()
#define WIFI_PASSWORD_DYNAMIC getWiFiPassword()
#define ADMIN_PASSWORD_HASH_DYNAMIC getAdminPasswordHash()
#define VPS_AUTH_TOKEN_DYNAMIC getVPSAuthToken()
#define DEVICE_ID_DYNAMIC getDeviceID()

#endif