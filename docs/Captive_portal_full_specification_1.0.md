# 📐 Captive Portal - Architektura Kompletna
**Dokument referencyjny dla implementacji Captive Portal w projekcie ESP32 Water System**

---

## 🎯 Cel dokumentu
Kompendium architektury provisioning system dla ESP32-C3 z captive portal, umożliwiającym konfigurację urządzenia IoT przez interfejs webowy bez konieczności programowania przez USB/Serial.

---

## 📊 System Overview

### Tryby pracy urządzenia
```
┌─────────────────────────────────────────────────────────┐
│                     ESP32 Boot                          │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  Check Button State   │
         │  (GPIO: configurable) │
         │  Hold time: >5s       │
         └───────────┬───────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    [HELD >5s]              [NOT HELD]
         │                       │
         ▼                       ▼
┌──────────────────┐    ┌──────────────────┐
│ PROVISIONING     │    │ PRODUCTION       │
│ MODE             │    │ MODE             │
│ (blocking)       │    │ (normal loop)    │
└──────────────────┘    └──────────────────┘
         │                       │
         │                       ▼
         │              ┌──────────────────┐
         │              │ Load FRAM        │
         │              │ credentials      │
         │              └────────┬─────────┘
         │                       │
         │              ┌────────┴─────────┐
         │              │                  │
         │         [VALID]           [INVALID]
         │              │                  │
         │              ▼                  ▼
         │     ┌─────────────────┐  ┌──────────────┐
         │     │ Connect WiFi    │  │ ERROR STATE  │
         │     │ Start System    │  │ Serial warn  │
         │     └─────────────────┘  │ Halt system  │
         │                          └──────────────┘
         │
         └──────> [Manual restart required after config]
```

---

## 🔄 State Machine - Provisioning Mode

### Główny flow provisioning
```
┌──────────────────────────────────────────────────────────┐
│              PROVISIONING MODE ACTIVE                    │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  1. Initialize AP     │
         │  - SSID: ESP32-WATER  │
         │  - Pass: setup12345   │
         │  - WPA2 secured       │
         └───────────┬───────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  2. Start DNS Server  │
         │  - Captive detect     │
         │  - Hijack all domains │
         └───────────┬───────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  3. Start Web Server  │
         │  - Port 80            │
         │  - Setup dashboard    │
         └───────────┬───────────┘
                     │
                     ▼
         ┌───────────────────────┐
         │  4. Wait for config   │
         │  - Loop blocking      │
         │  - Process requests   │
         └───────────┬───────────┘
                     │
            [User submits form]
                     │
                     ▼
         ┌───────────────────────┐
         │  5. Validate input    │
         │  - Client-side JS     │
         │  - Server-side check  │
         └───────────┬───────────┘
                     │
                 [VALID]
                     │
                     ▼
         ┌───────────────────────┐
         │  6. Run tests         │
         │  - WiFi connect       │
         │  - DNS resolve        │
         │  - HTTP test          │
         │  - WebSocket test     │
         │  - VPS auth test      │
         │  [Log buffer → UI]    │
         └───────────┬───────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    [ALL PASS]              [ANY FAIL]
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌─────────────────┐
│ 7a. Save FRAM   │    │ 7b. Show error  │
│ - Encrypt data  │    │ - Keep AP mode  │
│ - Write block   │    │ - Retry option  │
└────────┬────────┘    └─────────────────┘
         │
         ▼
┌─────────────────┐
│ 8. Show success │
│ - Instruct user │
│ - Manual restart│
└─────────────────┘
```

---

## 🗄️ FRAM Memory Layout

### Opcja A: Wykorzystanie istniejącego layoutu
**Lokalizacja w kodzie:** `src/hardware/fram_controller.cpp/h`

**Modyfikacje wymagane:**
1. Rozszerzenie struktury `credentials_t` o nowe pola:
   - `char vps_url[128]` (obecnie brak, hardcoded w config.h)
   - `char admin_password_hash[32]` (SHA256 hash)
   - `uint8_t whitelist_ips[MAX_WHITELIST][4]` (tablica IP)
   - `uint8_t whitelist_count`

2. Aktualizacja wersji credentials:
   - Obecna: `0x0002`
   - Nowa: `0x0003`

3. Usunięcie z FRAM (już niepotrzebne):
   - WiFi fail counter (rezygnacja z auto-fallback)
   - LED pattern state

**Struktura do zachowania:**
- Magic number validation
- AES-256 encryption (z `fram_encryption.h`)
- CRC32 checksum
- Istniejące pola: device_name, wifi_ssid, wifi_password, vps_token

---

### Opcja B: Propozycja nowego layoutu od zera

```
FRAM ADDRESS MAP (32KB total)
═══════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────┐
│ BLOCK 1: CREDENTIALS (0x0000 - 0x01FF, 512 bytes)      │
│ [ENCRYPTED with AES-256]                                │
├─────────────────────────────────────────────────────────┤
│ 0x0000 - 0x0003  Magic + Version                        │
│                  uint32_t: 0x45535032 + 0x0003          │
│                                                          │
│ 0x0004 - 0x0007  CRC32 Checksum                         │
│                  uint32_t: całego bloku                 │
│                                                          │
│ 0x0008 - 0x0027  Device Name (32 bytes)                 │
│                  char[32]: null-terminated              │
│                  Pattern: [a-zA-Z0-9_-]                 │
│                                                          │
│ 0x0028 - 0x0047  WiFi SSID (32 bytes)                   │
│                  char[32]: null-terminated              │
│                                                          │
│ 0x0048 - 0x0087  WiFi Password (64 bytes)               │
│                  char[64]: null-terminated              │
│                                                          │
│ 0x0088 - 0x00A7  Admin Password Hash (32 bytes)         │
│                  uint8_t[32]: SHA256 hash               │
│                                                          │
│ 0x00A8 - 0x00E7  VPS Token (64 bytes)                   │
│                  char[64]: null-terminated              │
│                                                          │
│ 0x00E8 - 0x0167  VPS URL (128 bytes)                    │
│                  char[128]: null-terminated             │
│                  Format: https://domain.com:port        │
│                                                          │
│ 0x0168 - 0x0168  Whitelist Count (1 byte)               │
│                  uint8_t: 0-10                          │
│                                                          │
│ 0x0169 - 0x0190  IP Whitelist (40 bytes)                │
│                  uint8_t[10][4]: max 10 IPs             │
│                  Format: [192][168][1][100]             │
│                                                          │
│ 0x0191 - 0x01FF  Reserved (111 bytes)                   │
│                  Padding + future expansion             │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ BLOCK 2: SYSTEM STATE (0x0200 - 0x02FF, 256 bytes)     │
│ [UNENCRYPTED - operational data]                        │
├─────────────────────────────────────────────────────────┤
│ 0x0200 - 0x0207  Last Boot Timestamp                    │
│                  uint64_t: Unix epoch                   │
│                                                          │
│ 0x0208 - 0x020B  Boot Count                             │
│                  uint32_t: incremental counter          │
│                                                          │
│ 0x020C - 0x020F  Last WiFi Connect Time (ms)            │
│                  uint32_t: connection duration          │
│                                                          │
│ 0x0210 - 0x0213  Last VPS Connect Time (ms)             │
│                  uint32_t: connection duration          │
│                                                          │
│ 0x0214 - 0x0217  Production Mode Uptime (s)             │
│                  uint32_t: seconds since last boot      │
│                                                          │
│ 0x0218 - 0x02FF  Reserved (231 bytes)                   │
│                  Logs, diagnostics, future use          │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ BLOCK 3+: APPLICATION DATA (0x0300 - 0x7FFF)            │
│ [Existing water system data - PRESERVE]                 │
├─────────────────────────────────────────────────────────┤
│ 0x0300+          Dosing logs                            │
│                  Water usage stats                      │
│                  Error history                          │
│                  ... (istniejące dane aplikacji)        │
└─────────────────────────────────────────────────────────┘
```

