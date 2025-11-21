#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

// =================================================================
// IMPR3ZA TELEMETRY DASHBOARD v1.16-BetaRC (Battery Cells Update)
// =================================================================

const char dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>IMPR3ZA Telemetry v1.16</title>
  <style>
    :root {
      --bg-primary: #0a0e27;
      --bg-secondary: #151b3d;
      --bg-tertiary: #1e2749;
      --accent-primary: #00d4ff;
      --accent-secondary: #00ff88;
      --accent-warn: #ff8800;
      --accent-danger: #ff3333;
      --text-primary: #e8eaed;
      --text-secondary: #9aa0a6;
      --border-color: #2d3748;
      --success: #00ff88;
      --warning: #ff8800;
      --error: #ff3333;
      --info: #00d4ff;
      --gps-excellent: #00ff88;
      --gps-good: #00d4ff;
      --gps-acceptable: #ff8800;
      --gps-poor: #ff3333;
      --gps-nofix: #666;
    }

    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    body {
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
      background: var(--bg-primary);
      color: var(--text-primary);
      line-height: 1.6;
      overflow-x: hidden;
    }

    .header {
      background: var(--bg-secondary);
      border-bottom: 2px solid var(--accent-primary);
      padding: 1rem 2rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
    }

    .header-title {
      font-size: 1.5rem;
      font-weight: 700;
      color: var(--accent-primary);
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }

    .fw-version {
      font-size: 0.8rem;
      color: var(--text-secondary);
      font-weight: 400;
    }

    .status-bar {
      display: flex;
      gap: 1rem;
      flex-wrap: wrap;
    }

    .status-indicator {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.5rem 1rem;
      background: var(--bg-tertiary);
      border-radius: 6px;
      font-size: 0.85rem;
    }

    .status-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      animation: pulse 2s ease-in-out infinite;
    }

    .status-dot.connected { background: var(--success); }
    .status-dot.disconnected { background: var(--error); }
    .status-dot.warning { background: var(--warning); }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }

    .container {
      max-width: 1600px;
      margin: 0 auto;
      padding: 1.5rem;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
      gap: 1.5rem;
      margin-bottom: 1.5rem;
    }

    .grid-full {
      grid-column: 1 / -1;
    }

    .card {
      background: var(--bg-secondary);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 1.5rem;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
    }

    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1rem;
      padding-bottom: 0.75rem;
      border-bottom: 1px solid var(--border-color);
    }

    .card-title {
      font-size: 1.1rem;
      font-weight: 600;
      color: var(--accent-primary);
    }

    .card-badge {
      padding: 0.25rem 0.75rem;
      background: var(--bg-tertiary);
      border-radius: 12px;
      font-size: 0.75rem;
      font-weight: 600;
    }

    .gauge-container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 1.5rem;
    }

    .gauge {
      text-align: center;
    }

    .gauge-value {
      font-size: 2.5rem;
      font-weight: 700;
      color: var(--accent-primary);
      line-height: 1;
      margin-bottom: 0.5rem;
    }

    .gauge-label {
      font-size: 0.9rem;
      color: var(--text-secondary);
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .gauge-sublabel {
      font-size: 0.75rem;
      color: var(--text-secondary);
      margin-top: 0.25rem;
    }

    .progress-group {
      margin-bottom: 1rem;
    }

    .progress-header {
      display: flex;
      justify-content: space-between;
      margin-bottom: 0.5rem;
      font-size: 0.9rem;
    }

    .progress-bar {
      width: 100%;
      height: 24px;
      background: var(--bg-tertiary);
      border-radius: 12px;
      overflow: hidden;
      position: relative;
    }

    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, var(--accent-primary), var(--accent-secondary));
      transition: width 0.3s ease;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 0.75rem;
      font-weight: 600;
    }

    .progress-fill.warning {
      background: linear-gradient(90deg, #ff8800, #ffaa00);
    }

    .progress-fill.danger {
      background: linear-gradient(90deg, #ff3333, #ff6666);
    }

    .btn-group {
      display: flex;
      gap: 0.75rem;
      flex-wrap: wrap;
    }

    .btn {
      padding: 0.75rem 1.5rem;
      border: none;
      border-radius: 8px;
      font-size: 0.9rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s ease;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .btn-primary {
      background: var(--accent-primary);
      color: var(--bg-primary);
    }

    .btn-primary:hover {
      background: #00b8dd;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(0, 212, 255, 0.4);
    }

    .btn-success {
      background: var(--success);
      color: var(--bg-primary);
    }

    .btn-warning {
      background: var(--warning);
      color: var(--bg-primary);
    }

    .btn-danger {
      background: var(--error);
      color: white;
    }

    .btn-secondary {
      background: var(--bg-tertiary);
      color: var(--text-primary);
      border: 1px solid var(--border-color);
    }

    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }

    .btn-sm {
      padding: 0.4rem 0.8rem;
      font-size: 0.8rem;
    }

    .data-table {
      width: 100%;
      border-collapse: collapse;
    }

    .data-table th,
    .data-table td {
      padding: 0.75rem;
      text-align: left;
      border-bottom: 1px solid var(--border-color);
    }

    .data-table th {
      font-weight: 600;
      color: var(--accent-primary);
      font-size: 0.85rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .data-table td {
      font-size: 0.9rem;
    }

    .data-table tr:last-child td {
      border-bottom: none;
    }

    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
      gap: 1rem;
    }

    .metric {
      text-align: center;
      padding: 1rem;
      background: var(--bg-tertiary);
      border-radius: 8px;
    }

    .metric-value {
      font-size: 1.5rem;
      font-weight: 700;
      color: var(--accent-secondary);
      margin-bottom: 0.25rem;
    }

    .metric-label {
      font-size: 0.75rem;
      color: var(--text-secondary);
      text-transform: uppercase;
    }

    .form-group {
      margin-bottom: 1rem;
    }

    .form-label {
      display: block;
      margin-bottom: 0.5rem;
      font-size: 0.9rem;
      font-weight: 600;
      color: var(--text-secondary);
    }

    .form-input,
    .form-select {
      width: 100%;
      padding: 0.75rem;
      background: var(--bg-tertiary);
      border: 1px solid var(--border-color);
      border-radius: 6px;
      color: var(--text-primary);
      font-size: 0.9rem;
    }

    .form-input:focus,
    .form-select:focus {
      outline: none;
      border-color: var(--accent-primary);
      box-shadow: 0 0 0 3px rgba(0, 212, 255, 0.1);
    }

    .form-row {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 1rem;
    }

    .checkbox-group {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }

    .checkbox {
      width: 18px;
      height: 18px;
      cursor: pointer;
    }

    .log-container {
      max-height: 300px;
      overflow-y: auto;
      background: var(--bg-tertiary);
      border-radius: 8px;
      padding: 1rem;
      font-family: 'Courier New', monospace;
      font-size: 0.8rem;
    }

    .log-entry {
      padding: 0.25rem 0;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    }

    .log-entry:last-child {
      border-bottom: none;
    }

    .file-list {
      max-height: 400px;
      overflow-y: auto;
    }

    .file-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.75rem;
      background: var(--bg-tertiary);
      border-radius: 6px;
      margin-bottom: 0.5rem;
    }

    .file-info {
      display: flex;
      flex-direction: column;
      gap: 0.25rem;
    }

    .file-name {
      font-weight: 600;
    }

    .file-size {
      font-size: 0.8rem;
      color: var(--text-secondary);
    }

    .file-actions {
      display: flex;
      gap: 0.5rem;
    }

    .notification {
      position: fixed;
      top: 1rem;
      right: 1rem;
      padding: 1rem 1.5rem;
      background: var(--bg-secondary);
      border-left: 4px solid var(--accent-primary);
      border-radius: 8px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
      z-index: 1000;
      animation: slideIn 0.3s ease;
      max-width: 400px;
    }

    .notification.success {
      border-left-color: var(--success);
    }

    .notification.error {
      border-left-color: var(--error);
    }

    .notification.warning {
      border-left-color: var(--warning);
    }

    @keyframes slideIn {
      from {
        transform: translateX(400px);
        opacity: 0;
      }
      to {
        transform: translateX(0);
        opacity: 1;
      }
    }

    .tabs {
      display: flex;
      gap: 0.5rem;
      margin-bottom: 1.5rem;
      border-bottom: 2px solid var(--border-color);
    }

    .tab {
      padding: 0.75rem 1.5rem;
      background: transparent;
      border: none;
      color: var(--text-secondary);
      font-size: 0.9rem;
      font-weight: 600;
      cursor: pointer;
      border-bottom: 3px solid transparent;
      transition: all 0.2s ease;
    }

    .tab.active {
      color: var(--accent-primary);
      border-bottom-color: var(--accent-primary);
    }

    .tab-content {
      display: none;
    }

    .tab-content.active {
      display: block;
    }

    /* GPS Quality Badge Colors */
    .gps-excellent { background: var(--gps-excellent) !important; color: var(--bg-primary) !important; }
    .gps-good { background: var(--gps-good) !important; color: var(--bg-primary) !important; }
    .gps-acceptable { background: var(--gps-acceptable) !important; color: var(--bg-primary) !important; }
    .gps-poor { background: var(--gps-poor) !important; color: white !important; }
    .gps-nofix { background: var(--gps-nofix) !important; color: white !important; }

    /* System Health Stats */
    .health-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 1rem;
      margin-top: 1rem;
    }

    .health-stat {
      text-align: center;
      padding: 1rem;
      background: var(--bg-tertiary);
      border-radius: 8px;
      border-left: 3px solid var(--accent-primary);
    }

    .health-stat.good {
      border-left-color: var(--success);
    }

    .health-stat.warning {
      border-left-color: var(--warning);
    }

    .health-stat.error {
      border-left-color: var(--error);
    }

    .health-value {
      font-size: 1.8rem;
      font-weight: 700;
      margin-bottom: 0.25rem;
    }

    .health-label {
      font-size: 0.75rem;
      color: var(--text-secondary);
      text-transform: uppercase;
    }

    @media (max-width: 768px) {
      .header {
        padding: 1rem;
      }

      .container {
        padding: 1rem;
      }

      .grid {
        grid-template-columns: 1fr;
      }

      .gauge-container {
        grid-template-columns: repeat(2, 1fr);
      }

      .btn-group {
        flex-direction: column;
      }

      .btn {
        width: 100%;
      }
    }

    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .card {
      animation: fadeIn 0.3s ease;
    }

    ::-webkit-scrollbar {
      width: 8px;
      height: 8px;
    }

    ::-webkit-scrollbar-track {
      background: var(--bg-tertiary);
    }

    ::-webkit-scrollbar-thumb {
      background: var(--border-color);
      border-radius: 4px;
    }

    ::-webkit-scrollbar-thumb:hover {
      background: var(--accent-primary);
    }
  </style>
