# Zdalny przycisk resetu systemu — plan implementacji

## Cel

Fizyczny przycisk reset na ESP32 dziala tylko w stanie ERROR (`checkResetButton()` w `water_algorithm.cpp:1373`).
Potrzebny jest zdalny odpowiednik dostepny z dashboardu webowego, ktory:
- Dziala z **kazdego** stanu (nie tylko ERROR)
- W aktywnym cyklu: bezpiecznie zatrzymuje pompe, zapisuje czesciowe dane do FRAM
- W stanie ERROR: kasuje blad i sygnal error
- W IDLE: no-op (potwierdza ze system jest OK)
- Jest **monostabilny** — klik → akcja → przycisk wraca do stanu domyslnego
- **Bez modala** potwierdzajacego

## Co NIE jest resetowane (bezpieczenstwo)

| Element | Zachowany? | Powod |
|---|---|---|
| `dailyVolumeML` | TAK | Limit dzienny chroni przed przelaniem |
| `availableVolumeCurrent` | TAK | Stan zbiornika zrodlowego |
| `framCycles` / `todayCycles` | TAK | Historia cykli do debugowania |
| Ustawienia pompy | TAK | Konfiguracja uzytkownika |

## Analiza istniejacych mechanizmow resetu

Algorytm ma juz 4 funkcje resetujace, kazda z innym zakresem:

| Funkcja | Zakres | Stan ERROR | Aktywna pompa | Daily volume |
|---|---|---|---|---|
| `resetCycle()` (prywatna) | Czyści dane jednego cyklu | NIE kasuje | NIE zatrzymuje | Zachowany |
| `resetFromError()` | Kasuje blad, state→IDLE | KASUJE | NIE zatrzymuje | Zachowany |
| `handleSystemDisable()` | Bezpieczne przerwanie cyklu | NIE kasuje (zachowuje ERROR) | ZATRZYMUJE + zapis FRAM | Zachowany |
| `resetDailyVolume()` | Reset licznika dziennego | n/a | Blokuje jesli pompa aktywna | ZERUJE |

**Nowa `resetSystem()` laczy `handleSystemDisable()` + `resetFromError()`:**
- Obsluguje ERROR (czego handleSystemDisable nie robi)
- Bezpiecznie zatrzymuje pompe (czego resetFromError nie robi)
- NIE ustawia `systemWasDisabled` (to nie jest disable systemu)

## Logika `resetSystem()`

```
resetSystem():
  if STATE_IDLE:
    → log "already idle"
    → return true (nic do zrobienia)

  if STATE_LOGGING:
    → log "logging in progress, please wait"
    → return false (nie przerywaj zapisu do FRAM)

  if STATE_ERROR:
    → resetFromError() (czyści error + signal + state→IDLE + resetCycle)
    → return true

  if aktywny cykl (PRE_QUAL, SETTLING, DEBOUNCING, PUMPING_AND_VERIFY, MANUAL_OVERRIDE):
    → stopPump() jesli pompa aktywna
    → oblicz i zapisz czesciowy volume do dailyVolumeML
    → zapisz czesciowy cykl do FRAM
    → state = STATE_IDLE
    → resetCycle()
    → return true
```

Wzorowane na `handleSystemDisable()` linie 89-175 w `water_algorithm.cpp`.

## Bezpieczenstwo: reset w trakcie pompowania

**Krytyczne**: `resetCycle()` resetuje stan algorytmu ale NIE zatrzymuje pompy!
Pompa ma wlasny niezalezny timer w `pump_controller.cpp`.

Dlatego `resetSystem()` MUSI:
1. Najpierw `stopPump()` — zatrzymac hardware
2. Obliczyc faktyczny volume (czas pompowania * ml/s)
3. Dodac do `dailyVolumeML` i zapisac do FRAM
4. Dopiero potem `resetCycle()` i state→IDLE

## Pliki do modyfikacji

### 1. `src/algorithm/water_algorithm.h`
Dodaj w sekcji public (obok `resetFromError()`):
```cpp
bool resetSystem();
```