**Kluczowe zmiany w kodzie:**
1. Funkcja `verifyCredentialsInFRAM()` - sprawdzanie magic + version
2. Funkcja `readCredentialsFromFRAM()` - dekrypcja + walidacja CRC
3. Funkcja `writeCredentialsToFRAM()` - szyfrowanie + zapis
4. Dodanie `validateCredentialsStructure()` - format checking
5. Migration handler dla starszych wersji (opcjonalnie)

---

## 🌐 API Endpoints - Provisioning Web Server

### Base Configuration
```
Server: ESP32 Async Web Server
Port: 80 (HTTP only)
Base URL: http://192.168.4.1
DNS: All domains → 192.168.4.1
```

---

### Endpoint 1: Landing Page
```
GET /
─────────────────────────────────────────────────
Response: HTML
Status: 200 OK
Content-Type: text/html; charset=utf-8

Purpose:
- Main setup page
- Mobile-first responsive design
- WiFi network scanner UI
- Form validation JavaScript
- Warning banner (no HTTPS)
- Serial log viewer (auto-refresh 5-10s)

Modifications needed:
- Nowy plik: src/ap_mode/ap_html.cpp/h
- HTML template z embedded CSS/JS
- Formularz z polami: device_name, ssid, password, 
  admin_password, vps_token, vps_url, whitelist_ips
```

---

### Endpoint 2: WiFi Network Scan
```
GET /api/scan
─────────────────────────────────────────────────
Response: JSON
Status: 200 OK
Content-Type: application/json

{
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -45,
      "encryption": 3,
      "channel": 6
    },
    ...
  ],
  "count": 12
}

Purpose:
- Populate SSID dropdown
- Show signal strength icons
- Filter duplicates (keep strongest RSSI)

Implementation:
- WiFi.scanNetworks(false, true) - async scan
- Sort by RSSI descending
- Remove duplicates
- Return max 20 networks

Modifications needed:
- Nowy plik: src/ap_mode/wifi_scanner.cpp/h
- Async scan handler
- JSON serialization (ArduinoJson library)
```

---

### Endpoint 3: Configuration Submit
```
POST /api/configure
─────────────────────────────────────────────────
Request: application/json
{
  "device_name": "water-pump-01",
  "wifi_ssid": "MyNetwork",
  "wifi_password": "secret123",
  "admin_password": "admin456",
  "vps_token": "abcd1234...",
  "vps_url": "https://vps.example.com:8443",
  "whitelist_ips": ["192.168.1.100", "192.168.1.101"]
}

Response: JSON
Status: 200 OK / 400 Bad Request
{
  "success": true,
  "test_results": {
    "wifi_connect": { "status": "ok", "time_ms": 3245 },
    "dns_resolve": { "status": "ok", "ip": "1.2.3.4" },
    "http_test": { "status": "ok", "code": 200 },
    "websocket_test": { "status": "ok" },
    "vps_auth": { "status": "ok" }
  },
  "message": "Configuration saved. Please restart device manually."
}

Purpose:
- Validate all inputs (server-side)
- Run connection tests in sequence
- Buffer serial logs for UI display
- Save to FRAM if all tests pass
- Return detailed test results

Modifications needed:
- Nowy plik: src/ap_mode/ap_handlers.cpp/h
- Validation functions:
  * validateDeviceName() - regex [a-zA-Z0-9_-]{3,32}
  * validateSSID() - length 1-32
  * validatePassword() - length 8-64
  * validateURL() - format check
  * validateIP() - format check
- Test sequence runner
- FRAM write wrapper
```

---

### Endpoint 4: Test Status (Polling)
```
GET /api/test-status
─────────────────────────────────────────────────
Response: JSON
Status: 200 OK
{
  "testing": true,
  "current_step": "wifi_connect",
  "logs": [
    "[12:34:56] Starting WiFi connection...",
    "[12:34:58] Connected to MyNetwork (RSSI: -45)",
    "[12:34:59] Testing DNS resolution..."
  ],
  "progress": 40
}

Purpose:
- Live test progress updates
- Serial log streaming to UI
- Client polls every 2-3 seconds during test

Implementation:
- Global log buffer (circular, 50 entries)
- Thread-safe access (mutex)
- Timestamp each entry
- Progress percentage calculation

Modifications needed:
- Log buffer w RAM (struktura danych)
- Mutex dla thread safety
- Funkcje: addLog(), getRecentLogs()
```

---

### Endpoint 5: Get Logs
```
GET /api/logs
─────────────────────────────────────────────────
Response: JSON
Status: 200 OK
{
  "logs": [
    { "timestamp": "12:34:56", "level": "INFO", "message": "WiFi connected" },
    { "timestamp": "12:34:57", "level": "ERROR", "message": "DNS failed" }
  ],
  "count": 45
}

Purpose:
- Retrieve all buffered logs
- Auto-refresh every 5-10s via JavaScript
- Display in scrollable log viewer on UI

Modifications needed:
- Integracja z istniejącym logger system (jeśli jest)
- Formatowanie timestamp
- Level filtering (INFO/WARN/ERROR)
```

---

