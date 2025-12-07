#include "html_pages.h"

const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ESP32-C3 Water System Login</title>

    <style>
      body {
        font-family: Arial, sans-serif;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        margin: 0;
        padding: 0;
        display: flex;
        justify-content: center;
        align-items: center;
        min-height: 100vh;
      }
      .login-box {
        background: white;
        margin: 20px;
        padding: 40px;
        border-radius: 15px;
        box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
        width: 100%;
        max-width: 400px;
      }
      h1 {
        text-align: center;
        color: #333;
        margin-bottom: 30px;
      }
      .form-group {
        margin-bottom: 20px;
      }
      label {
        display: block;
        margin-bottom: 8px;
        font-weight: bold;
        color: #333;
      }
      input[type="password"] {
        width: 100%;
        padding: 15px;
        border: 2px solid #ddd;
        border-radius: 8px;
        font-size: 16px;
        box-sizing: border-box;
      }
      input[type="password"]:focus {
        outline: none;
        border-color: #667eea;
      }
      .login-btn {
        width: 100%;
        padding: 15px;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        border: none;
        border-radius: 8px;
        font-size: 16px;
        font-weight: bold;
        cursor: pointer;
      }
      .login-btn:hover {
        opacity: 0.9;
      }
      .alert {
        padding: 15px;
        margin: 15px 0;
        border-radius: 8px;
        display: none;
      }
      .alert.error {
        background: #f8d7da;
        color: #721c24;
        border: 1px solid #f5c6cb;
      }
      .info {
        margin-top: 20px;
        padding: 15px;
        background: #f8f9fa;
        border-radius: 8px;
        font-size: 12px;
        color: #666;
      }
    </style>
  </head>
  <body>
    <div class="login-box">
      <h1>🌱 Water System</h1>
      <form id="loginForm">
        <div class="form-group">
          <label for="password">Administrator Password:</label>
          <input type="password" id="password" name="password" required />
        </div>
        <button type="submit" class="login-btn">Login</button>
      </form>
      <div id="error" class="alert error"></div>
      <div class="info">
        <strong>🔒 Security Features:</strong><br />
        • Session-based authentication<br />
        • Rate limiting & IP filtering<br />
        • VPS event logging
      </div>
    </div>
    <script>
      document
        .getElementById("loginForm")
        .addEventListener("submit", function (e) {
          e.preventDefault();
          const password = document.getElementById("password").value;
          const errorDiv = document.getElementById("error");

          fetch("/api/login", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "password=" + encodeURIComponent(password),
          })
            .then((response) => response.json())
            .then((data) => {
              if (data.success) {
                window.location.href = "/";
              } else {
                let errorMessage = data.error || "Login failed";

                if (data.message && data.setup_instructions) {
                  errorMessage = data.message;
                  const setupDiv = document.createElement("div");
                  setupDiv.style.marginTop = "15px";
                  setupDiv.style.padding = "15px";
                  setupDiv.style.backgroundColor = "#fff3cd";
                  setupDiv.style.border = "1px solid #ffeaa7";
                  setupDiv.style.borderRadius = "8px";
                  setupDiv.style.fontSize = "14px";
                  setupDiv.innerHTML = `
                        <strong>🔧 Setup Required:</strong><br>
                        ${data.setup_instructions}<br>
                        <em>Hold button 5s during boot to enter Captive Portal</em>
                    `;

                  const existingSetup = document.querySelector(
                    ".setup-instructions"
                  );
                  if (existingSetup) existingSetup.remove();

                  setupDiv.className = "setup-instructions";
                  errorDiv.parentNode.insertBefore(
                    setupDiv,
                    errorDiv.nextSibling
                  );
                }

                errorDiv.textContent = errorMessage;
                errorDiv.style.display = "block";
              }
            })
            .catch((error) => {
              errorDiv.textContent =
                "Connection error - Check if device is running";
              errorDiv.style.display = "block";
            });
        });
    </script>
  </body>
</html>
)rawliteral";

