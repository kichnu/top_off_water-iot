

#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <ESPAsyncWebServer.h>
#include "../hardware/water_sensors.h"    // dla readWaterSensor1/2()
#include "../algorithm/water_algorithm.h" // dla waterAlgorithm

// Page handlers
void handleDashboard(AsyncWebServerRequest *request);
void handleLoginPage(AsyncWebServerRequest *request);

// Authentication handlers
void handleLogin(AsyncWebServerRequest *request);
void handleLogout(AsyncWebServerRequest *request);

// API handlers
void handleStatus(AsyncWebServerRequest *request);
void handlePumpNormal(AsyncWebServerRequest *request);
void handlePumpExtended(AsyncWebServerRequest *request);
void handlePumpStop(AsyncWebServerRequest *request);
void handlePumpSettings(AsyncWebServerRequest *request);

// ============== SYSTEM TOGGLE ==============
// Replaces old pump toggle with full system control
void handleSystemToggle(AsyncWebServerRequest *request);

// LEGACY - kept for compatibility, will be removed
void handlePumpToggle(AsyncWebServerRequest *request);

// Statistics handlers
void handleResetStatistics(AsyncWebServerRequest *request);
void handleGetStatistics(AsyncWebServerRequest *request);

void handleGetDailyVolume(AsyncWebServerRequest *request);
void handleResetDailyVolume(AsyncWebServerRequest *request);

// 🆕 NEW: Available Volume endpoints
void handleGetAvailableVolume(AsyncWebServerRequest* request);
void handleSetAvailableVolume(AsyncWebServerRequest* request);
void handleRefillAvailableVolume(AsyncWebServerRequest* request);

// 🆕 NEW: Fill Water Max endpoints
void handleGetFillWaterMax(AsyncWebServerRequest* request);
void handleSetFillWaterMax(AsyncWebServerRequest* request);

// Cycle History endpoint
void handleGetCycleHistory(AsyncWebServerRequest* request);

#endif