### Endpoint 6: Captive Portal Detection (iOS)
```
GET /hotspot-detect.html
GET /captive
─────────────────────────────────────────────────
Response: 302 Redirect / 200 OK
Location: http://192.168.4.1/

Purpose:
- iOS captive portal auto-detection
- Redirect to main setup page

Implementation:
- Return "Success" with redirect header
- Or return HTML with <meta refresh>
```

---

### Endpoint 7: Captive Portal Detection (Android)
```
GET /generate_204
─────────────────────────────────────────────────
Response: 302 Redirect
Status: 302 Found
Location: http://192.168.4.1/

Purpose:
- Android captive portal detection
- Expects 204 No Content normally
- Return redirect to trigger portal

Implementation:
- Return 302 instead of 204
- Force captive notification
```

---

### Endpoint 8: Captive Portal Detection (Windows)
```
GET /connecttest.txt
GET /redirect
─────────────────────────────────────────────────
Response: 302 Redirect
Location: http://192.168.4.1/

Purpose:
- Windows 10/11 captive detection
- Redirect to setup page
```

---

### Summary: DNS Hijack Implementation
```
DNSServer configuration:
- Listen on port 53
- Respond to ALL domains with 192.168.4.1
- setErrorReplyCode(DNSReplyCode::NoError)
- processNextRequest() w loop()

Modifications needed:
- Dodanie DNSServer object w ap_portal.cpp
- Konfiguracja w startAPMode()
- Process w loop()
```

---

## 🧪 Test Connection Sequence

### Kolejność testów (zgodna z wprowadzaniem danych)

```
TEST SEQUENCE FLOW
═══════════════════════════════════════════════════════════

1. WIFI CONNECTION TEST
   ├─ Disconnect current WiFi (if any)
   ├─ Set credentials from form
   ├─ Retry logic:
   │  ├─ Attempt 1: timeout 5000ms
   │  ├─ Attempt 2: timeout 5000ms
   │  └─ Attempt 3: timeout 5000ms
   ├─ Check: WiFi.status() == WL_CONNECTED
   ├─ Log: RSSI, IP address, gateway
   └─ FAIL → abort, return error

         ↓ [PASS]

2. DNS RESOLUTION TEST
   ├─ Parse hostname from vps_url
   ├─ WiFi.hostByName(hostname, ip)
   ├─ Timeout: 10000ms
   ├─ Check: ip != INADDR_NONE
   ├─ Log: resolved IP address
   └─ FAIL → abort, return error

         ↓ [PASS]

3. HTTP CONNECTIVITY TEST
   ├─ HTTPClient http
   ├─ http.begin(vps_url)
   ├─ http.setTimeout(10000)
   ├─ int httpCode = http.GET()
   ├─ Check: httpCode == 200 (lub 301/302 OK)
   ├─ Log: HTTP status code, response time
   └─ FAIL → abort, return error

         ↓ [PASS]

4. WEBSOCKET HANDSHAKE TEST
   ├─ WebSocketsClient ws
   ├─ ws.begin(hostname, port, "/ws/path")
   ├─ ws.setTimeout(15000)
   ├─ Check: connection established
   ├─ Log: handshake success/fail
   ├─ ws.disconnect()
   └─ FAIL → abort, return error

         ↓ [PASS]

5. VPS AUTHENTICATION TEST
   ├─ Send auth packet with vps_token
   ├─ Format: {"action":"auth","token":"..."}
   ├─ Wait for response: {"status":"ok"}
   ├─ Timeout: 10000ms
   ├─ Check: received valid auth response
   ├─ Log: auth status
   └─ FAIL → abort, return error

         ↓ [ALL PASS]

6. SAVE TO FRAM
   ├─ Hash admin_password → SHA256
   ├─ Parse whitelist_ips → uint8_t[10][4]
   ├─ Build credentials_t struct
   ├─ Encrypt with AES-256
   ├─ Calculate CRC32
   ├─ Write to FRAM address 0x0000
   ├─ Verify write (read back + compare)
   └─ Return success

         ↓

7. DISPLAY SUCCESS MESSAGE
   ├─ Show: "Configuration saved successfully!"
   ├─ Show: "Please restart device manually"
   ├─ Show: "Remove power, wait 5s, reconnect"
   └─ Keep AP active (no auto-restart)
```

---

### Implementacja testów - modyfikacje kodu

**Nowy plik:** `src/ap_mode/connection_tester.cpp/h`

**Funkcje do implementacji:**
1. `bool testWiFiConnection(const char* ssid, const char* password, String& log)`
2. `bool testDNSResolution(const char* url, IPAddress& resolvedIP, String& log)`
3. `bool testHTTPConnectivity(const char* url, String& log)`
4. `bool testWebSocketHandshake(const char* url, String& log)`
5. `bool testVPSAuthentication(const char* url, const char* token, String& log)`
6. `bool runAllTests(credentials_t* creds, TestResults& results)`

**Log buffer:**
- Struktura: `std::vector<String> logBuffer` (max 50 entries)
- Mutex: `SemaphoreHandle_t logMutex`
- Funkcje: `addLog(String message)`, `clearLogs()`, `getLogsJSON()`

**Integracja z UI:**
- JavaScript polling `/api/test-status` co 2s
- Display progress bar (0-100%)
- Scrollable log viewer (auto-scroll bottom)
- Disable form submit during tests

---

## 📁 Struktura plików - Modyfikacje projektu

### Nowe moduły do dodania

```
src/
├── ap_mode/                      [NEW DIRECTORY]
│   ├── ap_portal.cpp             [NEW] Main AP mode controller
│   ├── ap_portal.h               [NEW] 
│   ├── ap_server.cpp             [NEW] AsyncWebServer setup
│   ├── ap_server.h               [NEW]
│   ├── ap_handlers.cpp           [NEW] Request handlers (POST/GET)
│   ├── ap_handlers.h             [NEW]
│   ├── ap_html.cpp               [NEW] HTML pages (embedded)
│   ├── ap_html.h                 [NEW]
│   ├── wifi_scanner.cpp          [NEW] Network scanning
│   ├── wifi_scanner.h            [NEW]
│   ├── connection_tester.cpp     [NEW] Test sequence runner
│   ├── connection_tester.h       [NEW]
│   ├── captive_dns.cpp           [NEW] DNS server hijack
│   ├── captive_dns.h             [NEW]
│   └── log_buffer.cpp            [NEW] Serial log buffering
│       └── log_buffer.h          [NEW]
│
├── utils/                        [NEW DIRECTORY]
│   ├── credentials_validator.cpp [NEW] Input validation
│   ├── credentials_validator.h   [NEW]
│   ├── ip_whitelist.cpp          [NEW] IP parsing/checking
│   └── ip_whitelist.h            [NEW]
```

