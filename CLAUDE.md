# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C3 automated water top-off system for aquarium/plant water management. Built with PlatformIO and Arduino framework, targeting Seeed Xiao ESP32-C3. Features dual float sensor detection, intelligent pump control algorithms, web dashboard, and cloud logging.

## Build Commands

```bash
# Build and upload firmware
pio run -e seeed_xiao_esp32c3 -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Clean build
pio run -e seeed_xiao_esp32c3 -t clean

# Full rebuild
pio run -e seeed_xiao_esp32c3 -t clean && pio run -e seeed_xiao_esp32c3 -t upload
```

## Architecture

### Entry Point and Operation Modes

`src/main.cpp` implements two runtime modes determined at boot:

1. **Provisioning Mode** - Activated by holding reset button (GPIO 8) for 5+ seconds during boot. Starts WiFi AP with captive portal for initial credential setup.

2. **Production Mode** - Normal operation with water algorithm, web server, and cloud logging.

### Core Modules (src/)

```
algorithm/          Water dosing algorithm with state machine
  algorithm_config.h   Algorithm constants and state definitions
  water_algorithm.*    Main WaterAlgorithm class

config/             Configuration and credential management
  config.*             Global settings, pump parameters
  credentials_manager.*  Dynamic credential loading from FRAM

core/               Logging utilities

crypto/             AES-256 encryption for FRAM credentials
  aes.*, sha256.*, fram_encryption.*

hardware/           Hardware abstraction layer
  fram_controller.*    FRAM memory management (credentials + pump cycles)
  fram_constants.h     FRAM memory layout addresses
  hardware_pins.h      GPIO pin definitions
  pump_controller.*    Pump relay control
  rtc_controller.*     DS3231 RTC with NTP sync
  water_sensors.*      Float sensor debouncing

network/            Network connectivity
  wifi_manager.*       WiFi connection with dynamic credentials
  vps_logger.*         Cloud event logging

provisioning/       Captive portal for initial setup
  ap_core.*            Access point management
  ap_server.*          Web server for provisioning
  ap_handlers.*        HTTP request handlers
  ap_html.*            Embedded HTML pages
  prov_detector.*      Boot button detection
  wifi_scanner.*       Network scanning

security/           Authentication and rate limiting
  auth_manager.*       Password verification
  session_manager.*    Session token management
  rate_limiter.*       Request throttling

web/                Production web interface
  web_server.*         ESPAsyncWebServer setup
  web_handlers.*       API endpoint handlers
  html_pages.*         Dashboard HTML
```

### Water Algorithm State Machine

Located in `algorithm/water_algorithm.cpp`, states defined in `algorithm_config.h`:

```
STATE_IDLE → STATE_TRYB_1_WAIT → STATE_TRYB_1_DELAY → STATE_TRYB_2_PUMP
         → STATE_TRYB_2_VERIFY → STATE_TRYB_2_WAIT_GAP2 → STATE_LOGGING → STATE_IDLE
```

Key timing parameters in `algorithm_config.h`:
- `TIME_GAP_1_MAX` (2300s) - Max debounce time for both sensors
- `TIME_GAP_2_MAX` (30s) - Max wait for sensor recovery after pump
- `WATER_TRIGGER_MAX_TIME` (240s) - Max wait for sensor response to pump
- `DEBOUNCE_COUNTER_1` (4) - Required LOW readings for debounce
- `SINGLE_DOSE_VOLUME` (200ml) - Volume per pump cycle
- `FILL_WATER_MAX` (2000ml) - Daily volume limit

### FRAM Memory Layout

Unified layout defined in `fram_controller.h`:
- `0x0000-0x0017`: Magic number and version
- `0x0018-0x0417`: Encrypted credentials (WiFi, admin password, VPS token)
- `0x0500+`: ESP32 water system data (volume settings, error stats, pump cycles)

### Hardware Pin Configuration (Seeed Xiao ESP32-C3)

Defined in `hardware/hardware_pins.h`:
- GPIO 10: Pump relay (HIGH = ON)
- GPIO 3: Float sensor 1 (pull-up, active LOW)
- GPIO 4: Float sensor 2 (pull-up, active LOW)
- GPIO 6/7: I2C SDA/SCL for DS3231 RTC and FRAM
- GPIO 2: Error signal output
- GPIO 8: Reset/provisioning button

### Key API Endpoints (Production Mode)

- `POST /api/login` - Session authentication
- `GET /api/status` - System status JSON
- `POST /api/pump/normal` - Trigger manual pump cycle
- `POST /api/pump/stop` - Emergency stop
- `GET /api/pump-settings` - Get pump configuration
- `POST /api/pump-settings` - Update pump parameters
- `GET /api/get-statistics` - Error statistics
- `POST /api/reset-statistics` - Reset counters

### Dependencies (platformio.ini)

- ESPAsyncWebServer (me-no-dev)
- AsyncTCP (me-no-dev)
- ArduinoJson ^7.4.2
- RTClib ^2.1.1 (Adafruit)
- Adafruit FRAM I2C

## Important Patterns

### Credential Loading

Credentials are loaded dynamically at boot from encrypted FRAM storage. Fallback hardcoded values exist if FRAM is empty/corrupted. Access via macros in `config.h`:
- `WIFI_SSID_DYNAMIC`, `WIFI_PASSWORD_DYNAMIC`
- `ADMIN_PASSWORD_HASH_DYNAMIC`, `VPS_AUTH_TOKEN_DYNAMIC`
- `DEVICE_ID_DYNAMIC`

### System Disable/Enable

Global system control with 30-minute auto-re-enable timeout. Functions in `config.h`:
- `setSystemState(bool enabled)`
- `isSystemDisabled()`
- `checkSystemAutoEnable()`

### 24-Hour Auto-Restart

System automatically restarts after 24 hours uptime (`main.cpp:176`). Pump is safely stopped before restart.

### Timezone Handling

Configured for Poland (CET/CEST) with automatic DST. RTC stores UTC; all display times converted via `localtime_r()`. NTP servers use IP addresses for DNS independence.
