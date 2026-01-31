

#include "html_pages.h"

const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Top Off Water - Login</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-input: #1e293b;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --radius: 12px;
            --radius-sm: 8px;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .login-box {
            background: var(--bg-card);
            border: 1px solid var(--border);
            padding: 40px;
            border-radius: var(--radius);
            box-shadow: 0 4px 24px rgba(0,0,0,0.4);
            width: 100%;
            max-width: 400px;
            margin: 20px;
            position: relative;
            z-index: 1;
        }

        .logo {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 12px;
            margin-bottom: 30px;
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border-radius: var(--radius-sm);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }

        .form-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--text-secondary);
            font-size: 0.875rem;
        }

        input[type="password"] {
            width: 100%;
            padding: 12px 16px;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            font-size: 1rem;
            color: var(--text-primary);
        }

        input[type="password"]:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .login-btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            color: var(--bg-primary);
            border: none;
            border-radius: var(--radius-sm);
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }

        .login-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(56, 189, 248, 0.3);
        }

        .alert {
            padding: 12px 16px;
            margin: 15px 0;
            border-radius: var(--radius-sm);
            display: none;
            font-size: 0.875rem;
        }

        .alert.error {
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid rgba(239, 68, 68, 0.3);
            color: var(--accent-red);
        }

        .info {
            margin-top: 24px;
            padding: 16px;
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            font-size: 0.75rem;
            color: var(--text-muted);
        }

        .info strong {
            color: var(--text-secondary);
        }
    </style>
</head>
<body>
    <div class="login-box">
        <div class="logo">
            <div class="logo-icon">
                <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
            </div>
            <h1>Top Off Water <span>– System</span></h1>
        </div>
        <form id="loginForm">
            <div class="form-group">
                <label for="password">Administrator Password</label>
                <input type="password" id="password" name="password" required />
            </div>
            <button type="submit" class="login-btn">Login</button>
        </form>
        <div id="error" class="alert error"></div>
        <div class="info">
            <strong>Security Features:</strong><br />
            • Session-based authentication<br />
            • Rate limiting & IP filtering<br />
            • VPS event logging
        </div>
    </div>
    <script>
        document.getElementById("loginForm").addEventListener("submit", function(e) {
            e.preventDefault();
            const password = document.getElementById("password").value;
            const errorDiv = document.getElementById("error");

            fetch("api/login", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "password=" + encodeURIComponent(password),
            })
            .then((response) => response.json())
            .then((data) => {
                if (data.success) {
                    window.location.href = "/";
                } else {
                    errorDiv.textContent = data.error || "Login failed";
                    errorDiv.style.display = "block";
                }
            })
            .catch((error) => {
                errorDiv.textContent = "Connection error - Check if device is running";
                errorDiv.style.display = "block";
            });
        });
    </script>
</body>
</html>
)rawliteral";