---

### Istniejące pliki do modyfikacji

```
src/
├── main.cpp                      [MODIFY]
│   ├─ Dodać: Button GPIO check w setup()
│   ├─ Dodać: Boot decision logic
│   ├─ Dodać: Call startProvisioningMode() lub startProductionMode()
│   └─ Usunąć: Dual-mode logic (jeśli istnieje)
│
├── hardware/
│   ├── fram_controller.cpp       [MODIFY]
│   │   ├─ Rozszerzyć: credentials_t struct
│   │   ├─ Dodać: vps_url field (128 bytes)
│   │   ├─ Dodać: admin_password_hash field (32 bytes)
│   │   ├─ Dodać: whitelist_ips array
│   │   ├─ Dodać: whitelist_count field
│   │   ├─ Zmienić: CREDENTIALS_VERSION → 0x0003
│   │   ├─ Usunąć: WiFi fail counter
│   │   └─ Dodać: verifyCredentialsStructure()
│   │
│   └── fram_controller.h         [MODIFY]
│       └─ Update struct definition
│
├── config/
│   ├── config.cpp                [MODIFY]
│   │   ├─ Usunąć: Hardcoded VPS_URL
│   │   ├─ Usunąć: WiFi credentials (teraz w FRAM)
│   │   ├─ Dodać: #define PROVISIONING_BUTTON_PIN (configurable)
│   │   ├─ Dodać: #define BUTTON_HOLD_TIME_MS 5000
│   │   ├─ Dodać: #define AP_SSID "ESP32-WATER-SETUP"
│   │   ├─ Dodać: #define AP_PASSWORD "setup12345"
│   │   └─ Dodać: #define TEST_TIMEOUT_MS 10000
│   │
│   └── config.h                  [MODIFY]
│       └─ Export new constants
│
├── network/
│   ├── web_server.cpp            [MODIFY]
│   │   ├─ Dodać: IP whitelist checking middleware
│   │   ├─ Dodać: Endpoint POST /api/enter-provisioning (opcjonalnie)
│   │   └─ Zabezpieczenie: Check IP before serving dashboard
│   │
│   └── wifi_manager.cpp          [MODIFY - jeśli istnieje]
│       ├─ Usunąć: Auto-reconnect logic z retry counter
│       └─ Uproszczenie: Single connect attempt w produkcji
│
└── utils/
    └── logger.cpp                [MODIFY - jeśli istnieje]
        ├─ Dodać: Buffer mode (RAM storage)
        ├─ Dodać: getBufferedLogs() funkcja
        └─ Integracja z log_buffer.cpp
```

---

### PlatformIO Configuration Changes

**platformio.ini** - dodać/zmodyfikować:

```ini
[MODIFY]
lib_deps =
    existing_libs...
    ESPAsyncWebServer              [ADD] For async web server
    AsyncTCP                       [ADD] TCP async lib
    ArduinoJson                    [ADD] JSON parsing/generation
    WebSockets                     [ADD] WebSocket client test
    DNSServer                      [ADD] Captive portal DNS

build_flags =
    ${env.build_flags}
    -DPROVISIONING_BUTTON_PIN=10   [ADD] Configurable GPIO
    -DLOG_BUFFER_SIZE=50           [ADD] Serial log buffer size
    -DTEST_TIMEOUT_MS=10000        [ADD] Connection test timeout

[REMOVE]
; Usunąć dual-mode config jeśli istnieje
```

---

## 🔧 Boot Flow - Decision Tree (szczegółowy)

```
═══════════════════════════════════════════════════════════════
                    ESP32 BOOT SEQUENCE
═══════════════════════════════════════════════════════════════

void setup() {
    │
    ├─ Serial.begin(115200)
    ├─ initHardware()              // RTC, FRAM, sensors
    ├─ initFRAM()                  // I2C bus init
    │
    ├─ pinMode(PROVISIONING_BUTTON_PIN, INPUT_PULLUP)
    │
    ├─ unsigned long pressStart = millis()
    ├─ bool buttonHeld = false
    │
    ├─ while (millis() - pressStart < 100) {  // 100ms debounce
    │      if (digitalRead(PROVISIONING_BUTTON_PIN) == LOW) {
    │          pressStart = millis()
    │          buttonHeld = true
    │          break
    │      }
    │  }
    │
    ├─ if (buttonHeld) {
    │      while (digitalRead(PROVISIONING_BUTTON_PIN) == LOW) {
    │          if (millis() - pressStart >= BUTTON_HOLD_TIME_MS) {
    │              // Button held >5s
    │              Serial.println("PROVISIONING MODE TRIGGERED")
    │              startProvisioningMode()
    │              // BLOCKING - never returns until configured
    │          }
    │          delay(50)  // Check every 50ms
    │      }
    │  }
    │
    └─ // Button not held → PRODUCTION MODE
       │
       ├─ bool credsValid = verifyCredentialsInFRAM()
       │
       ├─ if (!credsValid) {
       │      Serial.println("ERROR: No valid credentials in FRAM!")
       │      Serial.println("HALT: Press and hold button >5s to configure")
       │      while(1) {
       │          delay(1000)  // Infinite halt
       │      }
       │  }
       │
       ├─ credentials_t creds = readCredentialsFromFRAM()
       │
       ├─ WiFi.begin(creds.wifi_ssid, creds.wifi_password)
       │
       ├─ int attempts = 0
       ├─ while (WiFi.status() != WL_CONNECTED && attempts < 3) {
       │      delay(5000)
       │      attempts++
       │  }
       │
       ├─ if (WiFi.status() != WL_CONNECTED) {
       │      Serial.println("ERROR: WiFi connection failed!")
       │      Serial.println("HALT: Check credentials or reconfigure")
       │      while(1) {
       │          delay(1000)  // Infinite halt
       │      }
       │  }
       │
       ├─ Serial.println("WiFi connected successfully")
       ├─ Serial.print("IP: ")
       ├─ Serial.println(WiFi.localIP())
       │
       ├─ startProductionMode(&creds)
       │
       └─ // Continue to loop()
}

void loop() {
    // Production mode - normal water system operations
    // Provisioning mode never reaches here (blocking)
}
```

---

## 🔐 Security - Implementacja zabezpieczeń

### 1. AP Mode Security
```
SSID: ESP32-WATER-SETUP
Password: setup12345
Encryption: WPA2-PSK

Zagrożenie: Ktoś w zasięgu może się połączyć
Mitigacja:
├─ Session timeout: 10 minut bez aktywności
├─ Rate limiting: max 5 request/minute per IP
├─ CAPTCHA (opcjonalnie): po 3 failed submits
└─ Strong password required dla admin_password field
```