### 2. `src/algorithm/water_algorithm.cpp`
Implementacja `resetSystem()` — wzorowana na `handleSystemDisable()` ale z obsluga ERROR.

### 3. `src/web/web_handlers.h`
```cpp
void handleSystemReset(AsyncWebServerRequest *request);
```

### 4. `src/web/web_handlers.cpp`
```cpp
void handleSystemReset(AsyncWebServerRequest *request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = waterAlgorithm.resetSystem();

    JsonDocument json;
    json["success"] = success;
    json["state"] = waterAlgorithm.getStateString();
    json["message"] = success ? "System reset to IDLE" : "Reset blocked - logging in progress";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}
```

### 5. `src/web/web_server.cpp`
```cpp
server.on("/api/system-reset", HTTP_POST, handleSystemReset);
```

### 6. `src/web/html_pages.cpp`
Nowy przycisk w sekcji "System Control" (SECOND CARD).
Grid: `repeat(2, 1fr)` → `repeat(3, 1fr)`.

```html
<button id="systemResetBtn" class="btn btn-secondary" onclick="systemReset()">
    System Reset
</button>
```

JS (monostabilny):
```js
function systemReset() {
    var btn = document.getElementById("systemResetBtn");
    btn.disabled = true;
    btn.textContent = "Resetting...";
    fetch("api/system-reset", { method: "POST" })
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (data.success) showNotification(data.message, "success");
            else showNotification(data.message || "Reset failed", "error");
        })
        .catch(function() { showNotification("Connection error", "error"); })
        .finally(function() {
            btn.disabled = false;
            btn.textContent = "System Reset";
        });
}
```

## Weryfikacja

```bash
# 1. Kompilacja
pio run

# 2. Test IDLE: klik "System Reset" → "System already in IDLE" / success
# 3. Test ERROR: wywolaj reset w stanie ERROR → powrot do IDLE, sygnal error zgaszony
# 4. Test VPS: endpoint dziala przez proxy (auto-auth)
# 5. Test mid-pump: reset w trakcie pompowania → pompa zatrzymana, volume zapisany
```

## Tabela stanow vs. akcja resetSystem()

| Stan | Akcja | Pompa | FRAM zapis | Wynik |
|---|---|---|---|---|
| STATE_IDLE | no-op | n/a | NIE | return true |
| STATE_PRE_QUALIFICATION | resetCycle() | n/a (nie pompowal) | NIE | return true |
| STATE_SETTLING | resetCycle() | n/a | NIE | return true |
| STATE_DEBOUNCING | resetCycle() | n/a | NIE | return true |
| STATE_PUMPING_AND_VERIFY | stopPump() + zapis volume | TAK — zatrzymaj | TAK — czesciowy cykl | return true |
| STATE_LOGGING | odmowa | n/a | w trakcie | return false |
| STATE_ERROR | resetFromError() | n/a | NIE | return true |
| STATE_MANUAL_OVERRIDE | stopPump() + zapis volume | TAK — zatrzymaj | TAK — czesciowy cykl | return true |

---

# Zadanie 2: Przycisk "Manual Pump" — bezposrednie sterowanie pompa

## Cel

Obecny przycisk `manualCycleBtn` przechodzi przez `triggerPump()` → `requestManualPump()` → maszyne stanow algorytmu. To powoduje:
- Blokade w stanach ERROR, LOGGING, system disabled, daily limit
- Zmiane stanu na `STATE_MANUAL_OVERRIDE`
- Resetowanie biezacego cyklu automatycznego
- Interakcje z `addManualVolume()` i `onManualPumpComplete()`

**Nowe zachowanie**: przycisk steruje pompa BEZPOSREDNIO na poziomie GPIO — omija caly algorytm, system disabled, daily limit i pumpGlobalEnabled. Jedyne ograniczenie to hardware.

## Dwa tryby pracy przycisku

