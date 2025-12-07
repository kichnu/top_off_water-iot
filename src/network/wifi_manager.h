#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void initWiFi();
void updateWiFi();
bool isWiFiConnected();
String getWiFiStatus();
IPAddress getLocalIP();

#endif