**Modyfikacje:**
- Dodać session manager w `ap_server.cpp`
- Middleware rate limiter (tracking per client IP)
- Admin password validation: min 8 chars, mix of chars/numbers

---

### 2. HTTPS Warning UI
```
Banner na górze setup page:
┌───────────────────────────────────────────────┐
│ ⚠️ WARNING: This connection is not secure    │
│                                               │
│ Data is transmitted unencrypted. Only use    │
│ this setup mode in trusted environments.     │
│                                               │
│ Do not enter sensitive passwords that you    │
│ use on other services.                       │
└───────────────────────────────────────────────┘
```

**Modyfikacje:**
- HTML banner w `ap_html.cpp`
- CSS: orange/red background, icon, clear text

---

### 3. Password Handling
```
Admin Password:
├─ Input: type="password" with show/hide toggle
├─ Client: SHA256 hash przed submit (crypto-js)
├─ Server: Re-hash received hash → SHA256(SHA256(password))
└─ FRAM: Store double-hashed value

VPS Token:
├─ Input: type="password" (lub text with warning)
└─ FRAM: Store encrypted (AES-256)

WiFi Password:
├─ Input: type="password" with show/hide toggle
└─ FRAM: Store encrypted (AES-256)
```

**Modyfikacje:**
- JavaScript: crypto-js library dla client-side hashing
- Server: `credentials_validator.cpp` - password strength check
- FRAM: Istniejąca AES-256 encryption z `fram_encryption.h`

---

### 4. IP Whitelist (Production Mode)
```
Dashboard access check:
├─ Middleware w web_server.cpp
├─ Request → Extract client IP
├─ if (whitelist_count == 0) → ALLOW ALL
├─ else → Check IP in whitelist array
└─ if NOT whitelisted → 403 Forbidden

Exceptions:
├─ /api/system-info → Always accessible (read-only)
└─ Static assets (CSS/JS) → Cached, accessible
```

**Modyfikacje:**
- Nowy plik: `src/utils/ip_whitelist.cpp/h`
- Funkcje: `isIPWhitelisted(IPAddress ip)`, `parseIPString(const char* ip)`
- Middleware w `web_server.cpp` - call before route handlers

---

## 🎨 UI/UX - Frontend Guidelines

### Mobile-First Design
```
Viewport: <meta name="viewport" content="width=device-width, initial-scale=1.0">
CSS Framework: Inline (no external dependencies)
Layout: Single-column, max-width 600px, centered
Forms: Large touch targets (min 44x44px)
Fonts: System fonts (no web fonts)
```

**Sekcje UI:**

1. **Header**
   - Logo/Title: "ESP32 Water System Setup"
   - Warning banner (security)

2. **WiFi Scanner**
   - Scan button: "Scan for Networks"
   - Loading spinner podczas scan
   - Dropdown/List: SSID + signal icon (bars)
   - Refresh button

3. **Configuration Form**
   - Device Name: text input, pattern validation live
   - WiFi SSID: dropdown (from scan) + manual input option
   - WiFi Password: password input with show/hide toggle
   - Admin Password: password input with strength meter
   - VPS Token: password input
   - VPS URL: text input, placeholder: "https://..."
   - IP Whitelist: dynamic list (add/remove), format: "192.168.1.100"
   - Submit button: "Save & Test Configuration"

4. **Test Progress**
   - Modal/Overlay during testing
   - Progress bar: 0-100%
   - Current step indicator
   - Live log viewer (scrollable, monospace font)
   - Auto-refresh: 2s interval via AJAX

5. **Success/Error Messages**
   - Success: Green banner, manual restart instructions
   - Error: Red banner, specific error message, retry button

**Modyfikacje:**
- HTML template w `ap_html.cpp` - embedded string
- CSS: inline `<style>` tag (no external file)
- JavaScript: inline `<script>` (no external dependencies)
- AJAX: vanilla JS `fetch()` API (no jQuery)

---

## 📝 Serial Log Buffering - Architektura

### Log Buffer Structure
```cpp
// Struktura pojedynczego log entry
struct LogEntry {
    uint32_t timestamp;    // millis() since boot
    uint8_t level;         // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    char message[128];     // Treść wiadomości
};

// Global buffer
#define LOG_BUFFER_SIZE 50
LogEntry logBuffer[LOG_BUFFER_SIZE];
uint8_t logBufferIndex = 0;     // Circular buffer index
uint8_t logBufferCount = 0;     // Actual entries count
SemaphoreHandle_t logMutex;     // Thread safety
```

### Funkcje do implementacji

**Nowy plik:** `src/ap_mode/log_buffer.cpp/h`

```
void initLogBuffer() {
    ├─ Create mutex
    └─ Clear buffer
}

void addLog(uint8_t level, const char* message) {
    ├─ Take mutex
    ├─ logBuffer[logBufferIndex] = {millis(), level, message}
    ├─ logBufferIndex = (logBufferIndex + 1) % LOG_BUFFER_SIZE
    ├─ logBufferCount = min(logBufferCount + 1, LOG_BUFFER_SIZE)
    └─ Release mutex
}

String getLogsJSON() {
    ├─ Take mutex
    ├─ Build JSON array from buffer
    ├─ Format: [{"time":"12:34:56","level":"INFO","msg":"..."},...]
    ├─ Release mutex
    └─ Return JSON string
}

void clearLogBuffer() {
    ├─ Take mutex
    ├─ logBufferIndex = 0
    ├─ logBufferCount = 0
    └─ Release mutex
}
```

### Integracja z Serial.print

**Modyfikacja w miejscach logowania:**
```
Przed:
    Serial.println("WiFi connected");

Po:
    addLog(LOG_INFO, "WiFi connected");
    Serial.println("WiFi connected");  // Opcjonalnie keep Serial
```

**Macro dla łatwiejszego użycia:**
```cpp
#define LOG_DEBUG(msg) addLog(0, msg)
#define LOG_INFO(msg)  addLog(1, msg)
#define LOG_WARN(msg)  addLog(2, msg)
#define LOG_ERROR(msg) addLog(3, msg)
```