### Tryb bistabilny (klik < 3s)
- Krotkie nacisniecie przycisku (mouseup przed uplywem 3 sekund)
- Jesli pompa OFF → wlacz na czas `manualCycleSeconds` (obecnie 60s z `PumpSettings`)
- Jesli pompa ON → wylacz natychmiast
- Przycisk pokazuje aktualny stan: "Manual Pump OFF" / "Manual Pump ON"

### Tryb monostabilny (przytrzymanie >= 3s)
- Po 3 sekundach trzymania przycisku (mousedown) pompa startuje
- Pompa dziala dopoki uzytkownik trzyma przycisk
- mouseup → pompa stop
- **Safety timeout**: ESP zatrzymuje pompe po 120s bez komendy stop (zabezpieczenie przed utrata polaczenia)
- Przycisk pokazuje: "Manual Pump ON" podczas trzymania

## Nowe stale konfiguracyjne

### `src/config/config.h`
```cpp
// Direct manual pump safety timeout (monostable mode)
#define DIRECT_PUMP_SAFETY_TIMEOUT_S 120
```

## Nowa funkcja: `directPumpOn()` / `directPumpOff()`

### `src/hardware/pump_controller.h`
```cpp
bool directPumpOn(uint16_t durationSeconds);
void directPumpOff();
bool isDirectPumpMode();
```

### `src/hardware/pump_controller.cpp`

Nowa zmienna stanu:
```cpp
static bool directPumpMode = false;
```

```
directPumpOn(durationSeconds):
  if pumpRunning AND NOT directPumpMode:
    → return false (nie przerywaj cyklu algorytmu — jedyny warunek blokady)
  if pumpRunning AND directPumpMode:
    → juz dziala w trybie direct, ignoruj
    → return true

  directPumpMode = true
  digitalWrite(PUMP_RELAY_PIN, LOW)   // relay ON
  pumpRunning = true
  pumpStartTime = millis()
  pumpDuration = durationSeconds * 1000
  currentActionType = "DIRECT_MANUAL"
  return true

directPumpOff():
  if NOT pumpRunning OR NOT directPumpMode:
    → return (nic do zrobienia)

  digitalWrite(PUMP_RELAY_PIN, HIGH)  // relay OFF
  pumpRunning = false
  directPumpMode = false
  currentActionType = ""
  // BEZ wywolania waterAlgorithm.addManualVolume()
  // BEZ wywolania waterAlgorithm.onManualPumpComplete()
  LOG_INFO("Direct pump stopped")

isDirectPumpMode():
  return directPumpMode
```

### Modyfikacja `updatePumpController()`

Obecny kod w `pump_controller.cpp:35` zatrzymuje pompe gdy `!pumpGlobalEnabled`:
```cpp
if (!pumpGlobalEnabled && pumpRunning) {
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = false;
```

**Zmiana**: dodaj warunek — NIE zatrzymuj jesli `directPumpMode`:
```cpp
if (!pumpGlobalEnabled && pumpRunning && !directPumpMode) {
```

Obecny auto-stop po uplywie `pumpDuration` (linia 43) — zachowaj bez zmian.
Gdy timer uplywa w trybie direct, pompa sie zatrzymuje (safety timeout).

Po auto-stop w trybie direct, ustaw `directPumpMode = false` i NIE wywoluj `waterAlgorithm.addManualVolume()` ani `onManualPumpComplete()`.

### Modyfikacja sekcji onManualPumpComplete (linie 73-80)

Dodaj warunek — NIE wywoluj `onManualPumpComplete()` jesli `directPumpMode`:
```cpp
if (wasManualActive && !manualPumpActive && !pumpRunning && !directPumpMode) {
```

## Nowe endpointy API

### `src/web/web_handlers.h`
```cpp
void handleDirectPumpOn(AsyncWebServerRequest *request);
void handleDirectPumpOff(AsyncWebServerRequest *request);
```

### `src/web/web_handlers.cpp`