const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Top Off Water</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-card-hover: #1a2332;
            --bg-input: #1e293b;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --accent-yellow: #eab308;
            --shadow: 0 4px 24px rgba(0,0,0,0.4);
            --radius: 12px;
            --radius-sm: 8px;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            line-height: 1.5;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 16px;
            position: relative;
            z-index: 1;
        }

        /* Header */
        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 0 24px;
            border-bottom: 1px solid var(--border);
            margin-bottom: 24px;
        }

        .logo {
            display: flex;
            align-items: center;
            gap: 12px;
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border-radius: var(--radius-sm);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
            letter-spacing: -0.02em;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }
        

        .btn-back {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
            padding: 8px 16px;
            border-radius: var(--radius-sm);
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn-back:hover {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        /* Notifications */
        .notifications {
            position: fixed;
            top: 16px;
            right: 16px;
            z-index: 100;
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .alert {
            padding: 12px 16px;
            border-radius: var(--radius-sm);
            font-size: 0.875rem;
            font-weight: 500;
            animation: slideIn 0.3s ease;
        }

        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }

        .alert.success {
            background: rgba(34, 197, 94, 0.15);
            border: 1px solid rgba(34, 197, 94, 0.3);
            color: var(--accent-green);
        }

        .alert.error {
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid rgba(239, 68, 68, 0.3);
            color: var(--accent-red);
        }

        /* Cards */
        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 20px;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
        }

        .card-header {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 16px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border);
        }

        .card-header h2 {
            font-size: 0.9rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .stat-column h3{
            font-size: 0.9rem;
            font-weight: 600;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .card-header-icon {
            width: 28px;
            height: 28px;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .card-header-icon svg {
            width: 16px;
            height: 16px;
        }

        /* ===== FIRST CARD: System Status ===== */
        .status-grid {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .status-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
        }

        @media (max-width: 600px) {
            .status-row {
                grid-template-columns: 1fr;
            }
        }

        .status-badge {
            display: flex;
            flex-direction: column;
            align-items: center;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 10px 14px;
            position: relative;
        }

        .status-badge .label {
            font-size: 0.65rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.08em;
            color: var(--text-muted);
            margin-bottom: 4px;
        }

        .status-badge .value {
            font-family: 'Courier New', monospace;
            font-size: 0.875rem;
            font-weight: 600;
        }

        .status-badge.ok {
            background: rgba(34, 197, 94, 0.15);
            border-color: rgba(34, 197, 94, 0.3);
        }
        .status-badge.ok .value { color: var(--accent-green); }

        .status-badge.off .value { color: var(--text-muted); }

        .status-badge.idle .value { color: var(--accent-yellow); }

        .status-badge.active {
            background: rgba(34, 197, 94, 0.15);
            border-color: rgba(34, 197, 94, 0.3);
        }
        .status-badge.active .value { color: var(--accent-green); }

        .status-badge.warning {
            background: rgba(234, 179, 8, 0.15);
            border-color: rgba(234, 179, 8, 0.3);
        }
        .status-badge.warning .value { color: var(--accent-yellow); }

        .status-badge.danger {
            background: rgba(249, 115, 22, 0.15);
            border-color: rgba(249, 115, 22, 0.3);
        }
        .status-badge.danger .value { color: var(--accent-orange); }

        .status-badge.error {
            background: rgba(239, 68, 68, 0.15);
            border-color: rgba(239, 68, 68, 0.3);
        }
        .status-badge.error .value { color: var(--accent-red); }

        .status-badge.ok::before {
            content: '';
            position: absolute;
            top: 8px;
            right: 8px;
            width: 6px;
            height: 6px;
            background: var(--accent-green);
            border-radius: 50%;
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.6; }
        }

        .status-message {
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 12px 16px;
            text-align: center;
            display: flex;
            flex-direction: column;
            justify-content: center;
        }

        .status-message .main {
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 2px;
        }

        .status-message .sub {
            font-size: 0.8rem;
            color: var(--text-muted);
        }

        .info-item {
            display: flex;
            align-items: center;
            flex-direction: column;
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 12px;
        }

        .info-item .label {
            font-size: 0.7rem;
            font-weight: 500;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-muted);
            margin-bottom: 4px;
        }

        .info-item .value {
            font-family: 'Courier New', monospace;
            font-size: 0.9rem;
            font-weight: 500;
            color: var(--text-primary);
        }

        .info-item .hint {
            font-size: 0.7rem;
            color: var(--text-muted);
            margin-top: 2px;
        }

        .info-item.connected .value { color: var(--accent-green); }
        .info-item.rtc-error .value { color: var(--accent-red); }

        /* ===== SECOND CARD: Pump Control ===== */
        .pump-controls {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }

        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            height: 48px;
            gap: 8px;
            padding: 14px 20px;
            border-radius: var(--radius-sm);
            font-family: inherit;
            font-size: 0.9rem;
            font-weight: 600;
            border: none;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none !important;
        }

        .btn svg {
            width: 18px;
            height: 18px;
        }

        .btn-primary {
            background: rgba(34, 197, 94, 0.15);
            border: 1px solid rgba(34, 197, 94, 0.3);
            color: var(--accent-green);
        }

        .btn-primary:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 2px 8px rgba(34, 197, 94, 0.3);
        }

        /* Stan OFF dla przycisków bistabilnych */
        .btn-off {
            background: var(--bg-input);
            border: 1px solid rgba(34, 197, 94, 0.3);
            color: var(--text-secondary);
        }

        .btn-off:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-secondary {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
        }

        .btn-secondary:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-small {
            padding: 10px 16px;
            font-size: 0.8rem;
            margin-top: 10px;
            /* margin-right: 4px; */
        }

        .btn-small:nth-of-type(2){
            margin-left: 10px;
        }

        /* ===== THIRD CARD: Statistics ===== */
        .stats-columns {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
            align-items: start;
        }

        @media (max-width: 600px) {
            .stats-columns {
                grid-template-columns: 1fr;
            }
        }

        .stat-column {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 16px;
            display: flex;
            min-height: 245px;
            flex-direction: column;
            gap: 12px;
        }

        .stat-column .btn {
            width: 100%;
        }

        .stat-content {
            display: flex;
            flex-direction: column;
            gap: 6px;
        }

        .stat-errors {
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

         .stat-available{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

        .stat-daily{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }
        
        .stat-line {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.8rem;
        }

        .stat-line .stat-label {
            color: var(--text-muted);
        }

        .stat-line .stat-value {
            font-family: 'Courier New', monospace;
            font-weight: 600;
            color: var(--accent-green);
        }

        .stat-line .stat-value.neutral {
            color: var(--text-primary);
        }

        /* Volume indicator */
        .volume-indicator {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .volume-bar {
            height: 8px;
            background: var(--bg-primary);
            border-radius: 4px;
            overflow: hidden;
        }

        .volume-bar-fill {
            height: 100%;
            width: 0%;
            background: linear-gradient(90deg, var(--accent-cyan), var(--accent-blue));
            border-radius: 4px;
            transition: width 0.3s;
        }

        .volume-text {
            font-family: 'Courier New', monospace;
            font-size: 0.85rem;
            color: var(--text-primary);
            text-align: center;
        }

        /* ===== FOURTH CARD: Pump Settings ===== */
        .settings-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 16px;
            align-items: start;
        }

        @media (max-width: 600px) {
            .settings-row {
                grid-template-columns: 1fr;
            }
        }

        .setting-item {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .setting-item.input-group {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 12px;
        }

        .setting-item label {
            font-size: 0.75rem;
            font-weight: 500;
            text-transform: uppercase;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-muted);
        }

        input[type="text"],
        input[type="number"] {
            background: var(--bg-primary);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 10px 14px;
            font-family: 'Courier New', monospace;
            font-size: 1rem;
            color: var(--text-primary);
            width: 100%;
            text-align: center;
        }

        input:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .current-value {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-align: center;
            margin-top: 4px;
        }

        /* Footer */
        .footer-info {
            text-align: center;
            padding: 16px;
            color: var(--text-muted);
            font-size: 0.75rem;
        }

        /* Cycle History Table */
        .cycle-table-wrap {
            // overflow-x: auto;
            // -webkit-overflow-scrolling: touch;
            // max-height: 420px;
            overflow-y: auto;
            margin-top: 12px;
        }
        .cycle-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.72rem;
            white-space: nowrap;
        }
        .cycle-table th {
            background: var(--bg-input);
            color: var(--text-muted);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.04em;
            padding: 7px 8px;
            text-align: center;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 1;
        }
        .cycle-table td {
            padding: 5px 8px;
            text-align: center;
            border-bottom: 1px solid rgba(45, 58, 79, 0.4);
            color: var(--text-secondary);
            font-family: 'Courier New', monospace;
        }
        .cycle-table tr:hover td {
            background: rgba(56, 189, 248, 0.06);
        }
        .ct-ok { color: var(--accent-green); }
        .ct-fail { color: var(--accent-red); font-weight: 600; }
        .ct-warn { color: var(--accent-yellow); }
        .ct-n { color: var(--text-primary); }

        /* Modal System */
        .modal-overlay {
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0, 0, 0, 0.7);
            backdrop-filter: blur(4px);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000;
            opacity: 0;
            visibility: hidden;
            transition: all 0.2s ease;
        }
        .modal-overlay.show {
            opacity: 1;
            visibility: visible;
        }
        .modal-box {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 14px;
            padding: 24px;
            max-width: 360px;
            width: 90%;
            transform: scale(0.9);
            transition: transform 0.2s ease;
        }
        .modal-overlay.show .modal-box {
            transform: scale(1);
        }
        .modal-icon {
            width: 48px;
            height: 48px;
            margin: 0 auto 16px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .modal-icon svg { width: 24px; height: 24px; }
        .modal-icon.ok   { background: rgba(34, 197, 94, 0.15); }
        .modal-icon.ok svg   { color: var(--accent-green); }
        .modal-icon.err  { background: rgba(239, 68, 68, 0.15); }
        .modal-icon.err svg  { color: var(--accent-red); }
        .modal-icon.warn { background: rgba(234, 179, 8, 0.15); }
        .modal-icon.warn svg { color: var(--accent-yellow); }
        .modal-icon.info { background: rgba(56, 189, 248, 0.15); }
        .modal-icon.info svg { color: var(--accent-blue); }
        .modal-title {
            font-size: 0.95rem;
            font-weight: 700;
            text-align: center;
            margin-bottom: 8px;
            color: var(--text-primary);
        }
        .modal-text {
            font-size: 0.7rem;
            text-align: center;
            color: var(--text-secondary);
            margin-bottom: 20px;
            line-height: 1.5;
        }
        .modal-actions {
            display: flex;
            gap: 10px;
        }
        .modal-actions .btn {
            flex: 1;
            height: 40px;
            padding: 0 16px;
            border-radius: 6px;
            font-size: 0.7rem;
        }
        .modal-actions .btn-primary {
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border: none;
            color: var(--bg-primary);
        }
        .modal-actions .btn-primary:hover:not(:disabled) {
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(34, 211, 213, 0.3);
        }
        .modal-actions .btn-secondary:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header>
            <div class="logo">
                <div class="logo-icon">
                    <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
                </div>
                <h1>Top Off Water</h1>
            </div>
            <button class="btn-back" onclick="logout()">Back</button>
        </header>

        <!-- Notifications container -->
        <div id="notifications" class="notifications"></div>

        <!-- FIRST CARD: System Status -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(56, 189, 248, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-blue);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>
                </div>
                <h2>System Status</h2>
            </div>

            <div class="status-grid">
                <!-- Row 1: sensor1, sensor2, pump -->
                <div class="status-row">
                    <div class="status-badge off" id="sensor1Badge">
                        <span class="label">Sensor 1</span>
                        <span class="value">OFF</span>
                    </div>
                    <div class="status-badge off" id="sensor2Badge">
                        <span class="label">Sensor 2</span>
                        <span class="value">OFF</span>
                    </div>
                    <div class="status-badge idle" id="pumpBadge">
                        <span class="label">Pump</span>
                        <span class="value">IDLE</span>
                    </div>
                </div>

                <!-- Row 2: system, status-message, wifi status -->
                <div class="status-row">
                    <div class="status-badge ok" id="systemBadge">
                        <span class="label">System</span>
                        <span class="value">OK</span>
                    </div>
                    <div class="status-message">
                        <div class="main" id="processDescription">IDLE - Waiting for sensors</div>
                        <div class="sub" id="processTime">—</div>
                    </div>
                    <div class="info-item connected" id="wifiItem">
                        <span class="label">WiFi Status</span>
                        <span class="value" id="wifiStatus">Loading...</span>
                    </div>
                </div>

                <!-- Row 3: RTC Time, Free Memory, Uptime -->
                <div class="status-row">
                    <div class="info-item" id="rtcItem">
                        <span class="label">RTC Time (UTC)</span>
                        <span class="value" id="rtcTime">Loading...</span>
                        <span class="hint" id="rtcHint"></span>
                    </div>
                    <div class="info-item">
                        <span class="label">Free Memory</span>
                        <span class="value" id="freeHeap">Loading...</span>
                    </div>
                    <div class="info-item">
                        <span class="label">Uptime</span>
                        <span class="value" id="uptime">Loading...</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- SECOND CARD: Pump Control -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(34, 197, 94, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-green);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 14.5v-9l6 4.5-6 4.5z"/></svg>
                </div>
                <h2>System Control</h2>
            </div>

            <div class="pump-controls">
                <button id="manualCycleBtn" class="btn btn-off" onclick="toggleManualCycle()">
                    Manual Cycle Off
                </button>
                <button id="systemToggleBtn" class="btn btn-primary" onclick="toggleSystem()">
                    System On
                </button>
            </div>
        </div>

        <!-- THIRD CARD: Statistics -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(234, 179, 8, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-yellow);" viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zM9 17H7v-7h2v7zm4 0h-2V7h2v10zm4 0h-2v-4h2v4z"/></svg>
                </div>
                <h2>System Setting</h2>
            </div>

 

            <div class="stats-columns">
                
                <!-- Column 1: Available Volume -->
                <div class="stat-column">
                    <h3>Available Water</h3>
                    <div class="stat-content">
                        <div class="volume-indicator">
                            <div class="volume-bar">
                                <div class="volume-bar-fill" id="availableBarFill"></div>
                            </div>
                            <div class="volume-text" id="availableText">0 ml / 10000 ml</div>
                        </div>
                    </div>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="availableVolumeInput" min="100" max="10000" step="100" placeholder="ml">
                    </div>
                    <div class="stat-available">
                        <button class="btn btn-secondary btn-small" onclick="setAvailableVolume()">Set</button>
                        <button class="btn btn-secondary btn-small" onclick="refillAvailableVolume()">Refill</button>
                    </div>
                </div>

                <!-- Column 2: Daily Volume -->
                <div class="stat-column">
                    <h3>Daily Refill Limit</h3>
                    <div class="stat-content">
                        <div class="volume-indicator">
                            <div class="volume-bar">
                                <div class="volume-bar-fill" id="volumeBarFill"></div>
                            </div>
                            <div class="volume-text" id="volumeText">0 ml / 2000 ml</div>
                        </div>
                    </div>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="dailyLimitInput" min="100" max="10000" step="100" placeholder="ml">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="setDailyLimit()">Set</button>
                        <button id="resetDailyVolumeBtn" class="btn btn-secondary btn-small" onclick="resetDailyVolume()">Reset</button>
                    </div>
                </div>
            </div>

            
            
        </div>

        <!-- CYCLE HISTORY CARD -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(34, 211, 213, 0.15);">
                    <svg fill="currentColor" style="color: #22d3d5;" viewBox="0 0 24 24"><path d="M13 3c-4.97 0-9 4.03-9 9H1l3.89 3.89.07.14L9 12H6c0-3.87 3.13-7 7-7s7 3.13 7 7-3.13 7-7 7c-1.93 0-3.68-.79-4.94-2.06l-1.42 1.42C8.27 19.99 10.51 21 13 21c4.97 0 9-4.03 9-9s-4.03-9-9-9zm-1 5v5l4.28 2.54.72-1.21-3.5-2.08V8H12z"/></svg>
                </div>
                <h2>Cycle History</h2>
            </div>
            <button class="btn btn-secondary" onclick="loadCycleHistory()" id="loadCyclesBtn" style="margin-bottom:4px;">Load History</button>
            <div class="cycle-table-wrap">
                <table class="cycle-table">
                    <thead>
                        <tr>
                            <th>Date/Time</th>
                            <th>S1 Deb</th>
                            <th>S2 Deb</th>
                            <th>Debounce</th>
                            <th>Gap1</th>
                            <th>Att</th>
                            <th>S1 Rel</th>
                            <th>S2 Rel</th>
                            <th>Vol</th>
                            <th>Pump</th>
                            <th>Alarm</th>
                        </tr>
                    </thead>
                    <tbody id="cycleTableBody">
                        <tr><td colspan="11" style="color:var(--text-muted);">Click "Load History"</td></tr>
                    </tbody>
                </table>
            </div>
        </div>

        <!-- FOURTH CARD: Pump Setting -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(249, 115, 22, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-orange);" viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
                </div>
                <h2>Pump Calibration</h2>
            </div>


            <div class="settings-row">
                <!-- Element 1: Pump Calibration button -->
                <div class="setting-item">
                    <button id="extendedBtn" class="btn btn-secondary" onclick="triggerExtendedPump()">Start Calibration (30s)</button>
                </div>

                <!-- Element 2: Input z opisem -->
                <div class="setting-item input-group">
                    <label for="volumePerSecond"  style="text-align: center;">Mililiters per Second</label>
                    <input class="volumePerSecond" type="number" id="volumePerSecond" min="0.1" max="50.0" step="0.1" value="1.0">
                </div>

                <!-- Element 3: Update Setting + current value -->
                <div class="setting-item">
                    <button class="btn btn-primary" onclick="updateVolumePerSecond()">Update Setting</button>
                    <span class="current-value" id="volumeStatus">Current: — ml/s</span>
                </div>
            </div>
        </div>

        <!-- Footer -->
        <div class="footer-info">
            Top Off Water System • v2.1
        </div>
    </div>

    <!-- Modal -->
    <div class="modal-overlay" id="alertModal">
        <div class="modal-box">
            <div class="modal-icon" id="alertIcon"></div>
            <div class="modal-title" id="alertTitle"></div>
            <div class="modal-text" id="alertText"></div>
            <div class="modal-actions" id="alertActions"></div>
        </div>
    </div>

    <script>

        // ============================================
        // MODAL SYSTEM
        // ============================================
        const MODAL_ICONS = {
            ok:   '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="20 6 9 17 4 12"/></svg>',
            err:  '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>',
            warn: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>',
            info: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>'
        };

        let alertCallback = null;

        function showAlert(title, msg, type, cb) {
            document.getElementById('alertIcon').className = 'modal-icon ' + type;
            document.getElementById('alertIcon').innerHTML = MODAL_ICONS[type] || MODAL_ICONS.info;
            document.getElementById('alertTitle').textContent = title;
            document.getElementById('alertText').textContent = msg;
            document.getElementById('alertActions').innerHTML =
                '<button class="btn btn-primary" onclick="closeAlert()">OK</button>';
            alertCallback = cb || null;
            document.getElementById('alertModal').classList.add('show');
        }

        function showConfirm(title, msg, type, onConfirm) {
            document.getElementById('alertIcon').className = 'modal-icon ' + type;
            document.getElementById('alertIcon').innerHTML = MODAL_ICONS[type] || MODAL_ICONS.info;
            document.getElementById('alertTitle').textContent = title;
            document.getElementById('alertText').textContent = msg;
            document.getElementById('alertActions').innerHTML =
                '<button class="btn btn-secondary" onclick="closeAlert()">Cancel</button>' +
                '<button class="btn btn-primary" onclick="closeAlert(true)">Confirm</button>';
            alertCallback = onConfirm;
            document.getElementById('alertModal').classList.add('show');
        }

        function closeAlert(confirmed) {
            document.getElementById('alertModal').classList.remove('show');
            if (confirmed && alertCallback) alertCallback();
            alertCallback = null;
        }

        document.addEventListener('click', function(e) {
            if (e.target.id === 'alertModal') closeAlert();
        });

        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape') closeAlert();
        });

        // ============================================
        // SESSION EXPIRY HANDLING (for VPS proxy)
        // ============================================

        let sessionExpired = false;
        let pollingIntervals = [];

        function handleSessionExpired() {
            if (sessionExpired) return;
            sessionExpired = true;

            // Stop all polling intervals
            pollingIntervals.forEach(id => clearInterval(id));
            pollingIntervals = [];
            if (pumpCheckInterval) {
                clearInterval(pumpCheckInterval);
                pumpCheckInterval = null;
            }

            // Show overlay
            const overlay = document.createElement('div');
            overlay.innerHTML = `
                <div style="
                    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
                    background: rgba(0,0,0,0.85); z-index: 9999;
                    display: flex; justify-content: center; align-items: center;
                ">
                    <div style="
                        background: white; padding: 40px; border-radius: 12px;
                        text-align: center; max-width: 400px; margin: 20px;
                    ">
                        <h2 style="color: #e74c3c; margin-bottom: 15px;">Session Expired</h2>
                        <p style="color: #666; margin-bottom: 25px;">Your session has expired. Please log in again.</p>
                        <a href="/login" style="
                            display: inline-block; padding: 12px 30px;
                            background: #3498db; color: white;
                            text-decoration: none; border-radius: 8px;
                            font-weight: bold;
                        ">Back to Login</a>
                    </div>
                </div>
            `;
            document.body.appendChild(overlay);
            console.log("Session expired - polling stopped");
        }

        async function secureFetch(url, options = {}) {
            if (sessionExpired) return null;

            try {
                const response = await fetch(url, options);
                
                // Check for auth failure (401 or redirect to login)
                if (response.status === 401 || 
                    response.redirected && response.url.includes('/login')) {
                    handleSessionExpired();
                    return null;
                }
                return response;
            } catch (error) {
                console.error('Fetch error:', error);
                return null;
            }
        }


        // ============================================
        // STATE TRACKING
        // ============================================
        let systemEnabled = true;
        let manualCycleActive = false;
        let pumpCheckInterval = null;
        let maxDailyVolume = 2000;

        // ============================================
        // NOTIFICATION HELPER
        // ============================================
        function showNotification(message, type) {
            const notifications = document.getElementById("notifications");
            const alert = document.createElement("div");
            alert.className = "alert " + type;
            alert.textContent = message;
            notifications.appendChild(alert);

            setTimeout(() => {
                if (notifications.contains(alert)) {
                    alert.style.opacity = '0';
                    alert.style.transform = 'translateX(100%)';
                    setTimeout(() => notifications.removeChild(alert), 300);
                }
            }, 4000);
        }

        // ============================================
        // SYSTEM TOGGLE (bistable ON/OFF)
        // ============================================
        function toggleSystem() {
            const btn = document.getElementById("systemToggleBtn");
            btn.disabled = true;

            fetch("api/system-toggle", { method: "POST" })
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled, data.remaining_seconds);
                        showNotification(data.message, "success");
                    } else {
                        showNotification("Failed to toggle system", "error");
                    }
                })
                .catch((error) => {
                    showNotification("Network error", "error");
                })
                .finally(() => {
                    btn.disabled = false;
                });
        }

        function updateSystemToggleButton(enabled, remainingSeconds) {
            const btn = document.getElementById("systemToggleBtn");
            if (!btn) return;

            if (enabled) {
                btn.textContent = "System On";
                btn.className = "btn btn-primary";
            } else {
                if (remainingSeconds && remainingSeconds > 0) {
                    const minutes = Math.floor(remainingSeconds / 60);
                    const seconds = remainingSeconds % 60;
                    btn.textContent = "System Off (" + minutes + ":" + seconds.toString().padStart(2, "0") + ")";
                } else {
                    btn.textContent = "System Off";
                }
                btn.className = "btn btn-off";
            }
        }

        function loadSystemState() {
            secureFetch("api/system-toggle")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled, data.remaining_seconds);
                    }
                })
                .catch((error) => console.error("Failed to load system state:", error));
        }

        // ============================================
        // MANUAL CYCLE TOGGLE (bistable with auto-reset)
        // ============================================
        function toggleManualCycle() {
            const btn = document.getElementById("manualCycleBtn");
            
            if (manualCycleActive) {
                btn.disabled = true;
                btn.textContent = "Stopping...";

                fetch("api/pump/stop", { method: "POST" })
                    .then((response) => response.json())
                    .then((data) => {
                        if (data.success) {
                            manualCycleActive = false;
                            updateManualCycleButton(false);
                            showNotification("Manual cycle stopped", "success");
                            stopPumpMonitoring();
                        } else {
                            showNotification("Failed to stop pump", "error");
                        }
                    })
                    .catch(() => showNotification("Connection error", "error"))
                    .finally(() => {
                        btn.disabled = false;
                    });
            } else {
                btn.disabled = true;
                btn.textContent = "Starting...";

                fetch("api/pump/normal", { method: "POST" })
                    .then((response) => response.json())
                    .then((data) => {
                        if (data.success) {
                            manualCycleActive = true;
                            updateManualCycleButton(true);
                            showNotification("Manual cycle started for " + data.duration + "s", "success");
                            startPumpMonitoring();
                        } else {
                            showNotification("Failed to start pump", "error");
                        }
                    })
                    .catch(() => showNotification("Connection error", "error"))
                    .finally(() => {
                        btn.disabled = false;
                    });
            }
        }

        function updateManualCycleButton(isActive) {
            const btn = document.getElementById("manualCycleBtn");
            if (!btn) return;

            if (isActive) {
                btn.textContent = "Manual Cycle On";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = "Manual Cycle Off";
                btn.className = "btn btn-off";
            }
        }

        function startPumpMonitoring() {
            if (pumpCheckInterval) clearInterval(pumpCheckInterval);
            
            pumpCheckInterval = setInterval(() => {
                secureFetch("api/status")
                    .then((response) => {
                        if (!response) return null;
                        return response.json();
                    })
                    .then((data) => {
                        if (!data) return;
                        if (!data.pump_active && manualCycleActive) {
                            manualCycleActive = false;
                            updateManualCycleButton(false);
                            stopPumpMonitoring();
                        }
                    })
                    .catch(() => {});
            }, 1000);
        }

        function stopPumpMonitoring() {
            if (pumpCheckInterval) {
                clearInterval(pumpCheckInterval);
                pumpCheckInterval = null;
            }
        }

        // ============================================
        // EXTENDED PUMP (Calibration)
        // ============================================
        function triggerExtendedPump() {
            const btn = document.getElementById("extendedBtn");
            btn.disabled = true;
            btn.textContent = "Starting...";

            fetch("api/pump/extended", { method: "POST" })
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        showNotification("Calibration pump started for " + data.duration + "s", "success");
                    } else {
                        showNotification("Failed to start pump", "error");
                    }
                })
                .catch(() => showNotification("Connection error", "error"))
                .finally(() => {
                    btn.disabled = false;
                    btn.textContent = "Pump Calibration (30s)";
                });
        }

        // ============================================
        // VOLUME SETTINGS
        // ============================================
        function loadVolumePerSecond() {
            fetch("api/pump-settings")
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        document.getElementById("volumePerSecond").value = parseFloat(data.volume_per_second).toFixed(1);
                        document.getElementById("volumeStatus").textContent = "Current: " + parseFloat(data.volume_per_second).toFixed(1) + " ml/s";
                    }
                })
                .catch((error) => {
                    console.error("Failed to load volume setting:", error);
                });
        }

        function updateVolumePerSecond() {
            const volumeInput = document.getElementById("volumePerSecond");
            const statusSpan = document.getElementById("volumeStatus");
            const volumeValue = parseFloat(volumeInput.value);

            if (volumeValue < 0.1 || volumeValue > 50.0) {
                statusSpan.textContent = "Error: 0.1-50.0 range";
                return;
            }

            showConfirm("Change Setting?", "Change Volume per Second to " + volumeValue.toFixed(1) + " ml/s?", "info", function() {
                statusSpan.textContent = "Updating...";

                const formData = new FormData();
                formData.append("volume_per_second", volumeValue);

                fetch("api/pump-settings", { method: "POST", body: formData })
                    .then((response) => response.json())
                    .then((data) => {
                        if (data.success) {
                            statusSpan.textContent = "Current: " + volumeValue.toFixed(1) + " ml/s";
                            showNotification("Volume updated to " + volumeValue.toFixed(1) + " ml/s", "success");
                        } else {
                            statusSpan.textContent = "Error: " + (data.error || "Update failed");
                        }
                    })
                    .catch((error) => {
                        statusSpan.textContent = "Network error";
                    });
            });
        }

        // ============================================
        // STATUS BADGE UPDATES
        // ============================================
        function updateSensorBadge(badgeId, isActive) {
            const badge = document.getElementById(badgeId);
            if (!badge) return;

            const valueSpan = badge.querySelector(".value");
            badge.className = "status-badge";

            if (isActive) {
                badge.classList.add("active");
                valueSpan.textContent = "ON";
            } else {
                badge.classList.add("off");
                valueSpan.textContent = "OFF";
            }
        }

        function updatePumpBadge(badgeId, isActive, attempt) {
            const badge = document.getElementById(badgeId);
            if (!badge) return;

            const valueSpan = badge.querySelector(".value");
            badge.className = "status-badge";

            if (!isActive) {
                badge.classList.add("idle");
                valueSpan.textContent = "IDLE";
            } else {
                if (attempt === 1) {
                    badge.classList.add("active");
                } else if (attempt === 2) {
                    badge.classList.add("warning");
                } else if (attempt >= 3) {
                    badge.classList.add("danger");
                } else {
                    badge.classList.add("active");
                }
                valueSpan.textContent = "ACTIVE";
            }
        }

        function updateSystemBadge(badgeId, hasError, isDisabled) {
            const badge = document.getElementById(badgeId);
            if (!badge) return;

            const valueSpan = badge.querySelector(".value");
            badge.className = "status-badge";

            if (hasError) {
                badge.classList.add("error");
                valueSpan.textContent = "ERROR";
            } else if (isDisabled) {
                badge.classList.add("off");
                valueSpan.textContent = "OFF";
            } else {
                badge.classList.add("ok");
                valueSpan.textContent = "OK";
            }
        }

        function formatTime(seconds) {
            if (!seconds || seconds <= 0) return "—";
            if (seconds < 60) return seconds + " sec";
            const minutes = Math.floor(seconds / 60);
            const secs = seconds % 60;
            if (secs === 0) return minutes + " min";
            return minutes + "m " + secs + "s";
        }

        function formatUptime(milliseconds) {
            const seconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            return hours + "h " + minutes + "m";
        }

        // ============================================
        // MAIN STATUS UPDATE
        // ============================================
        function updateStatus() {
            secureFetch("api/status")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    
                    // Badges
                    updateSensorBadge("sensor1Badge", data.sensor1_active);
                    updateSensorBadge("sensor2Badge", data.sensor2_active);
                    updatePumpBadge("pumpBadge", data.pump_active, data.pump_attempt || 0);
                    updateSystemBadge("systemBadge", data.system_error, data.system_disabled);

                    // Process status
                    document.getElementById("processDescription").textContent = data.state_description || "IDLE - Waiting for sensors";
                    document.getElementById("processTime").textContent = data.remaining_seconds > 0 ? "Remaining: " + formatTime(data.remaining_seconds) : "—";

                    // System toggle sync
                    if (typeof data.system_disabled !== 'undefined') {
                        systemEnabled = !data.system_disabled;
                        updateSystemToggleButton(!data.system_disabled, data.system_remaining_seconds);
                    }

                    // Manual cycle sync
                    if (!data.pump_active && manualCycleActive) {
                        manualCycleActive = false;
                        updateManualCycleButton(false);
                    }

                    // WiFi status
                    const wifiItem = document.getElementById("wifiItem");
                    const wifiStatus = document.getElementById("wifiStatus");
                    wifiStatus.textContent = data.wifi_status || "Unknown";
                    if (data.wifi_connected) {
                        wifiItem.className = "info-item connected";
                    } else {
                        wifiItem.className = "info-item";
                    }

                    // RTC
                    const rtcItem = document.getElementById("rtcItem");
                    const rtcTime = document.getElementById("rtcTime");
                    const rtcHint = document.getElementById("rtcHint");
                    
                    rtcTime.textContent = data.rtc_time || "Error";
                    
                    if (data.rtc_battery_issue || data.rtc_needs_sync) {
                        rtcItem.className = "info-item rtc-error";
                        rtcHint.textContent = "⚠️ Battery issue - replace CR2032";
                    } else {
                        rtcItem.className = "info-item";
                        rtcHint.textContent = data.rtc_info || "";
                    }

                    // Memory & Uptime
                    document.getElementById("freeHeap").textContent = (data.free_heap / 1024).toFixed(1) + " KB";
                    document.getElementById("uptime").textContent = formatUptime(data.uptime);

                    // Disable buttons when needed
                    const manualBtn = document.getElementById("manualCycleBtn");
                    if (manualBtn) {
                        manualBtn.disabled = data.system_disabled;
                    }

                    const extendedBtn = document.getElementById("extendedBtn");
                    if (extendedBtn) {
                        extendedBtn.disabled = data.pump_active;
                    }
                })
                .catch((error) => {
                    console.error("Status update failed:", error);
                });
        }


        // ============================================
        // DAILY VOLUME FUNCTIONS
        // ============================================
        function loadDailyVolume() {
            secureFetch("api/daily-volume")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        const current = data.daily_volume;
                        maxDailyVolume = data.max_volume;
                        const percent = Math.min((current / maxDailyVolume) * 100, 100);
                        
                        document.getElementById("volumeBarFill").style.width = percent + "%";
                        document.getElementById("volumeText").textContent = current + " ml / " + maxDailyVolume + " ml";
                    }
                })
                .catch((error) => {
                    console.error("Failed to load daily volume:", error);
                });
        }

        function resetDailyVolume() {
            showConfirm("Reset Volume?", "Reset daily volume to 0ml?", "warn", function() {
                const btn = document.getElementById("resetDailyVolumeBtn");
                btn.disabled = true;
                btn.textContent = "Resetting...";

                fetch("api/reset-daily-volume", { method: "POST" })
                    .then((response) => response.json())
                    .then((data) => {
                        if (data.success) {
                            showNotification("Daily volume reset", "success");
                            loadDailyVolume();
                        } else {
                            showNotification("Failed: " + (data.error || "Unknown error"), "error");
                        }
                    })
                    .catch(() => showNotification("Network error", "error"))
                    .finally(() => {
                        btn.disabled = false;
                        btn.textContent = "Reset Daily Volume";
                    });
            });
        }


        // ============================================
        // AVAILABLE VOLUME FUNCTIONS
        // ============================================
        function loadAvailableVolume() {
            secureFetch("api/available-volume")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        const current = data.current_ml;
                        const max = data.max_ml;
                        const percent = Math.min((current / max) * 100, 100);
                        
                        const barFill = document.getElementById("availableBarFill");
                        const text = document.getElementById("availableText");
                        
                        barFill.style.width = percent + "%";
                        text.textContent = current + " ml / " + max + " ml";
                        
                        // Red color when empty
                        if (current === 0) {
                            text.style.color = "var(--accent-red)";
                            barFill.style.background = "var(--accent-red)";
                        } else {
                            text.style.color = "";
                            barFill.style.background = "";
                        }
                    }
                })
                .catch((error) => {
                    console.error("Failed to load available volume:", error);
                });
        }

        function setAvailableVolume() {
            const input = document.getElementById("availableVolumeInput");
            const value = parseInt(input.value);
            
            if (isNaN(value) || value < 100 || value > 10000) {
                showNotification("Value must be 100-10000 ml", "error");
                return;
            }
            
            fetch("api/set-available-volume", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        showNotification("Available volume set to " + value + " ml", "success");
                        input.value = "";
                        loadAvailableVolume();
                    } else {
                        showNotification("Failed: " + (data.error || "Unknown error"), "error");
                    }
                })
                .catch(() => showNotification("Network error", "error"));
        }

        function refillAvailableVolume() {
            showConfirm("Refill Volume?", "Refill available volume to max?", "info", function() {
                fetch("api/refill-available-volume", { method: "POST" })
                    .then((response) => response.json())
                    .then((data) => {
                        if (data.success) {
                            showNotification("Available volume refilled", "success");
                            loadAvailableVolume();
                        } else {
                            showNotification("Failed: " + (data.error || "Unknown error"), "error");
                        }
                    })
                    .catch(() => showNotification("Network error", "error"));
            });
        }

        // ============================================
        // SET DAILY LIMIT FUNCTION
        // ============================================
        function setDailyLimit() {
            const input = document.getElementById("dailyLimitInput");
            const value = parseInt(input.value);
            
            if (isNaN(value) || value < 100 || value > 10000) {
                showNotification("Value must be 100-10000 ml", "error");
                return;
            }
            
            fetch("api/set-fill-water-max", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        showNotification("Daily limit set to " + value + " ml", "success");
                        input.value = "";
                        loadDailyVolume();
                    } else {
                        showNotification("Failed: " + (data.error || "Unknown error"), "error");
                    }
                })
                .catch(() => showNotification("Network error", "error"));
        }

        // ============================================
        // LOGOUT
        // ============================================
        function logout() {
            fetch("api/logout", { method: "POST" }).then(() => {
                window.location.href = "/login";
            });
        }

        // ============================================
        // CYCLE HISTORY
        // ============================================
        function loadCycleHistory() {
            var btn = document.getElementById("loadCyclesBtn");
            btn.disabled = true;
            btn.textContent = "Loading...";

            secureFetch("api/cycle-history")
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (!data) return;
                    if (!data.success) {
                        showNotification(data.error || "Failed to load cycles", "error");
                        return;
                    }
                    var tb = document.getElementById("cycleTableBody");
                    tb.innerHTML = "";
                    if (!data.cycles || data.cycles.length === 0) {
                        tb.innerHTML = '<tr><td colspan="11" style="color:var(--text-muted);">No cycles recorded</td></tr>';
                        return;
                    }
                    data.cycles.forEach(function(c) {
                        var tr = document.createElement("tr");
                        var d = new Date(c.ts * 1000);
                        var ds = d.toLocaleDateString("en-GB",{day:"2-digit",month:"2-digit"})
                                 + " " + d.toLocaleTimeString("en-GB",{hour:"2-digit",minute:"2-digit"});

                        var al = "-", ac = "ct-n";
                        if (c.alarm === 1) { al = "ERR1"; ac = "ct-fail"; }
                        else if (c.alarm === 2) { al = "ERR2"; ac = "ct-fail"; }
                        else if (c.alarm === 3) { al = "ERR0"; ac = "ct-fail"; }

                        function f(ok) { return ok ? '<td class="ct-ok">OK</td>' : '<td class="ct-fail">FAIL</td>'; }
                        function r(v) { return v===1 ? '<td class="ct-ok">OK</td>' : v===-1 ? '<td class="ct-fail">FAIL</td>' : '<td class="ct-n">X</td>'; }

                        tr.innerHTML =
                            '<td class="ct-n">' + ds + '</td>' +
                            f(c.s1_deb) + f(c.s2_deb) + f(c.deb_ok) +
                            '<td class="ct-n">' + c.gap1_s + 's</td>' +
                            '<td class="' + (c.attempts > 1 ? 'ct-warn' : 'ct-n') + '">' + c.attempts + '</td>' +
                            r(c.s1_rel) + r(c.s2_rel) +
                            '<td class="ct-n">' + c.volume_ml + 'ml</td>' +
                            '<td class="ct-n">' + c.pump_s + 's</td>' +
                            '<td class="' + ac + '">' + al + '</td>';
                        tb.appendChild(tr);
                    });
                    showNotification("Loaded " + data.total + " cycles", "success");
                })
                .catch(function() { showNotification("Network error", "error"); })
                .finally(function() {
                    btn.disabled = false;
                    btn.textContent = "Load History";
                });
        }

        // ============================================
        // INITIALIZATION
        // ============================================

        // Register all intervals for cleanup on session expiry
        pollingIntervals.push(setInterval(updateStatus, 2000));
        pollingIntervals.push(setInterval(loadSystemState, 30000));
        pollingIntervals.push(setInterval(loadDailyVolume, 10000));
        pollingIntervals.push(setInterval(loadAvailableVolume, 10000));

        // Initial loads
        updateStatus();
        loadSystemState();
        loadVolumePerSecond();
        loadDailyVolume();
        loadAvailableVolume();
    </script>
</body>
</html>
)rawliteral";