### UI Auto-refresh Mechanism
```javascript
// W ap_html.cpp - embedded JavaScript

let logRefreshInterval = null;

function startLogRefresh() {
    logRefreshInterval = setInterval(async () => {
        try {
            const response = await fetch('/api/logs');
            const data = await response.json();
            updateLogViewer(data.logs);
        } catch (error) {
            console.error('Log fetch error:', error);
        }
    }, 5000);  // 5s refresh
}

function updateLogViewer(logs) {
    const logContainer = document.getElementById('log-viewer');
    logContainer.innerHTML = '';
    logs.forEach(log => {
        const div = document.createElement('div');
        div.className = `log-entry log-${log.level}`;
        div.textContent = `[${log.time}] ${log.msg}`;
        logContainer.appendChild(div);
    });
    // Auto-scroll to bottom
    logContainer.scrollTop = logContainer.scrollHeight;
}
```

---

## 🚀 Implementation Roadmap

### Faza 1: Podstawowa infrastruktura (Priorytet 1)
```
1.1 Struktura katalogów
    ├─ Utworzyć src/ap_mode/
    ├─ Utworzyć src/utils/
    └─ Dodać do platformio.ini lib_deps

1.2 FRAM layout
    ├─ Rozszerzyć credentials_t struct
    ├─ Zmienić CREDENTIALS_VERSION → 0x0003
    ├─ Dodać verifyCredentialsStructure()
    └─ Testy read/write nowego layoutu

1.3 Button detection
    ├─ Dodać GPIO config w config.h
    ├─ Implementować debouncing
    ├─ Implementować hold time check
    └─ Testy physical button

1.4 Boot decision logic
    ├─ Modyfikacja main.cpp setup()
    ├─ verifyCredentialsInFRAM()
    ├─ Branch: Provisioning vs Production
    └─ Halt on invalid credentials (production)
```

---

### Faza 2: AP Mode Core (Priorytet 1)
```
2.1 AP Portal podstawa
    ├─ ap_portal.cpp/h - main controller
    ├─ WiFi.softAP() configuration
    ├─ Loop blocking mechanism
    └─ Exit condition (after save)

2.2 DNS Server (Captive Portal)
    ├─ captive_dns.cpp/h
    ├─ DNSServer init & config
    ├─ Hijack all domains → 192.168.4.1
    └─ Captive portal endpoints (iOS/Android/Windows)

2.3 Web Server
    ├─ ap_server.cpp/h
    ├─ AsyncWebServer init (port 80)
    ├─ Route handlers skeleton
    └─ Basic HTML response test
```

---

### Faza 3: Frontend & WiFi Scanner (Priorytet 2)
```
3.1 HTML/CSS/JS
    ├─ ap_html.cpp/h
    ├─ Mobile-first layout
    ├─ Form HTML structure
    ├─ Warning banner
    └─ Embedded CSS styling

3.2 WiFi Scanner
    ├─ wifi_scanner.cpp/h
    ├─ Async WiFi.scanNetworks()
    ├─ JSON response builder
    ├─ Frontend dropdown population
    └─ Signal strength icons

3.3 Form Validation
    ├─ credentials_validator.cpp/h
    ├─ Client-side JS validation (live)
    ├─ Server-side validation (on submit)
    └─ Error messages display
```

---

### Faza 4: Connection Testing (Priorytet 2)
```
4.1 Test Runner
    ├─ connection_tester.cpp/h
    ├─ WiFi connection test (3 retries)
    ├─ DNS resolution test
    ├─ HTTP connectivity test
    ├─ WebSocket handshake test
    └─ VPS authentication test

4.2 Log Buffering
    ├─ log_buffer.cpp/h
    ├─ Circular buffer implementation
    ├─ Thread-safe mutex
    ├─ JSON serialization
    └─ Integration z Serial.print

4.3 Progress UI
    ├─ /api/test-status endpoint
    ├─ Frontend polling mechanism
    ├─ Progress bar updates
    └─ Live log viewer updates
```

---

### Faza 5: Configuration Save & Production (Priorytet 2)
```
5.1 FRAM Write
    ├─ Build credentials_t from form
    ├─ Hash admin_password (double SHA256)
    ├─ Parse IP whitelist strings
    ├─ AES-256 encryption (existing)
    ├─ CRC32 calculation
    ├─ FRAM write & verify
    └─ Error handling

5.2 Production Mode
    ├─ startProductionMode() w main.cpp
    ├─ Read credentials from FRAM
    ├─ WiFi connect (single attempt)
    ├─ VPS WebSocket init
    └─ Start normal water system loop

5.3 IP Whitelist
    ├─ ip_whitelist.cpp/h
    ├─ Parse IP strings to uint8_t[4]
    ├─ isIPWhitelisted() check
    ├─ Middleware w web_server.cpp
    └─ 403 Forbidden response
```

---

### Faza 6: Polish & Testing (Priorytet 3)
```
6.1 Error Recovery
    ├─ Watchdog timer config
    ├─ AP mode crash handling
    ├─ Serial diagnostics
    └─ Recovery instructions

6.2 UI/UX Refinements
    ├─ Loading spinners
    ├─ Success animations
    ├─ Better error messages
    ├─ Accessibility (ARIA labels)
    └─ Mobile touch optimization

6.3 Security Hardening
    ├─ Rate limiting middleware
    ├─ Session timeout
    ├─ Input sanitization
    ├─ XSS prevention
    └─ Password strength enforcement

6.4 Comprehensive Testing
    ├─ Boot without credentials → AP auto
    ├─ Button >5s → AP entry
    ├─ iOS captive portal popup
    ├─ Android captive notification
    ├─ WiFi scan functionality
    ├─ All test sequence steps
    ├─ FRAM write/read cycles
    ├─ Production mode normal operation
    ├─ IP whitelist enforcement
    └─ Error scenarios handling
```

---

## 📊 Data Flow Diagram

