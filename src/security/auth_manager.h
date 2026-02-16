#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

void initAuthManager();
bool isIPAllowed(IPAddress ip);
bool verifyPassword(const String& password);
String hashPassword(const String& password);

// Trusted proxy check for VPS proxy
bool isTrustedProxy(IPAddress ip);

// Resolve real client IP (X-Forwarded-For from trusted proxy)
IPAddress resolveClientIP(AsyncWebServerRequest* request);

#endif
