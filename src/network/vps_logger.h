#ifndef VPS_LOGGER_H
#define VPS_LOGGER_H

#include <Arduino.h>

struct PumpCycle;
class WaterAlgorithm;

void initVPSLogger();
bool logEventToVPS(const String& eventType, uint16_t volumeML, uint32_t unixTime);
bool logCycleToVPS(const PumpCycle& cycle, uint32_t unixTime);

#endif