```
═══════════════════════════════════════════════════════════════
                    PROVISIONING MODE DATA FLOW
═══════════════════════════════════════════════════════════════

┌─────────────┐
│   User      │
│  (Mobile/   │
│   Laptop)   │
└──────┬──────┘
       │
       │ 1. Connect to AP
       │    SSID: ESP32-WATER-SETUP
       │    Pass: setup12345
       │
       ▼
┌──────────────────────────────────────────┐
│         ESP32 Captive Portal             │
│  ┌────────────────────────────────────┐  │
│  │      DNS Server (port 53)          │  │
│  │  All domains → 192.168.4.1         │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │   Web Server (port 80)             │  │
│  │                                    │  │
│  │  GET /                             │  │  2. Browser opens
│  │  ├─> Return setup HTML            │◄─┼──── captive portal
│  │  │                                 │  │     automatically
│  │  GET /api/scan                     │  │
│  │  ├─> WiFi.scanNetworks()          │  │
│  │  └─> Return JSON network list     │  │
│  │                                    │  │
│  │  POST /api/configure               │  │  3. User submits
│  │  ├─> Validate input                │◄─┼──── configuration
│  │  ├─> Run test sequence             │  │     form
│  │  │    ├─ WiFi connect              │  │
│  │  │    ├─ DNS resolve               │  │
│  │  │    ├─ HTTP test                 │  │
│  │  │    ├─ WebSocket test            │  │
│  │  │    └─ VPS auth test             │  │
│  │  ├─> Save to FRAM                  │  │
│  │  └─> Return success JSON           │  │
│  │                                    │  │
│  │  GET /api/logs                     │  │  4. UI polls for
│  │  └─> Return buffered logs          │◄─┼──── test progress
│  │                                    │  │     logs (5s interval)
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │      Log Buffer (RAM)              │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ [12:34:56] WiFi connecting.. │  │  │
│  │  │ [12:34:58] WiFi connected OK │  │  │
│  │  │ [12:34:59] Testing DNS...    │  │  │
│  │  │ [12:35:01] DNS resolved OK   │  │  │
│  │  │ [12:35:02] HTTP test OK      │  │  │
│  │  │ ...                          │  │  │
│  │  └──────────────────────────────┘  │  │
│  │  Circular buffer (50 entries)     │  │
│  └────────────────────────────────────┘  │
│                                          │
│  ┌────────────────────────────────────┐  │
│  │      FRAM (I2C)                    │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ 0x0000: Magic + Version      │  │  │
│  │  │ 0x0008: Device Name          │  │  │
│  │  │ 0x0028: WiFi SSID            │  │  │
│  │  │ 0x0048: WiFi Password (enc)  │  │  │  5. Credentials
│  │  │ 0x0088: Admin Pass (hash)    │  │  │     saved to FRAM
│  │  │ 0x00A8: VPS Token (enc)      │  │  │     (encrypted)
│  │  │ 0x00E8: VPS URL              │  │  │
│  │  │ 0x0168: Whitelist IPs        │  │  │
│  │  └──────────────────────────────┘  │  │
│  │  AES-256 encrypted                 │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
                     │
                     │ 6. Manual restart
                     │    (user power cycle)
                     ▼
┌──────────────────────────────────────────┐
│       ESP32 Production Mode              │
│  ┌────────────────────────────────────┐  │
│  │  Load credentials from FRAM        │  │
│  │  Connect WiFi                      │  │
│  │  Start WebSocket → VPS             │  │
│  │  Start Web Dashboard (with IP      │  │
│  │         whitelist enforcement)     │  │
│  │  Normal water system operations    │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

---

## 🔍 Validation Rules Summary

### Device Name
- Pattern: `^[a-zA-Z0-9_-]{3,32}$`
- Min length: 3 characters
- Max length: 32 characters
- Allowed: letters, numbers, underscore, hyphen
- Example: `water-pump-01`, `ESP32_Main`

### WiFi SSID
- Min length: 1 character
- Max length: 32 characters
- Encoding: UTF-8 (support for non-ASCII)

### WiFi Password
- Min length: 8 characters (WPA2 minimum)
- Max length: 64 characters
- Allowed: any printable ASCII

### Admin Password
- Min length: 8 characters
- Max length: 64 characters
- Strength requirements:
  - At least 1 letter
  - At least 1 number
  - Recommended: mix of upper/lower/special chars

### VPS Token
- Min length: 16 characters
- Max length: 64 characters
- Format: typically hex or base64
- Validation: regex `^[A-Za-z0-9+/=_-]+$`

### VPS URL
- Format: `^https?://[a-zA-Z0-9.-]+(:[0-9]{1,5})?(/.*)?$`
- Examples:
  - `https://vps.example.com`
  - `https://192.168.1.100:8443`
  - `http://vps.local/api/v1`

### IP Whitelist
- Format per IP: `^([0-9]{1,3}\.){3}[0-9]{1,3}$`
- Range check: each octet 0-255
- Max entries: 10 IPs
- Empty list: Allow all (no whitelist)
- Examples: `192.168.1.100`, `10.0.0.5`

---

## ⚡ Performance Considerations

### RAM Usage
```
Estimated memory footprint in AP mode:

AsyncWebServer:           ~15KB
DNSServer:                ~2KB
WiFi AP:                  ~8KB
Log Buffer (50 entries):  ~7KB
HTML page (embedded):     ~10KB
JSON buffers:             ~5KB
WebSocket test client:    ~8KB
────────────────────────────────
Total estimated:          ~55KB

ESP32-C3 available:       ~300KB
Safety margin:            OK ✓
```

**Monitoring:**
- Log `ESP.getFreeHeap()` co 10s w AP mode loop
- Warning jeśli free < 50KB
- Serial output: memory usage stats

---

### Flash Usage
```
New code modules:         ~60KB
HTML/CSS/JS embedded:     ~20KB
Libraries (AsyncWeb):     ~40KB
────────────────────────────────
Total additional:         ~120KB

ESP32-C3 flash (4MB):     Available ✓
```

---

### Network Performance
```
AP Mode simultaneous clients: Max 4 recommended
DNS requests/s:              ~100 (sufficient)
HTTP requests/s:             ~10 (sufficient for setup)
WebSocket test duration:     15s max

Throttling:
- Rate limiting: 5 req/min per endpoint
- WiFi scan: max 1 per 10s
- Test sequence: single instance (mutex)
```

---

## 🛡️ Error Handling Strategy

### Kategorie błędów
```
1. USER INPUT ERRORS
   ├─ Invalid format → Show field-specific error
   ├─ Empty required field → Highlight red
   └─ Password too weak → Show requirements

2. NETWORK ERRORS
   ├─ WiFi scan fail → Retry button, manual SSID entry
   ├─ WiFi connect timeout → Clear error message, retry option
   ├─ DNS resolution fail → Check URL format, test DNS servers
   ├─ HTTP timeout → Check VPS URL, firewall
   └─ WebSocket fail → Check port, protocol

3. SYSTEM ERRORS
   ├─ FRAM write fail → Retry, warn user, log details
   ├─ FRAM read fail → Factory reset option
   ├─ Memory low → Simplify UI, reduce buffer
   └─ Watchdog timeout → Auto-restart AP mode

4. SECURITY ERRORS
   ├─ Rate limit exceeded → 429 status, wait timer
   ├─ Invalid IP (production) → 403 Forbidden
   └─ Session expired → Redirect to setup
```

