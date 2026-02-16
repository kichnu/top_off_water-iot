# Migracja IoT ESP32 — obsluga VPS proxy (auto-auth)

Checklist zmian w istniejacym firmware ESP32, aby dzialal dostep zdalny przez VPS/nginx/WireGuard.
Referencja implementacji: projekt `switch_timer-iot`.
Pelna dokumentacja wzorcow: `docs/SECURITY_PATTERNS.md`.

---

## Wymagania wstepne

- Urzadzenie ma juz dzialajacy web server (ESPAsyncWebServer)
- Urzadzenie ma sesje cookie + IP whitelist + rate limiting
- VPS ma skonfigurowany WireGuard tunel do routera domowego
- VPS ma nginx z `auth_request` do Flask gateway

---

## Krok 1: config.h / config.cpp — TRUSTED_PROXY_IP

**config.h** — dodaj extern:
```cpp
// ================= TRUSTED PROXY (WireGuard VPS) =================
extern const IPAddress TRUSTED_PROXY_IP;
```

**config.cpp** — zdefiniuj IP i usun z whitelist:
```cpp
// Proxy IP — NIE w ALLOWED_IPS[]
const IPAddress TRUSTED_PROXY_IP(10, 99, 0, 1);

// ALLOWED_IPS — tylko adresy LAN (bez proxy IP)
const IPAddress ALLOWED_IPS[] = {
    IPAddress(192, 168, 2, 10),
    IPAddress(192, 168, 2, 20)
};
```

> Jesli `10.99.0.1` bylo w `ALLOWED_IPS[]` — usun. Proxy ma oddzielna sciezke.

---

## Krok 2: auth_manager.h / auth_manager.cpp — isTrustedProxy + resolveClientIP

**auth_manager.h** — dodaj include i deklaracje:
```cpp
#include <ESPAsyncWebServer.h>

bool isTrustedProxy(IPAddress ip);
IPAddress resolveClientIP(AsyncWebServerRequest* request);
```

**auth_manager.cpp** — dodaj implementacje:
```cpp
bool isTrustedProxy(IPAddress ip) {
    return ip == TRUSTED_PROXY_IP;
}

IPAddress resolveClientIP(AsyncWebServerRequest* request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (isTrustedProxy(sourceIP) && request->hasHeader("X-Forwarded-For")) {
        String xff = request->getHeader("X-Forwarded-For")->value();
        int comma = xff.indexOf(',');
        String clientStr = (comma > 0) ? xff.substring(0, comma) : xff;
        clientStr.trim();
        IPAddress realIP;
        if (realIP.fromString(clientStr)) {
            LOG_INFO("Proxy request: %s via %s", clientStr.c_str(), sourceIP.toString().c_str());
            return realIP;
        }
        LOG_WARNING("Failed to parse X-Forwarded-For: %s", xff.c_str());
    }
    return sourceIP;
}
```

---

## Krok 3: web_server.cpp — checkAuthentication() z auto-auth

Zastap istniejaca funkcje `checkAuthentication()`:

```cpp
bool checkAuthentication(AsyncWebServerRequest* request) {
    IPAddress sourceIP = request->client()->remoteIP();

    // VPS proxy auto-auth — VPS juz uwierzytelnial uzytkownika przez auth_request + sesje
    if (isTrustedProxy(sourceIP)) {
        return true;
    }

    // Dostep LAN — pelny lancuch: whitelist -> rate limit -> sesja
    if (!isIPAllowed(sourceIP)) {
        LOG_WARNING("IP %s not on whitelist - access denied", sourceIP.toString().c_str());
        return false;
    }

    if (isIPBlocked(sourceIP)) {
        return false;
    }

    if (isRateLimited(sourceIP)) {
        recordFailedAttempt(sourceIP);
        return false;
    }

    recordRequest(sourceIP);

    // Sprawdz session cookie
    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();

            String token = cookie.substring(tokenStart, tokenEnd);
            if (validateSession(token, sourceIP)) {
                return true;
            }
        }
    }

    recordFailedAttempt(sourceIP);
    return false;
}
```

**onNotFound** — dodaj obsluge proxy IP:
```cpp
server.onNotFound([](AsyncWebServerRequest* request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }
    request->send(404, "text/plain", "Not Found");
});
```

---

## Krok 4: web_handlers.h / web_handlers.cpp — handleHealth + resolveClientIP

**web_handlers.h** — dodaj deklaracje:
```cpp
void handleHealth(AsyncWebServerRequest *request);
```

