#pragma once

#include <Arduino.h>

// --- WiFi Setup Page (AP mode) ---
const char WIFI_SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%WEB_WIFI_SETUP_TITLE%</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #1a1a2e; color: #eee;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px;
    }
    .card {
      background: #16213e; border-radius: 16px;
      padding: 32px; max-width: 400px; width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h1 { font-size: 1.5rem; margin-bottom: 8px; color: #e94560; }
    p.sub { font-size: 0.85rem; color: #888; margin-bottom: 24px; }
    label { display: block; font-size: 0.85rem; color: #aaa; margin-bottom: 4px; }
    input[type=text], input[type=password], select {
      width: 100%; padding: 12px; border: 1px solid #333;
      border-radius: 8px; background: #0f3460; color: #fff;
      font-size: 1rem; margin-bottom: 16px; outline: none;
    }
    input[type=file] {
      width: 100%; padding: 10px; border: 1px dashed #e94560;
      border-radius: 8px; background: #0f3460; color: #aaa;
      font-size: 0.85rem; margin-bottom: 8px; cursor: pointer;
    }
    input:focus, select:focus { border-color: #e94560; }
    button, .btn {
      width: 100%; padding: 14px; border: none; border-radius: 8px;
      background: #e94560; color: #fff; font-size: 1rem;
      font-weight: 600; cursor: pointer; transition: background 0.2s;
      text-align: center; text-decoration: none; display: block;
    }
    button:hover, .btn:hover { background: #c73652; }
    .btn-wps { background: #4ecca3; margin-top: 8px; }
    .btn-wps:hover { background: #3ba88c; }
    .divider {
      text-align: center; color: #555; margin: 20px 0;
      font-size: 0.85rem; position: relative;
    }
    .divider::before, .divider::after {
      content: ''; position: absolute; top: 50%;
      width: 35%; height: 1px; background: #333;
    }
    .divider::before { left: 0; }
    .divider::after { right: 0; }
    .wps-info {
      text-align: center; font-size: 0.85rem; color: #aaa;
      margin-bottom: 12px; line-height: 1.5;
    }
    .id { text-align: center; font-size: 0.75rem; color: #555; margin-top: 16px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>%WEB_WIFI_SETUP_HEADING%</h1>
    <p class="sub">%WEB_WIFI_SETUP_SUBTITLE%</p>
    <form method="POST" action="/save-wifi">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px;">
        <label for="ssid-select" style="margin:0;">%WEB_AVAILABLE_NETWORKS%</label>
        <a href="#" onclick="doScan();return false;" style="color:#e94560;font-size:0.8rem;text-decoration:none;">%WEB_RESCAN%</a>
      </div>
      <select id="ssid-select" onchange="if(this.value){document.getElementById('ssid').value=this.value;}">
        <option value="">%WEB_SCANNING_NETWORKS%</option>
      </select>
      <label for="ssid">%WEB_WIFI_NAME%</label>
      <input type="text" id="ssid" name="ssid" required autocomplete="off" placeholder="%WEB_NETWORK_NAME%">
      <label for="pass">%WEB_PASSWORD%</label>
      <input type="password" id="pass" name="pass" autocomplete="off" placeholder="%WEB_WIFI_PASSWORD%">
      <button type="submit">%WEB_SAVE_AND_CONNECT%</button>
    </form>
    <div class="divider">%WEB_OR%</div>
    <p class="wps-info">%WEB_WPS_HINT%</p>
    <a class="btn btn-wps" href="/wps">%WEB_WPS_START%</a>

    <div class="id">%DEVICE_NAME%</div>
  </div>
  <script>
    const TEXT = {
      scanningNetworks: '%WEB_SCANNING_NETWORKS%',
      noNetworks: '%WEB_NO_NETWORKS%',
      selectNetworkPrefix: '%WEB_SELECT_NETWORK_PREFIX%',
      selectNetworkSuffix: '%WEB_SELECT_NETWORK_SUFFIX%',
      manualEntry: '%WEB_MANUAL_ENTRY%',
      signalUnit: '%WEB_SIGNAL_UNIT%'
    };

    function doScan() {
      const sel = document.getElementById('ssid-select');
      sel.innerHTML = '<option value="">' + TEXT.scanningNetworks + '</option>';
      fetch('/scan-wifi')
        .then(r => r.json())
        .then(nets => {
          if (!nets || nets.length === 0) {
            sel.innerHTML = '<option value="">' + TEXT.noNetworks + '</option>';
            return;
          }
          sel.innerHTML = '<option value="">' + TEXT.selectNetworkPrefix + nets.length + TEXT.selectNetworkSuffix + '</option>';
          nets.forEach(n => {
            if (n.ssid) {
              const opt = document.createElement('option');
              opt.value = n.ssid;
              opt.textContent = n.ssid + ' (' + n.rssi + ' ' + TEXT.signalUnit + ')';
              sel.appendChild(opt);
            }
          });
        })
        .catch(() => {
          sel.innerHTML = '<option value="">' + TEXT.manualEntry + '</option>';
        });
    }
    doScan();

  </script>
</body>
</html>
)rawliteral";



// --- WPS Status Page ---
const char WPS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%WEB_WPS_PAGE_TITLE%</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #1a1a2e; color: #eee;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px; text-align: center;
    }
    .card {
      background: #16213e; border-radius: 16px;
      padding: 32px; max-width: 400px; width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h1 { color: #4ecca3; margin-bottom: 12px; }
    p { color: #aaa; line-height: 1.6; }
    .spinner {
      margin: 20px auto; width: 40px; height: 40px;
      border: 4px solid #333; border-top: 4px solid #4ecca3;
      border-radius: 50%; animation: spin 1s linear infinite;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div class="card">
    <h1>%WEB_WPS_ACTIVE%</h1>
    <div class="spinner"></div>
    <p>%WEB_WPS_WAITING%</p>
  </div>
</body>
</html>
)rawliteral";

// --- Success Page (shared) ---
const char SUCCESS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%WEB_SUCCESS_PAGE_TITLE%</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #1a1a2e; color: #eee;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px; text-align: center;
    }
    .card {
      background: #16213e; border-radius: 16px;
      padding: 32px; max-width: 400px; width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h1 { color: #4ecca3; margin-bottom: 12px; }
    p { color: #aaa; }
  </style>
</head>
<body>
  <div class="card">
    <h1>%WEB_SUCCESS_HEADING%</h1>
    <p>%MESSAGE%</p>
  </div>
</body>
</html>
)rawliteral";

// --- Device Settings Page (STA mode) ---
const char SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%WEB_SETTINGS_TITLE%</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #1a1a2e; color: #eee;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px;
    }
    .card {
      background: #16213e; border-radius: 16px;
      padding: 32px; max-width: 420px; width: 100%;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }
    h1 { font-size: 1.5rem; margin-bottom: 8px; color: #4ecca3; }
    p.sub { font-size: 0.85rem; color: #888; margin-bottom: 24px; }
    label { display: block; font-size: 0.85rem; color: #aaa; margin-bottom: 4px; }
    input[type=text] {
      width: 100%; padding: 12px; border: 1px solid #333;
      border-radius: 8px; background: #0f3460; color: #fff;
      font-size: 1rem; margin-bottom: 16px; outline: none;
    }
    input[type=file] {
      width: 100%; padding: 10px; border: 1px dashed #4ecca3;
      border-radius: 8px; background: #0f3460; color: #aaa;
      font-size: 0.85rem; margin-bottom: 8px; cursor: pointer;
    }
    input:focus { border-color: #4ecca3; }
    button {
      width: 100%; padding: 14px; border: none; border-radius: 8px;
      background: #4ecca3; color: #1a1a2e; font-size: 1rem;
      font-weight: 600; cursor: pointer; transition: background 0.2s;
    }
    button:hover { background: #3ba88c; }
    .divider {
      text-align: center; color: #555; margin: 20px 0;
      font-size: 0.85rem; position: relative;
    }
    .divider::before, .divider::after {
      content: ''; position: absolute; top: 50%;
      width: 35%; height: 1px; background: #333;
    }
    .divider::before { left: 0; }
    .divider::after { right: 0; }
    .info {
      margin-top: 20px; padding: 12px; border-radius: 8px;
      background: rgba(78,204,163,0.1); font-size: 0.85rem; color: #aaa;
    }
    .info b { color: #eee; }
    .danger {
      margin-top: 16px; text-align: center;
    }
    .danger a {
      font-size: 0.8rem; color: #e94560; text-decoration: none;
    }
    .danger a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="card">
    <h1>%WEB_SETTINGS_HEADING%</h1>
    <p class="sub">%WEB_SETTINGS_SUBTITLE%</p>
    <form method="POST" action="/save-settings">
      <label for="name">%WEB_DEVICE_NAME%</label>
      <input type="text" id="name" name="name" value="%DEVICE_NAME%" autocomplete="off">
      <label for="serverIp">%WEB_SERVER_IP%</label>
      <input type="text" id="serverIp" name="serverIp" value="%SERVER_IP%" placeholder="%WEB_SERVER_IP_PLACEHOLDER%" autocomplete="off">
      <label for="port">%WEB_TCP_PORT%</label>
      <input type="text" id="port" name="port" value="%TCP_PORT%" autocomplete="off">
      <button type="submit">%WEB_SAVE_SETTINGS%</button>
    </form>
    <div class="info">
      <b>%WEB_SSID%:</b> %WIFI_SSID%<br>
      <b>%WEB_IP%:</b> %DEVICE_IP%<br>
      <b>%WEB_SERVER_IP%:</b> %SERVER_IP%<br>
      <b>%WEB_PORT%:</b> %TCP_PORT%<br>
      <b>%WEB_DEVICE_NAME%:</b> %DEVICE_NAME%
    </div>

    <div class="divider">%WEB_FIRMWARE_UPDATE%</div>
    <form id="upload-form" method="POST" action="/update" enctype="multipart/form-data">
      <label for="fw-file">%WEB_FLASH_FIRMWARE%</label>
      <input type="file" id="fw-file" name="update" accept=".bin" onchange="uploadFirmware(this.files[0])">
      <div id="progress-wrap" style="display:none;margin-top:8px;">
        <div style="background:#0f3460;border-radius:6px;height:14px;overflow:hidden;border:1px solid #333;">
          <div id="progress-bar" style="background:#4ecca3;height:100%;width:0%;transition:width 0.2s;"></div>
        </div>
        <p id="progress-txt" style="font-size:0.8rem;color:#aaa;text-align:center;margin-top:4px;">0%WEB_PERCENT_UNIT%</p>
      </div>
    </form>

    <div class="danger">
      <form method="POST" action="/reset-wifi" onsubmit="return confirm(TEXT.resetWifiConfirm)">
        <button type="submit" style="background:none;border:none;color:#e94560;font-size:0.8rem;cursor:pointer;padding:0">%WEB_RESET_WIFI%</button>
      </form>
    </div>
  </div>
  <script>
    const TEXT = {
      resetWifiConfirm: '%WEB_RESET_WIFI_CONFIRM%',
      installFirmwarePrefix: '%WEB_INSTALL_FIRMWARE_PREFIX%',
      installFirmwareSuffix: '%WEB_INSTALL_FIRMWARE_SUFFIX%',
      uploadRunning: '%WEB_UPLOAD_RUNNING%',
      flashingPrefix: '%WEB_FLASHING_PREFIX%',
      updateSuccess: '%WEB_UPDATE_SUCCESS%',
      errorPrefix: '%WEB_ERROR_PREFIX%',
      connectionInterrupted: '%WEB_CONNECTION_INTERRUPTED%',
      percentUnit: '%WEB_PERCENT_UNIT%'
    };

    function uploadFirmware(file) {
      if (!file) return;
      if (!confirm(TEXT.installFirmwarePrefix + file.name + TEXT.installFirmwareSuffix)) return;
      const fd = new FormData();
      fd.append('update', file);
      const pWrap = document.getElementById('progress-wrap');
      const pBar = document.getElementById('progress-bar');
      const pTxt = document.getElementById('progress-txt');
      pWrap.style.display = 'block';
      pTxt.innerText = TEXT.uploadRunning;

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);
      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          pBar.style.width = pct + '%';
          pTxt.innerText = TEXT.flashingPrefix + pct + TEXT.percentUnit;
        }
      };
      xhr.onload = function() {
        if (xhr.status === 200 && xhr.responseText.trim() === 'OK') {
          pBar.style.width = '100%';
          pBar.style.background = '#4ecca3';
          pTxt.innerText = TEXT.updateSuccess;
          setTimeout(function() { window.location.reload(); }, 5000);
        } else {
          pBar.style.background = '#e94560';
          pTxt.innerText = TEXT.errorPrefix + xhr.responseText;
        }
      };
      xhr.onerror = function() {
        pBar.style.background = '#e94560';
        pTxt.innerText = TEXT.connectionInterrupted;
      };
      xhr.send(fd);
    }
  </script>
</body>
</html>
)rawliteral";