### Error Messages - User-Friendly
```
❌ BAD: "FRAM_WRITE_ERROR_0x03"
✅ GOOD: "Could not save configuration. Please try again."

❌ BAD: "WL_CONNECT_FAILED"
✅ GOOD: "Could not connect to WiFi. Please check your password."

❌ BAD: "DNS_TIMEOUT"
✅ GOOD: "Could not reach VPS server. Check URL and network."
```

---

## 📚 Dependencies & Libraries

### Required Libraries (PlatformIO)
```ini
lib_deps =
    me-no-dev/ESPAsyncWebServer @ ^1.2.3
    me-no-dev/AsyncTCP @ ^1.1.1
    bblanchon/ArduinoJson @ ^6.21.0
    links2004/WebSockets @ ^2.4.0
    ; DNSServer - built-in ESP32 core
    ; WiFi - built-in ESP32 core
    
    ; Existing project libraries:
    ; Adafruit_FRAM_I2C (keep)
    ; RTClib (keep)
    ; ... (other existing deps)
```

### External Resources (None Required)
- No CDN dependencies
- No external fonts
- No external CSS frameworks
- All assets embedded in ESP32 flash

---

## 🧪 Testing Scenarios - Checklist

```
□ BOOT SCENARIOS
  □ First boot (no credentials) → HALT with message
  □ Button held >5s → Enter AP mode
  □ Button held <5s → Normal production boot
  □ Valid credentials → WiFi connect → Production
  □ Invalid credentials → HALT with error

□ AP MODE FUNCTIONALITY
  □ AP starts with correct SSID/password
  □ DNS hijack works (all domains → 192.168.4.1)
  □ iOS captive portal auto-opens
  □ Android notification appears
  □ Windows captive portal triggered

□ WIFI SCANNER
  □ Scan returns visible networks
  □ Networks sorted by RSSI
  □ Duplicates filtered (keep strongest)
  □ Dropdown populates correctly
  □ Refresh button works

□ FORM VALIDATION
  □ Empty field shows error
  □ Invalid device name pattern rejected
  □ Short password rejected (< 8 chars)
  □ Invalid URL format rejected
  □ Invalid IP format rejected
  □ Client-side validation instant
  □ Server-side validation enforced

□ CONNECTION TESTS
  □ WiFi test: 3 retries, 5s timeout each
  □ WiFi test: success logs IP/RSSI
  □ WiFi test: failure shows clear error
  □ DNS test: resolves VPS hostname
  □ HTTP test: returns 200 OK
  □ WebSocket test: handshake succeeds
  □ VPS auth test: token accepted
  □ Any test failure aborts save

□ LOG VIEWER
  □ Logs appear in real-time (5s refresh)
  □ Log viewer auto-scrolls to bottom
  □ Timestamp format correct
  □ Log levels color-coded (INFO/WARN/ERROR)
  □ Buffer doesn't overflow (circular)

□ FRAM OPERATIONS
  □ Save credentials → write success
  □ Restart → read credentials → values match
  □ Encryption/decryption works
  □ CRC validation passes
  □ Invalid magic number detected
  □ Version mismatch handled

□ PRODUCTION MODE
  □ WiFi connects with saved credentials
  □ Web dashboard accessible
  □ IP whitelist enforces restrictions (if configured)
  □ IP whitelist allows all if empty list
  □ Normal water system operations work
  □ VPS WebSocket connection stable

□ SECURITY
  □ WPA2 password required for AP
  □ HTTPS warning banner visible
  □ Rate limiting triggers after 5 requests
  □ Admin password hashed (double SHA256)
  □ VPS token encrypted in FRAM
  □ WiFi password encrypted in FRAM

□ ERROR SCENARIOS
  □ FRAM write fail → error message, retry
  □ WiFi scan fail → manual SSID entry works
  □ Low memory → system continues (degraded)
  □ Watchdog timeout → auto-restart AP mode
  □ Invalid session → redirect to setup
```

---

## 📄 Summary - Key Takeaways

### Główne zmiany w projekcie
1. **Dodanie provisioning mode** - button-triggered, blocking AP mode
2. **Usunięcie auto-fallback** - WiFi fail counter REMOVED
3. **Usunięcie LED patterns** - zamienione na serial logs w UI
4. **FRAM layout expansion** - dodanie VPS URL, admin password, IP whitelist
5. **Security layer** - IP whitelist dla production dashboard
6. **Mobile-first UI** - captive portal z WiFi scan i real-time logs

### Kluczowe decyzje architektoniczne
- **Provisioning TYLKO przez przycisk** (>5s hold podczas boot)
- **Ręczny restart po konfiguracji** (brak auto-reboot)
- **Wszystkie hardware wyłączone w AP mode** (tylko networking)
- **Kompletna sekwencja testów** (WiFi → DNS → HTTP → WS → Auth)
- **Serial logs buforowane w RAM** (50 entries, mutex-protected)
- **WPA2 secured AP** (password: setup12345)

### Najważniejsze endpointy
1. `GET /` - Setup page (HTML/CSS/JS embedded)
2. `GET /api/scan` - WiFi network scan
3. `POST /api/configure` - Configuration submit + test sequence
4. `GET /api/logs` - Buffered serial logs (auto-refresh)
5. `GET /hotspot-detect.html` - iOS captive portal detection
6. `GET /generate_204` - Android captive portal detection

### Bezpieczeństwo
- **NO HTTPS** w AP mode (ostrzeżenie w UI)
- **WPA2 AP** (nie open network)
- **Double-hashed passwords** (SHA256 client + server)
- **AES-256 FRAM encryption** (existing mechanism)
- **IP whitelist** dla production dashboard
- **Rate limiting** - 5 req/min per endpoint

### Roadmap priorytetów
1. **Faza 1-2** - FRAM, button, AP core, DNS (Priorytet 1)
2. **Faza 3-4** - Frontend, WiFi scan, tests, logs (Priorytet 2)
3. **Faza 5** - Save config, production mode, whitelist (Priorytet 2)
4. **Faza 6** - Polish, security hardening, testing (Priorytet 3)

---

## 📞 Next Steps

**Po zaakceptowaniu tej architektury:**

1. **Review tego dokumentu** - zatwierdzenie wszystkich decyzji
2. **Wybór FRAM layout** - Opcja A (modify existing) vs Opcja B (new from scratch)
3. **GPIO pin assignment** - określenie konkretnego pin number dla button
4. **Start implementacji** - zgodnie z roadmap (Faza 1)

**Dokument gotowy do wykorzystania jako:**
- Specyfikacja techniczna dla implementacji
- Referencja podczas code review
- Checklist dla testowania
- Dokumentacja architektury projektu

---

**Koniec dokumentu** ✓
