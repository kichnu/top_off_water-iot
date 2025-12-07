#ifndef CREDENTIALS_MANAGER_H
#define CREDENTIALS_MANAGER_H

#include <Arduino.h>

// Dynamic credentials structure
struct DynamicCredentials {
    String wifi_ssid;
    String wifi_password;
    String admin_password_hash;  // SHA-256 hex string
    String vps_auth_token;
    String vps_url;             // VPS URL
    String device_id;
    bool loaded_from_fram;
};

// Global dynamic credentials
extern DynamicCredentials dynamicCredentials;

// Credentials management functions
bool initCredentialsManager();
bool loadCredentialsFromFRAM();
bool areCredentialsLoaded();
void fallbackToHardcodedCredentials();

// Accessor functions for compatibility with existing code
const char* getWiFiSSID();
const char* getWiFiPassword();
const char* getAdminPasswordHash();
const char* getVPSAuthToken();
const char* getVPSURL();
const char* getDeviceID();

#endif