**web_handlers.cpp** — dodaj handler:
```cpp
void handleHealth(AsyncWebServerRequest *request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
        request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
        return;
    }

    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"device_name\":\"" + String(getDeviceID()) + "\",";
    json += "\"uptime\":" + String(millis());
    json += "}";

    request->send(200, "application/json", json);
}
```

**web_server.cpp** — zarejestruj route:
```cpp
server.on("/api/health", HTTP_GET, handleHealth);
```

---

## Krok 5: web_handlers.cpp — resolveClientIP w logach

Zamien wszystkie `request->client()->remoteIP()` w logach na `resolveClientIP(request)`.

Przeszukaj: `request->client()->remoteIP()` w web_handlers.cpp.
Zostaw oryginalne tylko w `handleHealth()` (tam sprawdzamy source IP, nie client IP).

Przyklad:
```cpp
// PRZED:
LOG_INFO("Web: action from %s", request->client()->remoteIP().toString().c_str());

// PO:
LOG_INFO("Web: action from %s", resolveClientIP(request).toString().c_str());
```

Dotyczy rowniez `handleLoginPage()` i `handleLogin()` — dodaj `isTrustedProxy()` do warunkow IP:
```cpp
// PRZED:
if (!isIPAllowed(sourceIP)) {

// PO:
if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
```

---

## Krok 6: html_pages.cpp — wzgledne URL-e (kompatybilnosc z proxy)

nginx proxy z prefixem sciezki (`/device/switch/`) stripuje prefix przez `proxy_pass http://IP:80/`.
Ale bezwzgledne URL-e w redirectach i JS omijaja prefix — powoduja redirect loop.

Znajdz i zamien:

| Kontekst | Przed | Po |
|---|---|---|
| Redirect po nieudanej auth | `request->redirect("/login")` | `request->redirect("login")` |
| JS: po udanym logowaniu | `window.location.href = "/"` | `window.location.href = "./"` |
| JS: link w overlay "sesja wygasla" | `href="/login"` | `href="login"` |
| JS: po wylogowaniu | `window.location.href = "/login"` | `window.location.href = "login"` |

> Zasada: NIGDY bezwzgledne sciezki (`/login`, `/`). ZAWSZE wzgledne (`login`, `./`).

### Logout i "Session Expired" — redirect do dashboardu VPS

Wzgledne URL-e rozwiazuja problem proxy path prefix, ale **logout i session expired
nadal prowadza do strony logowania ESP32** (`login`), zamiast do dashboardu VPS.

Rozwiazanie: JS wykrywa, czy urzadzenie jest dostepne przez proxy (sciezka zawiera `/device/`),
i przekierowuje na dashboard VPS (`/dashboard`) zamiast na login ESP32.

**Logout (JS w dashboard HTML):**
```js
function logout() {
    fetch("api/logout", {method: "POST"}).then(function() {
        if (window.location.pathname.indexOf("/device/") !== -1) {
            window.location.href = "/dashboard";
        } else {
            window.location.href = "login";
        }
    });
}
```

**Overlay "Session Expired" (link w HTML):**
```html
<a href="login"
   onclick="if(window.location.pathname.indexOf('/device/')!==-1){window.location.href='/dashboard';return false;}">
   Login
</a>
```

> Zasada: przez proxy → `/dashboard` (VPS), bezposrednio LAN → `login` (ESP32).

---

## Krok 7: Weryfikacja

```bash
# 1. Kompilacja
pio run

# 2. Sprawdz czy nie zostaly bezwzgledne URL-e
grep -rn '"/login"' src/web/
grep -rn '"/"' src/web/html_pages.cpp

# 3. Sprawdz czy nie zostaly stare remoteIP() w logach
grep -rn 'remoteIP()' src/web/web_handlers.cpp
# Dozwolone tylko w handleHealth() i handleLogin() (do sprawdzenia source IP)

# 4. Po wgraniu firmware — test z VPS:
curl http://<WG_IP>:80/api/health
# Oczekiwane: {"status":"ok","device_name":"...","uptime":...}
```

---

## Rejestr urzadzen — status migracji

| Urzadzenie | Projekt | Status | WireGuard IP |
|---|---|---|---|
| Switch Timer | switch_timer-iot | DONE | 192.168.10.X |
| Dolewka | dolewka-iot | TODO | 192.168.10.2 |
| Doser | doser-iot | TODO | 192.168.10.3 |