const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ESP32-C3 Water System Dashboard</title>

    <style>
      body {
        font-family: Arial, sans-serif;
        margin: 0;
        padding: 20px;
        background: #667eea 0%;
        min-height: 100vh;
      }
      .container {
        max-width: 1000px;
        margin: 0 auto;
      }

      .header {
        display: flex;
        flex-direction: column;
        align-items: center;
        text-align: center;
        color: white;
        position: relative;
      }

      .Header-nav {
        display: flex;
      }

      .header h1 {
        margin: 0;
        font-size: 32px;
        text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
      }
      .logout-btn {
        margin-left: 50px;
        background: #dc3545;
        color: white;
        border: none;
        padding: 10px 20px;
        border-radius: 5px;
        cursor: pointer;
        font-weight: bold;
      }
      .logout-btn:hover {
        background: #c82333;
      }
      .card {
        background: white;
        padding: 25px;
        margin: 15px 0;
        border-radius: 5px;
        box-shadow: 0 8px 25px rgba(0, 0, 0, 0.15);
      }
      .card h2 {
        margin-top: 0;
        color: #333;
        border-bottom: 3px solid #667eea;
        padding-bottom: 10px;
        font-size: 24px;
      }
      .status-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
        gap: 15px;
      }
      .status-item {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 15px;
        background: #dbdee0;
        border-radius: 5px;
      }
      .status-label {
        font-weight: bold;
        color: #333;
      }
      .status-value {
        font-weight: bold;
        color: #333;
      }
      .button {
        background: #28a745;
        color: white;
        border: none;
        padding: 15px 25px;
        border-radius: 5px;
        cursor: pointer;
        font-size: 16px;
        font-weight: bold;
        transition: all 0.3s;
        min-width: 200px;
        width: 30%;
      }
      .button:hover {
        background: #218838;
        transform: translateY(-2px);
      }
      .button:disabled {
        background: #6c757d;
        cursor: not-allowed;
        transform: none;
      }
      .button.on-off {
        background: #ffc107;
        color: #333;
      }
      .button.on-off:hover {
        background: #e0a800;
      }
      .button.danger {
        background: #dc3545;
      }
      .button.danger:hover {
        background: #c82333;
      }
      .btn-calibration {
        background: #dddddd;
        color: #333;
        width: 200px;
      }
      .btn-calibration.btn-stat {
        width: 150px;
      }
      .btn-calibration:hover {
        background: #ccc;
      }

      .stats {
        font-weight: bold;
        font-size: 14px;
        margin-top: 10px;
      }

      .stats-reset {
        font-size: 14px;
        font-weight: bold;
        margin-left: 15px;
      }

      .notifications {
        height: 25px;
        width: 100%;
        margin-top: 10px;
        margin-bottom: 10px;
      }

      .alert {
        background-color: #c77777;
        color: white;
        border-radius: 5px;
        margin-top: 20px;
        margin-bottom: 10px;
        display: flex;
        align-items: center;
        justify-content: center;
        font-weight: bold;
      }

      @media (max-width: 600px) {
        .stats-reset {
          margin-top: 10px;
        }
      }

      .alert.success {
        background: #d4edda;
        color: #155724;
        border: 1px solid #c3e6cb;
      }
      .alert.error {
        background: #f8d7da;
        color: #721c24;
        border: 1px solid #f5c6cb;
      }
      .pump-controls {
        display: flex;
        flex-wrap: wrap;
        justify-content: space-between;
        align-items: center;
      }
      .pump-setting {
        display: flex;
        justify-content: space-around;
        flex-direction: row;
      }
      .pump-setting-form {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 15px;
        margin-left: 30px;
      }

      .pump-setting-form > label {
        font-weight: bold;
        font-size: 18px;
      }

      .pump-setting-form > input {
        padding: 8px;
        font-size: 18px;
        border: 2px solid #ddd;
        border-radius: 5px;
        width: 50px;
      }

      .pump-setting-form > button {
        background: #3498db;
        color: white;
        font-size: 18px;
        border: none;
        padding: 10px 20px;
        border-radius: 5px;
        cursor: pointer;
      }

      .rtc-error {
        color: #e74c3c !important;
      }

      .pump-stats {
        display: flex;
        flex-wrap: wrap;
        justify-content: space-between;
        text-align: center;
        box-sizing: border-box;
      }

      .pump-stat {
        background-color: #f7f7f7;
        border-radius: 5px;
        box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
        width: 30%;
        padding-top: 8px;
        padding-bottom: 8px;
        transition: transform 0.2s ease, box-shadow 0.2s ease;
      }

      .stats-reset,
      .stats-info {
        margin-top: 0.8rem;
        font-size: 0.9rem;
        color: #222;
        font-weight: 600;
        line-height: 1.4;
      }

      .stats-reset span,
      .stats-info span {
        font-weight: 700;
        color: #000;
      }

      .hardware-status {
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 10px;
        height: 80px;
      }

      .status-badge {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        padding: 15px 3px;
        background: #dbdee0;
        border-radius: 5px;
        transition: all 0.3s ease;
      }

      .badge-label {
        font-size: 11px;
        font-weight: bold;
        color: #333;
        text-transform: uppercase;
        margin-bottom: 8px;
        letter-spacing: 0.5px;
      }

      .badge-value {
        font-size: 15px;
        font-weight: bold;
        color: #333;
      }

      .status-badge.active-green {
        background: #27ae60;
        box-shadow: 0 2px 8px rgba(39, 174, 96, 0.3);
      }

      .status-badge.active-yellow {
        background: #facc81ff;
        box-shadow: 0 2px 8px rgba(243, 156, 18, 0.3);
      }

      .status-badge.active-orange {
        background: #f58450ff;
        box-shadow: 0 2px 8px rgba(230, 126, 34, 0.3);
      }

      .status-badge.error-red {
        background: #dc3545;
        box-shadow: 0 2px 8px rgba(231, 76, 60, 0.3);
      }

      .status-badge.inactive-gray {
        background: #dbdee0;
      }
      .process-status {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 80px;
        border-radius: 5px;
        background: #dbdee0;
      }

      .process-description {
        font-size: 16px;
        font-weight: bold;
        color: #333;
        margin-bottom: 8px;
        text-align: center;
      }

      .process-time {
        font-size: 14px;
        color: #666;
        font-weight: 600;
      }

      @media (max-width: 800px) {
        .pump-controls {
          flex-direction: column;
        }
        .pump-controls > .button {
          margin: 10px 50px;
        }
        .button {
          width: 60%;
        }
        .pump-stats {
          flex-direction: column;
          align-items: center;
        }
        .pump-stat {
          width: 60%;
          margin: 10px;
        }
        .pump-setting {
          flex-direction: column;
          align-items: center;
        }

        .button.btn-calibration {
          width: 80%;
        }
        .pump-setting-form {
          flex-direction: column;
          margin: auto;
          width: 80%;
          padding-bottom: 10px;
          padding-top: 10px;
        }
      }

      @media (max-width: 600px) {
        .Header-nav {
          flex-direction: column;
        }
        .pump-stat {
          width: 80%;
        }
        .button {
          width: 80%;
        }
        .logout-btn {
          margin-left: 0;
          margin-top: 10px;
          margin-bottom: 10px;
        }
        .status-grid {
          grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
        }
      }
      @media (max-width: 450px) {
        .pump-controls > .button {
          margin: 10px 10px;
        }
        .badge-label {
          font-size: 9px;
        }
        .badge-value {
          font-size: 12px;
        }
        .pump-setting-form > label {
          font-size: 14px;
        }
        .pump-setting-form > input {
          font-size: 25px;
        }
      }
    </style>
  </head>

  <body>
    <div class="container">
      <div class="header">
        <div class="Header-nav">
          <h1>Top Off Water - System</h1>
          <button class="logout-btn" onclick="logout()">Logout</button>
        </div>
        <div id="notifications" class="notifications"></div>
      </div>

      <div class="card">
        <h2>System Status</h2>
        <div class="status-grid">
          <div class="hardware-status">
            <div class="status-badge" id="sensor1Badge">
              <span class="badge-label">SENSOR 1</span>
              <span class="badge-value">OFF</span>
            </div>
            <div class="status-badge" id="sensor2Badge">
              <span class="badge-label">SENSOR 2</span>
              <span class="badge-value">OFF</span>
            </div>
            <div class="status-badge" id="pumpBadge">
              <span class="badge-label">PUMP</span>
              <span class="badge-value">IDLE</span>
            </div>
            <div class="status-badge" id="systemBadge">
              <span class="badge-label">SYSTEM</span>
              <span class="badge-value">OK</span>
            </div>
          </div>
          <div class="process-status">
            <span class="process-description" id="processDescription"
              >IDLE - Waiting for sensors</span
            >
            <span class="process-time" id="processTime">—</span>
          </div>

          <div class="status-item">
            <span class="status-label">WiFi Status:</span>
            <span class="status-value" id="wifiStatus">Loading...</span>
          </div>
          <div class="status-item">
            <span class="status-label">RTC Time(UTC):</span>
            <span class="status-value" id="rtcTime">Loading...</span>
          </div>
          <div class="status-item">
            <span class="status-label">Free Memory:</span>
            <span class="status-value" id="freeHeap">Loading...</span>
          </div>
          <div class="status-item">
            <span class="status-label">Uptime:</span>
            <span class="status-value" id="uptime">Loading...</span>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>Pump Control</h2>
        <div class="pump-controls">
          <button
            id="normalBtn"
            class="button cycle"
            onclick="triggerNormalPump()"
          >
            Manual Cycle
          </button>
          <button id="stopBtn" class="button danger" onclick="stopPump()">
            Stop Pump
          </button>
          <button
            id="onOffBtn"
            class="button on-off"
            onclick="togglePumpGlobal()"
          >
            Pump ON
          </button>
        </div>
      </div>

      <div class="card">
        <h2>Statistic</h2>

        <div class="pump-stats">
          <div class="pump-stat">
            <button
              id="loadStatsBtn"
              class="button btn-calibration"
              onclick="manualLoadStatistics()"
            >
              Load Statistics
            </button>
            <div class="stats values">
              Err activate: <span id="gap1Value"></span><br />
              Err deactivate: <span id="gap2Value"></span><br />
              Err pump: <span id="waterValue"></span>
            </div>
          </div>

          <div class="pump-stat">
            <button
              id="resetStatsBtn"
              class="button btn-calibration"
              onclick="resetStatistics()"
            >
              Reset Statistics
            </button>
            <div class="stats reset">
              Last Reset: <span id="resetTime">Loading...</span>
            </div>
          </div>

          <div class="pump-stat">
            <button
              id="resetDailyVolumeBtn"
              class="button btn-calibration"
              onclick="resetDailyVolume()"
            >
              Reset Daily Volume
            </button>
            <div class="stats info">
              Current: <span id="currentDailyVolume">Loading...</span> ml<br />
              <small>Max: <span id="maxDailyVolume">Loading...</span> ml</small>
            </div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>Pump Setting</h2>
        <div class="pump-setting">
          <button
            id="extendedBtn"
            class="button btn-calibration"
            onclick="triggerExtendedPump()"
          >
            Pump Calibration (30s)
          </button>
          <form
            class="pump-setting-form"
            id="volumeForm"
            onsubmit="updateVolumePerSecond(event)"
          >
            <div class="pump-setting-form">
              <label for="volumePerSecond">Volume per Second (ml):</label>
              <input
                type="number"
                id="volumePerSecond"
                name="volumePerSecond"
                min="1.0"
                max="50.0"
                step="0.1"
                value="1.0"
                required
                placeholder="Loading..."
              />
              <button type="submit">Update Setting</button>
              <span
                id="volumeStatus"
                style="font-size: 12px; color: #666"
              ></span>
            </div>
          </form>
        </div>
      </div>
    </div>

    <script>
      function showNotification(message, type) {
        const notifications = document.getElementById("notifications");
        const alert = document.createElement("div");
        alert.className = "alert " + type;
        alert.textContent = message;
        notifications.appendChild(alert);

        setTimeout(() => {
          if (notifications.contains(alert)) {
            notifications.removeChild(alert);
          }
        }, 5000);
      }

      function triggerNormalPump() {
        const btn = document.getElementById("normalBtn");
        btn.disabled = true;
        btn.textContent = "Starting...";

        fetch("/api/pump/normal", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              showNotification(
                "Pump started for " + data.duration + "s (" + data.volume_ml + "ml)",
                "success"
              );
            } else {
              showNotification("Failed to start pump", "error");
            }
          })
          .catch(() => showNotification("Connection error", "error"))
          .finally(() => {
            btn.disabled = false;
            btn.textContent = "Manual Cycle";
            updateStatus();
          });
      }

      function triggerExtendedPump() {
        const btn = document.getElementById("extendedBtn");
        btn.disabled = true;
        btn.textContent = "Starting...";

        fetch("/api/pump/extended", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              showNotification(
                "Extended pump started for " + data.duration + "s",
                "success"
              );
            } else {
              showNotification("Failed to start pump", "error");
            }
          })
          .catch(() => showNotification("Connection error", "error"))
          .finally(() => {
            btn.disabled = false;
            btn.textContent = "Extended Cycle";
            updateStatus();
          });
      }

      function stopPump() {
        const btn = document.getElementById("stopBtn");
        btn.disabled = true;
        btn.textContent = "Stopping...";

        fetch("/api/pump/stop", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              showNotification("Pump stopped successfully", "success");
            } else {
              showNotification("Failed to stop pump", "error");
            }
          })
          .catch(() => showNotification("Connection error", "error"))
          .finally(() => {
            btn.disabled = false;
            btn.textContent = "Stop Pump";
            updateStatus();
          });
      }

      function loadVolumePerSecond() {
        fetch("/api/pump-settings")
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              document.getElementById("volumePerSecond").value = parseFloat(
                data.volume_per_second
              ).toFixed(1);
              document.getElementById(
                "volumeStatus"
              ).textContent = "Current: " + parseFloat(
                data.volume_per_second
              ).toFixed(1) + " ml/s";
            }
          })
          .catch((error) => {
            console.error("Failed to load volume setting:", error);
            document.getElementById("volumeStatus").textContent =
              "Failed to load current setting";
          });
      }

      function updateVolumePerSecond(event) {
        event.preventDefault();

        const volumeInput = document.getElementById("volumePerSecond");
        const statusSpan = document.getElementById("volumeStatus");
        const volumeValue = parseFloat(volumeInput.value);

        if (volumeValue < 1 || volumeValue > 50.0) {
          statusSpan.textContent = "Error: Value must be between 1-50.0";
          statusSpan.style.color = "#e74c3c";
          return;
        }

        const confirmMessage = "Are you sure you want to change Volume per Second to " + volumeValue.toFixed(
          1
        ) + " ml/s?\n\nThis will affect pump operations and be saved permanently.";

        if (!confirm(confirmMessage)) {
          statusSpan.textContent = "Update cancelled";
          statusSpan.style.color = "#f39c12";
          loadVolumePerSecond();
          return;
        }

        statusSpan.textContent = "Updating...";
        statusSpan.style.color = "#f39c12";

        const formData = new FormData();
        formData.append("volume_per_second", volumeValue);

        fetch("/api/pump-settings", {
          method: "POST",
          body: formData,
        })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              statusSpan.textContent = "Updated: " + volumeValue.toFixed(
                1
              ) + " ml/s";
              statusSpan.style.color = "#27ae60";

              setTimeout(() => {
                loadVolumePerSecond();
              }, 1000);
            } else {
              statusSpan.textContent = "Error: " + (
                data.error || "Update failed"
              );
              statusSpan.style.color = "#e74c3c";
              loadVolumePerSecond();
            }
          })
          .catch((error) => {
            statusSpan.textContent = "Network error";
            statusSpan.style.color = "#e74c3c";
            loadVolumePerSecond();
          });
      }

      let pumpGlobalEnabled = true;

      function loadPumpGlobalState() {
        fetch("/api/pump-toggle")
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              pumpGlobalEnabled = data.enabled;
              updatePumpToggleButton(data.enabled, data.remaining_seconds);
            }
          })
          .catch((error) => console.error("Failed to load pump state:", error));
      }

      function togglePumpGlobal() {
        const btn = document.getElementById("onOffBtn");
        btn.disabled = true;
        btn.textContent = "Processing...";

        fetch("/api/pump-toggle", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              pumpGlobalEnabled = data.enabled;
              updatePumpToggleButton(data.enabled, data.remaining_seconds);
              showNotification(data.message, "success");
            } else {
              showNotification("Failed to toggle pump", "error");
            }
          })
          .catch((error) => {
            showNotification("Network error", "error");
          })
          .finally(() => {
            btn.disabled = false;
          });
      }

      function updatePumpToggleButton(enabled, remainingSeconds) {
        const btn = document.getElementById("onOffBtn");

        if (enabled) {
          btn.textContent = "Pump ON";
          btn.className = "button on-off enabled";
          btn.style.backgroundColor = "#27ae60";
        } else {
          const minutes = Math.floor(remainingSeconds / 60);
          const seconds = remainingSeconds % 60;
          btn.textContent = "Pump OFF (" + minutes + ":" + seconds
            .toString()
            .padStart(2, "0") + ")";
          btn.className = "button on-off disabled";
          btn.style.backgroundColor = "#e74c3c";
        }
      }

      function updateSensorBadge(badgeId, isActive) {
        const badge = document.getElementById(badgeId);
        if (!badge) return;

        const valueSpan = badge.querySelector(".badge-value");
        badge.className = "status-badge";

        if (isActive) {
          badge.classList.add("active-green");
          valueSpan.textContent = "ON";
        } else {
          badge.classList.add("inactive-gray");
          valueSpan.textContent = "OFF";
        }
      }

      function updatePumpBadge(badgeId, isActive, attempt) {
        const badge = document.getElementById(badgeId);
        if (!badge) return;

        const valueSpan = badge.querySelector(".badge-value");
        badge.className = "status-badge";

        if (!isActive) {
          badge.classList.add("inactive-gray");
          valueSpan.textContent = "IDLE";
        } else {
          if (attempt === 1) {
            badge.classList.add("active-green");
          } else if (attempt === 2) {
            badge.classList.add("active-yellow");
          } else if (attempt >= 3) {
            badge.classList.add("active-orange");
          } else {
            badge.classList.add("active-green");
          }
          valueSpan.textContent = "ACTIVE";
        }
      }

      function updateSystemBadge(badgeId, hasError) {
        const badge = document.getElementById(badgeId);
        if (!badge) return;

        const valueSpan = badge.querySelector(".badge-value");
        badge.className = "status-badge";

        if (hasError) {
          badge.classList.add("error-red");
          valueSpan.textContent = "ERROR";
        } else {
          badge.classList.add("active-green");
          valueSpan.textContent = "OK";
        }
      }

      function updateProcessStatus(
        descId,
        timeId,
        description,
        remainingSeconds
      ) {
        const descElement = document.getElementById(descId);
        const timeElement = document.getElementById(timeId);

        if (!descElement || !timeElement) return;

        descElement.textContent = description || "IDLE - Waiting for sensors";

        if (remainingSeconds && remainingSeconds > 0) {
          timeElement.textContent =
            "Remaining: " + formatTime(remainingSeconds);
        } else {
          timeElement.textContent = "—";
        }
      }

      function formatTime(seconds) {
        if (!seconds || seconds <= 0) return "0 sec";

        if (seconds < 60) {
          return seconds + " sec";
        }

        const minutes = Math.floor(seconds / 60);
        const secs = seconds % 60;

        if (secs === 0) {
          return minutes + " min";
        }

        return minutes + " min " + secs + "s";
      }

      function updateStatus() {
        fetch("/api/status")
          .then((response) => response.json())
          .then((data) => {
            updateSensorBadge("sensor1Badge", data.sensor1_active);
            updateSensorBadge("sensor2Badge", data.sensor2_active);
            updatePumpBadge(
              "pumpBadge",
              data.pump_active,
              data.pump_attempt || 0
            );
            updateSystemBadge("systemBadge", data.system_error);

            updateProcessStatus(
              "processDescription",
              "processTime",
              data.state_description,
              data.remaining_seconds
            );

            document.getElementById("wifiStatus").textContent =
              data.wifi_status;

            const rtcText = data.rtc_time || "Error";
            const rtcInfo = data.rtc_info || "";
            const rtcElement = document.getElementById("rtcTime");

            let rtcHTML = rtcText;

            if (
              data.rtc_battery_issue === true ||
              data.rtc_needs_sync === true
            ) {
              rtcHTML +=
                '<br><small style="color: #e74c3c; font-size: 0.8em; font-weight: bold;">⚠️ Battery may be dead - replace CR2032</small>';
            } else {
              rtcHTML += '<br><small style="color: #666; font-size: 0.8em;">' + rtcInfo + '</small>';
            }

            rtcElement.innerHTML = rtcHTML;

            if (
              data.rtc_hardware === false ||
              data.rtc_battery_issue === true ||
              data.rtc_needs_sync === true
            ) {
              rtcElement.classList.add("rtc-error");
            } else {
              rtcElement.classList.remove("rtc-error");
            }

            document.getElementById("freeHeap").textContent =
              (data.free_heap / 1024).toFixed(1) + " KB";
            document.getElementById("uptime").textContent = formatUptime(
              data.uptime
            );

            const isRunning = data.pump_active;
            document.getElementById("normalBtn").disabled = isRunning;
            document.getElementById("extendedBtn").disabled = isRunning;
            document.getElementById("stopBtn").disabled = !isRunning;
          })
          .catch((error) => {
            console.error("Status update failed:", error);
          });
      }

      function formatUptime(milliseconds) {
        const seconds = Math.floor(milliseconds / 1000);
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        return hours + "h " + minutes + "m";
      }

      function logout() {
        fetch("/api/logout", { method: "POST" }).then(() => {
          window.location.href = "/login";
        });
      }

      setInterval(updateStatus, 2000);
      updateStatus();

      function loadStatistics() {
        fetch("/api/get-statistics")
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              document.getElementById("gap1Value").textContent =
                data.gap1_fail_sum;
              document.getElementById("gap2Value").textContent =
                data.gap2_fail_sum;
              document.getElementById("waterValue").textContent =
                data.water_fail_sum;
              document.getElementById("resetTime").textContent =
                data.last_reset_formatted || "Unknown";
            } else {
              document.getElementById("resetTime").textContent =
                "Error loading";
            }
          })
          .catch((error) => {
            console.error("Failed to load statistics:", error);
            document.getElementById("resetTime").textContent =
              "Connection error";
          });
      }

      function resetStatistics() {
        const confirmMessage =
          "Are you sure you want to reset error statistics?\n\nThis will set all error counters (Gap1, Gap2, Water) back to zero.\nThis action cannot be undone.";

        if (!confirm(confirmMessage)) {
          return;
        }

        const btn = document.getElementById("resetStatsBtn");
        btn.disabled = true;
        btn.textContent = "Resetting...";

        fetch("/api/reset-statistics", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              showNotification("Statistics reset successfully", "success");
              loadStatistics();
            } else {
              showNotification("Failed to reset statistics", "error");
            }
          })
          .catch((error) => {
            showNotification("Network error", "error");
          })
          .finally(() => {
            btn.disabled = false;
            btn.textContent = "Reset Statistics";
          });
      }

      function manualLoadStatistics() {
        const btn = document.getElementById("loadStatsBtn");
        btn.disabled = true;
        btn.textContent = "Loading...";

        loadStatistics();

        setTimeout(() => {
          btn.disabled = false;
          btn.textContent = "Load Statistics";
        }, 1000);
      }

      function loadDailyVolume() {
        fetch("/api/daily-volume")
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              document.getElementById("currentDailyVolume").textContent =
                data.daily_volume;
              document.getElementById("maxDailyVolume").textContent =
                data.max_volume;
            }
          })
          .catch((error) => {
            console.error("Failed to load daily volume:", error);
          });
      }

      function resetDailyVolume() {
        const confirmMessage =
          "Are you sure you want to reset Daily Volume to 0ml?\n\nCurrent volume will be cleared.\nThis action cannot be undone.";

        if (!confirm(confirmMessage)) {
          return;
        }

        const btn = document.getElementById("resetDailyVolumeBtn");
        btn.disabled = true;
        btn.textContent = "Resetting...";

        fetch("/api/reset-daily-volume", { method: "POST" })
          .then((response) => response.json())
          .then((data) => {
            if (data.success) {
              showNotification(
                "Daily volume reset to 0ml successfully",
                "success"
              );
              loadDailyVolume();
            } else {
              const errorMsg = data.error || "Reset failed";
              showNotification("Failed to reset: " + errorMsg, "error");
            }
          })
          .catch((error) => {
            showNotification("Network error", "error");
          })
          .finally(() => {
            btn.disabled = false;
            btn.textContent = "Reset Daily Volume";
          });
      }

      setInterval(loadPumpGlobalState, 30000);

      loadDailyVolume();
      setInterval(loadDailyVolume, 10000);

      loadVolumePerSecond();
      loadStatistics();

    </script>
  </body>
</html>

)rawliteral";

String getLoginHTML() {
    return String(LOGIN_HTML);
}

String getDashboardHTML() {
    return String(DASHBOARD_HTML);
}