</head>
<body>
  <div class="header">
    <div class="header-title">
      🏎️ IMPR3ZA TELEMETRY
      <span class="fw-version" id="fwVersion">v0.19.2</span>
    </div>
    <div class="status-bar">
      <div class="status-indicator">
        <div class="status-dot" id="sensorStatus"></div>
        <span>GPS/IMU</span>
      </div>
      <div class="status-indicator">
        <div class="status-dot" id="audioStatus"></div>
        <span>Audio</span>
      </div>
      <div class="status-indicator">
        <div class="status-dot" id="loraStatus"></div>
        <span>LoRa</span>
      </div>
      <div class="status-indicator">
        <div class="status-dot" id="rcStatus"></div>
        <span>RC Link</span>
      </div>
    </div>
  </div>

  <div class="container">
    <div class="grid">
      <div class="card">
        <div class="card-header">
          <h2 class="card-title">Speed & G-Force</h2>
          <span class="card-badge" id="modeDisplay">AUTO</span>
        </div>
        <div class="gauge-container">
          <div class="gauge">
            <div class="gauge-value" id="speedValue">0.0</div>
            <div class="gauge-label">KM/H</div>
            <div class="gauge-sublabel">Max: <span id="maxSpeed">0.0</span></div>
          </div>
          <div class="gauge">
            <div class="gauge-value" id="gForceValue">0.00</div>
            <div class="gauge-label">G-FORCE</div>
            <div class="gauge-sublabel">Max: <span id="maxG">0.00</span></div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <h2 class="card-title">RC Inputs</h2>
        </div>
        <div class="progress-group">
          <div class="progress-header">
            <span>Throttle</span>
            <span id="throttlePercent">0%</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="throttleBar" style="width: 50%"></div>
          </div>
        </div>
        <div class="progress-group">
          <div class="progress-header">
            <span>Steering</span>
            <span id="steeringAngle">0.0°</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="steeringBar" style="width: 50%"></div>
          </div>
        </div>
        <div style="margin-top: 0.5rem; font-size: 0.8rem; color: var(--text-secondary);">
          PWM: <span id="throttlePwm">1500</span> / <span id="steeringPwm">1500</span>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <h2 class="card-title">Battery Status</h2>
          <span class="card-badge" id="calModeBadge" style="display: none;">CAL</span>
        </div>
        <div class="gauge-container">
          <div class="gauge">
            <div class="gauge-value" id="battVoltage">12.6</div>
            <div class="gauge-label">VOLTS</div>
          </div>
          <div class="gauge">
            <div class="gauge-value" id="battPercent">100</div>
            <div class="gauge-label">PERCENT</div>
          </div>
        </div>
        <div class="progress-group" style="margin-top: 1rem;">
          <div class="progress-header">
            <span>Current Draw</span>
            <span id="battAmps">0.0 A</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="currentBar" style="width: 0%"></div>
          </div>
        </div>
        <div class="progress-group">
          <div class="progress-header">
            <span>Consumed</span>
            <span id="battMah">0 mAh</span>
          </div>
        </div>
      </div>
    </div>

    <div class="grid">
      <div class="card">
        <div class="card-header">
          <h2 class="card-title">GPS Validation</h2>
          <span class="card-badge gps-nofix" id="gpsQualityBadge">NO FIX</span>
        </div>
        <table class="data-table">
          <tr>
            <th>Quality</th>
            <td id="gpsQualityText">NO_FIX</td>
          </tr>
          <tr>
            <th>Satellites</th>
            <td id="gpsSats">0</td>
          </tr>
          <tr>
            <th>HDOP</th>
            <td id="gpsHdop">--</td>
          </tr>
          <tr>
            <th>Claims Valid</th>
            <td id="gpsClaimsValid" style="font-weight: 700;">❌</td>
          </tr>
          <tr>
            <th>Excellent %</th>
            <td id="gpsExcellentPct">0%</td>
          </tr>
          <tr>
            <th>Temperature</th>
            <td id="tempValue">--°C</td>
          </tr>
        </table>
      </div>

      <div class="card">
        <div class="card-header">
          <h2 class="card-title">Off-Road Metrics</h2>
        </div>
        <div class="metrics-grid">
          <div class="metric">
            <div class="metric-value" id="rollValue">0.0°</div>
            <div class="metric-label">Roll</div>
          </div>
          <div class="metric">
            <div class="metric-value" id="pitchValue">0.0°</div>
            <div class="metric-label">Pitch</div>
          </div>
          <div class="metric">
            <div class="metric-value" id="slipValue">0.0°</div>
            <div class="metric-label">Slip</div>
          </div>
          <div class="metric">
            <div class="metric-value" id="driftStatus">NO</div>
            <div class="metric-label">Drift</div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <h2 class="card-title">Storage</h2>
        </div>
        <div class="progress-group">
          <div class="progress-header">
            <span>Flash Usage</span>
            <span id="storagePercent">0%</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="storageBar" style="width: 0%"></div>
          </div>
        </div>
        <table class="data-table" style="margin-top: 1rem;">
          <tr>
            <th>Log Files</th>
            <td id="fileCount">0</td>
          </tr>
          <tr>
            <th>Format</th>
            <td id="logFormat">Binary</td>
          </tr>
          <tr>
            <th>Current Size</th>
            <td id="currentSize">0 KB</td>
          </tr>
          <tr>
            <th>Remaining</th>
            <td id="remainingTime">-- min</td>
          </tr>
        </table>
      </div>
    </div>

    <div class="card grid-full">
      <div class="card-header">
        <h2 class="card-title">System Health</h2>
      </div>
      <div class="health-grid">
        <div class="health-stat" id="i2cHealthStat">
          <div class="health-value" id="i2cSuccessRate">--</div>
          <div class="health-label">I2C Success</div>
        </div>
        <div class="health-stat" id="valHealthStat">
          <div class="health-value" id="validationRate">--</div>
          <div class="health-label">Validation</div>
        </div>
        <div class="health-stat">
          <div class="health-value" id="i2cAttempts">0</div>
          <div class="health-label">I2C Attempts</div>
        </div>
        <div class="health-stat">
          <div class="health-value" id="crcErrors">0</div>
          <div class="health-label">CRC Errors</div>
        </div>
        <div class="health-stat">
          <div class="health-value" id="violations">0</div>
          <div class="health-label">Violations</div>
        </div>
      </div>
    </div>

    <div class="card grid-full">
      <div class="card-header">
        <h2 class="card-title">Control Panel</h2>
      </div>
      <div class="btn-group">
        <button class="btn btn-primary" id="toggleLogBtn">Start Recording</button>
        <button class="btn btn-success" id="downloadLatestBtn">Download Latest</button>
        <button class="btn btn-warning" id="steeringCalBtn">Calibrate Steering</button>
        <button class="btn btn-warning" id="zeroImuBtn">Zero IMU</button>
        <button class="btn btn-secondary" id="resetBatteryBtn">Reset Battery</button>
        <button class="btn btn-danger" id="deleteAllBtn">Delete All Logs</button>
      </div>
    </div>

    <div class="card grid-full">
      <div class="tabs">
        <button class="tab active" onclick="switchTab('files')">Files</button>
        <button class="tab" onclick="switchTab('settings')">Settings</button>
        <button class="tab" onclick="switchTab('battery')">Battery Config</button>
        <button class="tab" onclick="switchTab('calibration')">Calibration</button>
        <button class="tab" onclick="switchTab('logs')">System Logs</button>
      </div>

      <div id="tab-files" class="tab-content active">
        <div class="file-list" id="fileList">
          <p style="text-align: center; color: var(--text-secondary);">Loading files...</p>
        </div>
      </div>

      <div id="tab-settings" class="tab-content">
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Driving Mode</label>
            <select class="form-select" id="modeSelect">
              <option value="0">On-Road</option>
              <option value="1">Off-Road</option>
              <option value="2">Auto</option>
            </select>
          </div>
          <div class="form-group">
            <label class="form-label">Master Volume</label>
            <input type="range" class="form-input" id="masterVolume" min="0" max="100" value="80">
            <div style="text-align: center; margin-top: 0.5rem; color: var(--text-secondary);">
              <span id="masterVolumeValue">80</span>%
            </div>
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Engine Volume</label>
            <input type="range" class="form-input" id="engineVolume" min="0" max="100" value="80">
            <div style="text-align: center; margin-top: 0.5rem; color: var(--text-secondary);">
              <span id="engineVolumeValue">80</span>%
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Turbo Volume</label>
            <input type="range" class="form-input" id="turboVolume" min="0" max="100" value="50">
            <div style="text-align: center; margin-top: 0.5rem; color: var(--text-secondary);">
              <span id="turboVolumeValue">50</span>%
            </div>
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Ambient Temperature (°C)</label>
            <input type="number" class="form-input" id="ambientTemp" step="0.1" value="25.0">
          </div>
        </div>
        <div class="btn-group" style="margin-top: 1rem;">
          <button class="btn btn-primary" onclick="saveAudioSettings()">Save Audio</button>
          <button class="btn btn-primary" onclick="saveTemperature()">Set Temperature</button>
          <button class="btn btn-secondary" onclick="saveDrivingMode()">Save Mode</button>
        </div>
      </div>

      <div id="tab-battery" class="tab-content">
        <div class="card-header" style="margin-bottom: 1rem; padding-bottom: 0.5rem;">
          <h3 style="font-size: 1rem; color: var(--accent-secondary);">Battery Configuration</h3>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Battery Capacity (mAh)</label>
            <input type="number" class="form-input" id="battCapacity" value="5000">
          </div>
           <div class="form-group">
            <label class="form-label">Cell Count (S)</label>
            <select class="form-select" id="battCells">
              <option value="1">1S (3.7V)</option>
              <option value="2">2S (7.4V)</option>
              <option value="3">3S (11.1V)</option>
              <option value="4">4S (14.8V)</option>
              <option value="6">6S (22.2V)</option>
            </select>
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Static Load (A)</label>
            <input type="number" class="form-input" id="battStatic" step="0.1" value="0.4">
          </div>
          <div class="form-group">
            <label class="form-label">Calibration Factor</label>
            <input type="number" class="form-input" id="battFactor" step="0.01" value="1.0">
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Motor Type</label>
            <select class="form-select" id="motorType">
              <option value="0">Brushless</option>
              <option value="1">Brushed</option>
            </select>
          </div>
           <div class="form-group">
            <label class="form-label">Motor Peak Amps</label>
            <input type="number" class="form-input" id="motorAmps" step="0.1" value="60.0">
          </div>
        </div>
        <div class="form-row">
           <div class="form-group">
            <label class="form-label">Motor Turns (T) / KV</label>
            <input type="number" class="form-input" id="motorTurns" step="any" value="13.5">
          </div>
        </div>
        <div class="btn-group" style="margin-top: 1rem;">
          <button class="btn btn-primary" onclick="saveBatteryConfig()">Save Config</button>
        </div>
      </div>

      <div id="tab-calibration" class="tab-content">
        <div class="card-header" style="margin-bottom: 1rem; padding-bottom: 0.5rem;">
          <h3 style="font-size: 1rem; color: var(--accent-secondary);">Battery Calibration</h3>
          <span class="card-badge" id="calStatusBadge">DISABLED</span>
        </div>
        <div style="padding: 1rem; background: var(--bg-tertiary); border-radius: 8px; margin-bottom: 1rem;">
          <p style="margin-bottom: 0.5rem; color: var(--text-secondary);">
            <strong>Instructions:</strong>
          </p>
          <ol style="margin-left: 1.5rem; color: var(--text-secondary); line-height: 1.8;">
            <li>Fully charge battery</li>
            <li>Click "Start Calibration"</li>
            <li>Run vehicle until low battery (~20%)</li>
            <li>Recharge battery completely</li>
            <li>Enter "Charged Capacity" from charger</li>
            <li>Click "Finish Calibration"</li>
            <li>Apply suggested factor if needed</li>
          </ol>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Predicted mAh</label>
            <input type="text" class="form-input" id="calPredicted" readonly value="0">
          </div>
          <div class="form-group">
            <label class="form-label">Charger Reading (mAh)</label>
            <input type="number" class="form-input" id="calActual" value="0">
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Suggested Factor</label>
            <input type="text" class="form-input" id="calSuggestedFactor" readonly value="--">
          </div>
        </div>
        <div class="btn-group" style="margin-top: 1rem;">
          <button class="btn btn-success" id="startCalBtn" onclick="startCalibration()">Start Calibration</button>
          <button class="btn btn-primary" id="finishCalBtn" onclick="finishCalibration()" disabled>Finish Calibration</button>
          <button class="btn btn-warning" onclick="applyCalibrationFactor()">Apply Factor</button>
        </div>
      </div>

      <div id="tab-logs" class="tab-content">
        <div class="log-container" id="logContainer">
          <p style="text-align: center; color: var(--text-secondary);">Loading logs...</p>
        </div>
        <div class="btn-group" style="margin-top: 1rem;">
          <button class="btn btn-secondary" onclick="sendCommand('getLogs')">Refresh Logs</button>
        </div>
      </div>
    </div>
  </div>

  <script>
    // ========== WEBSOCKET CONNECTION ==========
    let ws;
    let reconnectInterval;
    let lastData = {};

    function connectWebSocket() {
      ws = new WebSocket('ws://' + window.location.hostname + '/ws');

      ws.onopen = () => {
        console.log('WebSocket connected');
        clearInterval(reconnectInterval);
        showNotification('success', 'Connected to telemetry system');
        sendCommand('getSettings');
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'notification') {
            showNotification(data.status, data.message);
          } else if (data.type === 'cal_step') {
            showNotification('info', data.message);
          } else {
            updateDashboard(data);
            lastData = data;
          }
        } catch (e) {
          console.error('Parse error:', e);
        }
      };

      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
      };

      ws.onclose = () => {
        console.log('WebSocket closed, reconnecting...');
        reconnectInterval = setInterval(() => {
          connectWebSocket();
        }, 3000);
      };
    }

    // ========== HELPER FOR INPUTS ==========
    // Fixes the issue where typing is overwritten by incoming data
    function updateInputSafe(id, value, precision = null) {
      const element = document.getElementById(id);
      if (element && document.activeElement !== element) {
        if (precision !== null && typeof value === 'number') {
          element.value = value.toFixed(precision);
        } else {
          element.value = value;
        }
      }
    }

    // ========== DASHBOARD UPDATE ==========
    function updateDashboard(data) {
      // Firmware version
      if (data.fw) {
        document.getElementById('fwVersion').textContent = data.fw;
      }

      // Speed & G-Force
      document.getElementById('speedValue').textContent = (data.speed || 0).toFixed(1);
      document.getElementById('maxSpeed').textContent = (data.tel?.max_speed || 0).toFixed(1);
      document.getElementById('gForceValue').textContent = (data.tel?.g_lat || 0).toFixed(2);
      document.getElementById('maxG').textContent = (data.tel?.max_g || 0).toFixed(2);

      // Mode
      const modes = ['ON-ROAD', 'OFF-ROAD', 'AUTO'];
      document.getElementById('modeDisplay').textContent = modes[data.mode] || 'AUTO';
      if (data.mode !== undefined) {
        // Safe update for dropdowns too
        const modeSelect = document.getElementById('modeSelect');
        if (document.activeElement !== modeSelect) {
          modeSelect.value = data.mode;
        }
      }

      // RC Inputs
      const throttle = data.rc_thr || 0;
      const steering = data.rc_steer || 0;
      
      document.getElementById('throttlePercent').textContent = throttle + '%';
      const throttleWidth = ((throttle + 100) / 200) * 100;
      const throttleBar = document.getElementById('throttleBar');
      throttleBar.style.width = throttleWidth + '%';
      throttleBar.className = 'progress-fill' + (Math.abs(throttle) > 80 ? ' warning' : '');

      document.getElementById('steeringAngle').textContent = steering.toFixed(1) + '°';
      const steeringWidth = ((steering + 45) / 90) * 100;
      document.getElementById('steeringBar').style.width = Math.max(0, Math.min(100, steeringWidth)) + '%';

      // PWM values
      document.getElementById('throttlePwm').textContent = data.throttlePwm || 1500;
      document.getElementById('steeringPwm').textContent = data.steeringPwm || 1500;

      // Battery
      if (data.batt) {
        document.getElementById('battVoltage').textContent = (data.batt.volts || 12.6).toFixed(1);
        document.getElementById('battPercent').textContent = data.batt.pct || 100;
        document.getElementById('battAmps').textContent = (data.batt.amps || 0).toFixed(1) + ' A';
        document.getElementById('battMah').textContent = Math.round(data.batt.mah_used || 0) + ' mAh';

        const currentPercent = Math.min(100, (data.batt.amps / 60) * 100);
        const currentBar = document.getElementById('currentBar');
        currentBar.style.width = currentPercent + '%';
        currentBar.className = 'progress-fill' + (currentPercent > 80 ? ' danger' : currentPercent > 60 ? ' warning' : '');

        // Battery calibration status
        if (data.batt.cal_mode !== undefined) {
          const calBadge = document.getElementById('calModeBadge');
          const calStatusBadge = document.getElementById('calStatusBadge');
          const startBtn = document.getElementById('startCalBtn');
          const finishBtn = document.getElementById('finishCalBtn');
          
          if (data.batt.cal_mode === 1) { // CAL_RECORDING
            calBadge.style.display = 'block';
            calBadge.textContent = 'RECORDING';
            calBadge.style.background = 'var(--warning)';
            calStatusBadge.textContent = 'RECORDING';
            calStatusBadge.style.background = 'var(--warning)';
            startBtn.disabled = true;
            finishBtn.disabled = false;
            updateInputSafe('calPredicted', Math.round(data.batt.cal_predicted || 0));
          } else if (data.batt.cal_mode === 2) { // CAL_COMPLETE
            calBadge.style.display = 'none';
            calStatusBadge.textContent = 'COMPLETE';
            calStatusBadge.style.background = 'var(--success)';
            startBtn.disabled = false;
            finishBtn.disabled = true;
            updateInputSafe('calSuggestedFactor', (data.batt.cal_factor || 1.0), 3);
          } else {
            calBadge.style.display = 'none';
            calStatusBadge.textContent = 'DISABLED';
            calStatusBadge.style.background = 'var(--bg-tertiary)';
            startBtn.disabled = false;
            finishBtn.disabled = true;
          }
        }

        // Update battery config fields using SAFE UPDATE
        if (data.batt.cap) updateInputSafe('battCapacity', data.batt.cap);
        
        // NEW: Update Cell Count from WebSocket
        if (data.batt.cells) {
           const cellSelect = document.getElementById('battCells');
           if(document.activeElement !== cellSelect) cellSelect.value = data.batt.cells;
        }
        
        if (data.batt.static !== undefined) updateInputSafe('battStatic', data.batt.static);
        if (data.batt.factor !== undefined) updateInputSafe('battFactor', data.batt.factor);
        if (data.batt.m_type !== undefined) {
            const mType = document.getElementById('motorType');
            if(document.activeElement !== mType) mType.value = data.batt.m_type;
        }
        if (data.batt.m_amps) updateInputSafe('motorAmps', data.batt.m_amps);
        if (data.batt.m_turns) updateInputSafe('motorTurns', data.batt.m_turns);
      }

      // ========== GPS QUALITY VALIDATION ==========
      if (data.tel) {
        const qualityStr = data.tel.gps_quality_str || 'NO_FIX';
        const qualityClasses = {
          'EXCELLENT': 'gps-excellent',
          'GOOD': 'gps-good',
          'ACCEPTABLE': 'gps-acceptable',
          'POOR': 'gps-poor',
          'NO_FIX': 'gps-nofix'
        };
        
        const qualityBadge = document.getElementById('gpsQualityBadge');
        qualityBadge.textContent = qualityStr;
        qualityBadge.className = 'card-badge ' + qualityClasses[qualityStr];
        
        document.getElementById('gpsQualityText').textContent = qualityStr;
        document.getElementById('gpsSats').textContent = data.tel.sats || 0;
        document.getElementById('gpsHdop').textContent = (data.tel.gps_hdop !== undefined) ? 
          data.tel.gps_hdop.toFixed(2) : '--';
        document.getElementById('gpsClaimsValid').textContent = data.tel.gps_valid_claims ? '✅ YES' : '❌ NO';
        document.getElementById('gpsClaimsValid').style.color = data.tel.gps_valid_claims ? 
          'var(--success)' : 'var(--error)';
        document.getElementById('gpsExcellentPct').textContent = 
          (data.tel.gps_excellent_pct !== undefined) ? data.tel.gps_excellent_pct.toFixed(1) + '%' : '0%';
        
        // Temperature with source indicator
        let tempSource = '';
        if (data.tel.temp_manual) {
          tempSource = ' <span style="color: var(--warning);">🔧 Manual</span>';
        } else if (data.tel.temp_valid) {
          tempSource = ' <span style="color: var(--success);">📡 GPS</span>';
        } else {
          tempSource = ' <span style="color: var(--error);">⚠️ Invalid</span>';
        }
        document.getElementById('tempValue').innerHTML = 
          (data.tel.temp || 25).toFixed(1) + '°C' + tempSource;
      }

      // Off-Road Metrics
      if (data.off) {
        document.getElementById('rollValue').textContent = (data.off.roll || 0).toFixed(1) + '°';
        document.getElementById('pitchValue').textContent = (data.off.pitch || 0).toFixed(1) + '°';
        document.getElementById('slipValue').textContent = (data.off.slip_angle || 0).toFixed(1) + '°';
        document.getElementById('driftStatus').textContent = data.off.drift ? 'YES' : 'NO';
      }

      // Storage
      if (data.storage) {
        const used = data.storage.used || 0;
        const total = data.storage.total || 1;
        const percent = ((used / total) * 100).toFixed(1);
        
        document.getElementById('storagePercent').textContent = percent + '%';
        const storageBar = document.getElementById('storageBar');
        storageBar.style.width = percent + '%';
        storageBar.className = 'progress-fill' + (percent > 90 ? ' danger' : percent > 70 ? ' warning' : '');

        document.getElementById('fileCount').textContent = data.storage.files || 0;
        document.getElementById('logFormat').textContent = data.storage.format || 'binary';
        document.getElementById('currentSize').textContent = Math.round((data.storage.current_size || 0) / 1024) + ' KB';
        document.getElementById('remainingTime').textContent = (data.storage.remaining_minutes || 0) + ' min';
      }

      // ========== SYSTEM HEALTH STATISTICS ==========
      if (data.i2c) {
        const successRate = data.i2c.success_rate || 0;
        document.getElementById('i2cSuccessRate').textContent = successRate.toFixed(1) + '%';
        
        const i2cStat = document.getElementById('i2cHealthStat');
        if (successRate >= 95) {
          i2cStat.className = 'health-stat good';
        } else if (successRate >= 80) {
          i2cStat.className = 'health-stat warning';
        } else {
          i2cStat.className = 'health-stat error';
        }
        
        document.getElementById('i2cAttempts').textContent = data.i2c.attempts || 0;
        document.getElementById('crcErrors').textContent = data.i2c.crc_errors || 0;
      }

      if (data.validation) {
        const valRate = data.validation.rate || 0;
        document.getElementById('validationRate').textContent = valRate.toFixed(1) + '%';
        
        const valStat = document.getElementById('valHealthStat');
        if (valRate >= 99) {
          valStat.className = 'health-stat good';
        } else if (valRate >= 95) {
          valStat.className = 'health-stat warning';
        } else {
          valStat.className = 'health-stat error';
        }
        
        document.getElementById('violations').textContent = data.validation.violations || 0;
      }

      // Connection Status
      updateStatus('sensorStatus', data.sensor_conn);
      updateStatus('audioStatus', data.audio_conn);
      updateStatus('loraStatus', data.lora_conn);
      updateStatus('rcStatus', data.rc_thr_conn && data.rc_steer_conn);

      // Logging button
      const logBtn = document.getElementById('toggleLogBtn');
      if (data.log_active) {
        logBtn.textContent = 'Stop Recording';
        logBtn.className = 'btn btn-danger';
      } else {
        logBtn.textContent = 'Start Recording';
        logBtn.className = 'btn btn-primary';
      }

      // Update settings fields SAFE UPDATE
      if (data.settings) {
        if (data.settings.masterVolume) updateInputSafe('masterVolume', data.settings.masterVolume);
        if (data.settings.engineVolume) updateInputSafe('engineVolume', data.settings.engineVolume);
        if (data.settings.turboVolume) updateInputSafe('turboVolume', data.settings.turboVolume);
        
        // Update slider labels
        document.getElementById('masterVolumeValue').textContent = document.getElementById('masterVolume').value;
        document.getElementById('engineVolumeValue').textContent = document.getElementById('engineVolume').value;
        document.getElementById('turboVolumeValue').textContent = document.getElementById('turboVolume').value;
      }
    }

    function updateStatus(elementId, connected) {
      const dot = document.getElementById(elementId);
      dot.className = 'status-dot ' + (connected ? 'connected' : 'disconnected');
    }

    // ========== CONTROL FUNCTIONS ==========
    function sendCommand(command, params = {}) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({command, ...params}));
      }
    }

    document.getElementById('toggleLogBtn').addEventListener('click', () => {
      sendCommand('toggleLog');
    });

    document.getElementById('downloadLatestBtn').addEventListener('click', () => {
      window.open('/api/latestlog', '_blank');
    });

    document.getElementById('steeringCalBtn').addEventListener('click', () => {
      sendCommand('startSteeringCal', {
        minAngle: -30,
        centerAngle: 0,
        maxAngle: 30,
        reversed: false
      });
    });

    document.getElementById('zeroImuBtn').addEventListener('click', () => {
      if (confirm('Ensure vehicle is LEVEL and STATIONARY. Zero IMU?')) {
        sendCommand('zeroIMU');
      }
    });

    document.getElementById('resetBatteryBtn').addEventListener('click', () => {
      if (confirm('Reset battery fuel gauge?')) {
        sendCommand('resetBattery');
      }
    });

    document.getElementById('deleteAllBtn').addEventListener('click', () => {
      if (confirm('Delete ALL log files? This cannot be undone!')) {
        sendCommand('deleteAllLogs');
      }
    });

    // Volume sliders real-time update
    document.getElementById('masterVolume').addEventListener('input', (e) => {
      document.getElementById('masterVolumeValue').textContent = e.target.value;
    });
    document.getElementById('engineVolume').addEventListener('input', (e) => {
      document.getElementById('engineVolumeValue').textContent = e.target.value;
    });
    document.getElementById('turboVolume').addEventListener('input', (e) => {
      document.getElementById('turboVolumeValue').textContent = e.target.value;
    });

    // ========== FILE MANAGEMENT ==========
    async function loadFiles() {
      try {
        const response = await fetch('/api/listfiles');
        const files = await response.json();
        
        const fileList = document.getElementById('fileList');
        if (files.length === 0) {
          fileList.innerHTML = '<p style="text-align: center; color: var(--text-secondary);">No log files found</p>';
          return;
        }

        fileList.innerHTML = files.map(file => `
          <div class="file-item">
            <div class="file-info">
              <div class="file-name">${file.name}</div>
              <div class="file-size">${(file.size / 1024).toFixed(1)} KB</div>
            </div>
            <div class="file-actions">
              <button class="btn btn-primary btn-sm" onclick="downloadFile('${file.name}')">Download</button>
              <button class="btn btn-danger btn-sm" onclick="deleteFile('${file.name}')">Delete</button>
            </div>
          </div>
        `).join('');
      } catch (e) {
        console.error('Error loading files:', e);
      }
    }

    function downloadFile(filename) {
      window.open('/api/download?file=' + encodeURIComponent(filename), '_blank');
    }

    async function deleteFile(filename) {
      if (confirm('Delete ' + filename + '?')) {
        try {
          await fetch('/api/delete', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'file=' + encodeURIComponent(filename)
          });
          loadFiles();
        } catch (e) {
          console.error('Error deleting file:', e);
        }
      }
    }

    // ========== SETTINGS ==========
    function saveAudioSettings() {
      sendCommand('setAudio', {
        master: parseInt(document.getElementById('masterVolume').value),
        engine: parseInt(document.getElementById('engineVolume').value),
        turbo: parseInt(document.getElementById('turboVolume').value),
        mech: 30,
        mute: false,
        lpf: 0,
        hpf: 0,
        dist: 0
      });
      showNotification('success', 'Audio settings saved');
    }

    function saveTemperature() {
      const temp = parseFloat(document.getElementById('ambientTemp').value);
      if (temp >= -20 && temp <= 60) {
        sendCommand('setAmbientTemp', {temp: temp});
        showNotification('success', 'Temperature set to ' + temp.toFixed(1) + '°C');
      } else {
        showNotification('error', 'Temperature must be between -20°C and 60°C');
      }
    }

    function saveDrivingMode() {
      sendCommand('switchMode', {
        mode: parseInt(document.getElementById('modeSelect').value)
      });
      showNotification('success', 'Driving mode saved');
    }

    function saveBatteryConfig() {
      sendCommand('setBatteryConfig', {
        capacity: parseInt(document.getElementById('battCapacity').value),
        cells: parseInt(document.getElementById('battCells').value), // NEW: Send Cell Count
        static: parseFloat(document.getElementById('battStatic').value),
        factor: parseFloat(document.getElementById('battFactor').value)
      });
      
      sendCommand('setMotorConfig', {
        type: parseInt(document.getElementById('motorType').value),
        amps: parseFloat(document.getElementById('motorAmps').value),
        turns: parseFloat(document.getElementById('motorTurns').value),
        esc: 60
      });
      
      showNotification('success', 'Battery configuration saved');
    }

    // ========== BATTERY CALIBRATION ==========
    function startCalibration() {
      if (confirm('Start battery calibration? Run vehicle until low battery, then recharge and enter charger reading.')) {
        sendCommand('startCalibration');
        showNotification('info', 'Calibration started - run vehicle now');
      }
    }

    function finishCalibration() {
      const chargerMah = parseFloat(document.getElementById('calActual').value);
      if (chargerMah < 100) {
        showNotification('error', 'Enter valid charger reading (>100 mAh)');
        return;
      }
      
      sendCommand('finishCalibration', {charger_mah: chargerMah});
      showNotification('success', 'Calibration complete - check suggested factor');
    }

    function applyCalibrationFactor() {
      const factor = parseFloat(document.getElementById('calSuggestedFactor').value);
      if (isNaN(factor) || factor <= 0) {
        showNotification('error', 'No valid calibration factor to apply');
        return;
      }
      
      document.getElementById('battFactor').value = factor.toFixed(3);
      saveBatteryConfig();
      showNotification('success', 'Calibration factor applied: ' + factor.toFixed(3));
    }

    // ========== TABS ==========
    function switchTab(tabName) {
      document.querySelectorAll('.tab').forEach(tab => {
        tab.classList.remove('active');
      });
      document.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
      });

      event.target.classList.add('active');
      document.getElementById('tab-' + tabName).classList.add('active');

      if (tabName === 'files') {
        loadFiles();
      } else if (tabName === 'logs') {
        sendCommand('getLogs');
      }
    }

    // ========== NOTIFICATIONS ==========
    function showNotification(status, message) {
      const notification = document.createElement('div');
      notification.className = 'notification ' + status;
      notification.textContent = message;
      document.body.appendChild(notification);

      setTimeout(() => {
        notification.style.animation = 'slideIn 0.3s ease reverse';
        setTimeout(() => notification.remove(), 300);
      }, 3000);
    }

    // ========== INITIALIZATION ==========
    window.addEventListener('load', () => {
      connectWebSocket();
      loadFiles();
      
      // Request initial settings
      setTimeout(() => {
        sendCommand('getSettings');
      }, 1000);
    });
  </script>
</body>
</html>
)rawliteral";

#endif