```cpp
void handleDirectPumpOn(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    // Parametr duration: bistable uzywa manualCycleSeconds, monostable uzywa safety timeout
    uint16_t duration = currentPumpSettings.manualCycleSeconds;
    if (request->hasParam("mode", true) && request->getParam("mode", true)->value() == "monostable") {
        duration = DIRECT_PUMP_SAFETY_TIMEOUT_S;
    }

    bool success = directPumpOn(duration);

    JsonDocument json;
    json["success"] = success;
    json["duration"] = duration;
    json["mode"] = request->hasParam("mode", true) ? request->getParam("mode", true)->value() : "bistable";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOff(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    directPumpOff();

    JsonDocument json;
    json["success"] = true;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}
```

### `src/web/web_server.cpp`
```cpp
server.on("/api/pump/direct-on", HTTP_POST, handleDirectPumpOn);
server.on("/api/pump/direct-off", HTTP_POST, handleDirectPumpOff);
```

## Usuniecie starego kodu

### `src/web/web_handlers.cpp`
- Usun `handlePumpNormal()` — zastapione przez `handleDirectPumpOn()`
- Usun `handlePumpExtended()` — nie jest juz potrzebne (kalibracja przez osobny przycisk jesli trzeba)

### `src/web/web_server.cpp`
- Usun routing `/api/pump/normal` i `/api/pump/extended`

### `src/algorithm/water_algorithm.cpp`
- `requestManualPump()` — zachowaj (moze byc uzywane przez inne sciezki)
- `onManualPumpComplete()` — zachowaj (j.w.)

## Frontend: `src/web/html_pages.cpp`

### Zmiana tekstu przycisku

Obecny (linia 1063):
```html
<button id="manualCycleBtn" class="btn btn-off" onclick="toggleManualCycle()">
```

Nowy:
```html
<button id="manualPumpBtn" class="btn btn-off">
    Manual Pump OFF
</button>
```

Usun `onclick` — obsluga przez mousedown/mouseup/touchstart/touchend w JS.

### Nowa logika JavaScript

Usun cala funkcje `toggleManualCycle()`, `updateManualCycleButton()`, `startPumpMonitoring()`.
Usun zmienna `manualCycleActive`.

Nowe zmienne i funkcje:

```js
var pumpBtnDownTime = 0;
var monostableTimer = null;
var monostableActive = false;

// Inicjalizacja — podpiecie eventow
(function initPumpBtn() {
    var btn = document.getElementById("manualPumpBtn");
    if (!btn) return;

    // Mouse events
    btn.addEventListener("mousedown", onPumpBtnDown);
    btn.addEventListener("mouseup", onPumpBtnUp);
    btn.addEventListener("mouseleave", onPumpBtnUp);  // safety: kursor opuszcza przycisk

    // Touch events (mobile)
    btn.addEventListener("touchstart", function(e) {
        e.preventDefault();
        onPumpBtnDown();
    });
    btn.addEventListener("touchend", function(e) {
        e.preventDefault();
        onPumpBtnUp();
    });
})();

function onPumpBtnDown() {
    pumpBtnDownTime = Date.now();

    // Po 3 sekundach → tryb monostabilny
    monostableTimer = setTimeout(function() {
        monostableActive = true;
        // Wlacz pompe w trybie monostabilnym
        fetch("api/pump/direct-on", {
            method: "POST",
            headers: {"Content-Type": "application/x-www-form-urlencoded"},
            body: "mode=monostable"
        })
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (data.success) {
                updatePumpButton(true);
                showNotification("Monostable mode — hold to keep pump running", "success");
            }
        })
        .catch(function() { showNotification("Connection error", "error"); });
    }, 3000);
}

function onPumpBtnUp() {
    var holdDuration = Date.now() - pumpBtnDownTime;

    if (monostableTimer) {
        clearTimeout(monostableTimer);
        monostableTimer = null;
    }

    if (monostableActive) {
        // Koniec trybu monostabilnego — wylacz pompe
        monostableActive = false;
        fetch("api/pump/direct-off", { method: "POST" })
            .then(function(r) { return r.json(); })
            .then(function(data) {
                if (data.success) {
                    updatePumpButton(false);
                    showNotification("Pump stopped", "success");
                }
            })
            .catch(function() { showNotification("Connection error", "error"); });
    } else if (holdDuration < 3000) {
        // Tryb bistabilny — toggle
        var pumpIsOn = document.getElementById("manualPumpBtn").classList.contains("btn-primary");

        if (pumpIsOn) {
            // Wylacz
            fetch("api/pump/direct-off", { method: "POST" })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (data.success) {
                        updatePumpButton(false);
                        showNotification("Pump stopped", "success");
                    }
                })
                .catch(function() { showNotification("Connection error", "error"); });
        } else {
            // Wlacz na preset czas
            fetch("api/pump/direct-on", { method: "POST" })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (data.success) {
                        updatePumpButton(true);
                        showNotification("Pump ON for " + data.duration + "s", "success");
                    }
                })
                .catch(function() { showNotification("Connection error", "error"); });
        }
    }
}

function updatePumpButton(isOn) {
    var btn = document.getElementById("manualPumpBtn");
    if (!btn) return;
    if (isOn) {
        btn.textContent = "Manual Pump ON";
        btn.className = "btn btn-primary";
    } else {
        btn.textContent = "Manual Pump OFF";
        btn.className = "btn btn-off";
    }
}
```

### Synchronizacja stanu przycisku z rzeczywistym stanem pompy

Istniejacy polling `secureFetch("api/status")` juz zwraca `pump_active`.
W callbacku statusu dodaj:
```js
// Synchronize direct pump button with actual pump state
updatePumpButton(data.pump_active);
```

To zapewnia ze:
- Gdy safety timeout zatrzyma pompe → przycisk wroci do OFF
- Gdy inna sciezka zatrzyma pompe → przycisk sie zsynchronizuje
- Po odswiezeniu strony → przycisk pokazuje aktualny stan

## Podsumowanie zmian w plikach

| Plik | Akcja |
|---|---|
| `src/config/config.h` | Dodaj `#define DIRECT_PUMP_SAFETY_TIMEOUT_S 120` |
| `src/hardware/pump_controller.h` | Dodaj `directPumpOn()`, `directPumpOff()`, `isDirectPumpMode()` |
| `src/hardware/pump_controller.cpp` | Implementacja 3 nowych funkcji + modyfikacja `updatePumpController()` |
| `src/web/web_handlers.h` | Dodaj `handleDirectPumpOn()`, `handleDirectPumpOff()` |
| `src/web/web_handlers.cpp` | Implementacja 2 nowych handlerow. Usun `handlePumpNormal()`, `handlePumpExtended()` |
| `src/web/web_server.cpp` | Dodaj 2 nowe routingi, usun 2 stare |
| `src/web/html_pages.cpp` | Nowy przycisk, nowa logika JS, usun stary kod `toggleManualCycle` |

## Co NIE jest zmieniane

- `water_algorithm.cpp` — `requestManualPump()` i `onManualPumpComplete()` pozostaja (moga byc uzywane przez inne sciezki, np. AUTO_PUMP)
- `handlePumpStop()` — zachowany jako ogolny endpoint stop (dziala na kazdym trybie)
- `handlePumpToggle()` — legacy, zachowany

## Weryfikacja

```bash
# 1. Kompilacja
pio run

# 2. Test bistabilny: klik "Manual Pump OFF" → pompa ON na 60s, przycisk "Manual Pump ON"
# 3. Test bistabilny stop: klik "Manual Pump ON" → pompa OFF natychmiast
# 4. Test monostabilny: trzymaj przycisk 3s+ → pompa ON, pusc → pompa OFF
# 5. Test safety timeout: wlacz monostabilnie, zamknij przegladarke → pompa stop po 120s
# 6. Test w ERROR: przycisk dziala mimo stanu ERROR
# 7. Test system disabled: przycisk dziala mimo wylaczonego systemu
# 8. Test kolizja: algorytm pompuje (STATE_PUMPING_AND_VERIFY) → directPumpOn zwraca false
# 9. Test VPS proxy: endpointy dzialaja przez proxy
```
