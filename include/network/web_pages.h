#pragma once

#include <Arduino.h>

// --- WiFi Setup Page (AP mode) ---
const char WIFI_SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>EasySteel WiFi Setup</title>
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
    <h1>&#9881; EasySteel WiFi Setup</h1>
    <p class="sub">WLAN ausw&auml;hlen oder manuell eingeben.</p>
    <form method="POST" action="/save-wifi">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px;">
        <label for="ssid-select" style="margin:0;">Verf&uuml;gbare Netzwerke</label>
        <a href="#" onclick="doScan();return false;" style="color:#e94560;font-size:0.8rem;text-decoration:none;">&#128260; Neu scannen</a>
      </div>
      <select id="ssid-select" onchange="if(this.value){document.getElementById('ssid').value=this.value;}">
        <option value="">Scanne Netzwerke...</option>
      </select>
      <label for="ssid">WiFi Name (SSID)</label>
      <input type="text" id="ssid" name="ssid" required autocomplete="off" placeholder="Netzwerkname">
      <label for="pass">Passwort</label>
      <input type="password" id="pass" name="pass" autocomplete="off" placeholder="WLAN-Passwort">
      <button type="submit">Speichern &amp; Verbinden</button>
    </form>
    <div class="divider">oder</div>
    <p class="wps-info">WPS am Router dr&uuml;cken und hier starten.</p>
    <a class="btn btn-wps" href="/wps">&#128268; WPS Verbindung starten</a>

    <div class="id">%DEVICE_NAME%</div>
  </div>
  <script>
    function doScan() {
      const sel = document.getElementById('ssid-select');
      sel.innerHTML = '<option value="">Scanne Netzwerke...</option>';
      fetch('/scan-wifi')
        .then(r => r.json())
        .then(nets => {
          if (!nets || nets.length === 0) {
            sel.innerHTML = '<option value="">(Keine Netzwerke gefunden - bitte manuell eingeben)</option>';
            return;
          }
          sel.innerHTML = '<option value="">-- Netzwerk ausw&auml;hlen (' + nets.length + ' gefunden) --</option>';
          nets.forEach(n => {
            if (n.ssid) {
              const opt = document.createElement('option');
              opt.value = n.ssid;
              opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)';
              sel.appendChild(opt);
            }
          });
        })
        .catch(() => {
          sel.innerHTML = '<option value="">(Manuelle Eingabe)</option>';
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
  <title>EasySteel - WPS</title>
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
    <h1>&#128268; WPS Active</h1>
    <div class="spinner"></div>
    <p>Waiting for router...<br>This page will disconnect.<br>The device restarts automatically.</p>
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
  <title>EasySteel - Saved</title>
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
    <h1>&#10003; Saved!</h1>
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
  <title>EasySteel Settings</title>
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
    <h1>&#9881; EasySteel Settings</h1>
    <p class="sub">Configure your device.</p>
    <form method="POST" action="/save-settings">
      <label for="name">Device Name</label>
      <input type="text" id="name" name="name" value="%DEVICE_NAME%" autocomplete="off">
      <label for="serverIp">Server IP</label>
      <input type="text" id="serverIp" name="serverIp" value="%SERVER_IP%" placeholder="e.g. 192.168.1.100" autocomplete="off">
      <label for="port">TCP Port</label>
      <input type="text" id="port" name="port" value="%TCP_PORT%" autocomplete="off">
      <button type="submit">Save Settings</button>
    </form>
    <div class="info">
      <b>SSID:</b> %WIFI_SSID%<br>
      <b>IP:</b> %DEVICE_IP%<br>
      <b>Server IP:</b> %SERVER_IP%<br>
      <b>Port:</b> %TCP_PORT%<br>
      <b>Device Name:</b> %DEVICE_NAME%
    </div>

    <div class="divider">Firmware Update</div>
    <form id="upload-form" method="POST" action="/update" enctype="multipart/form-data">
      <label for="fw-file">firmware.bin flashen</label>
      <input type="file" id="fw-file" name="update" accept=".bin" onchange="uploadFirmware(this.files[0])">
      <div id="progress-wrap" style="display:none;margin-top:8px;">
        <div style="background:#0f3460;border-radius:6px;height:14px;overflow:hidden;border:1px solid #333;">
          <div id="progress-bar" style="background:#4ecca3;height:100%;width:0%;transition:width 0.2s;"></div>
        </div>
        <p id="progress-txt" style="font-size:0.8rem;color:#aaa;text-align:center;margin-top:4px;">0%</p>
      </div>
    </form>

    <div class="danger">
      <form method="POST" action="/reset-wifi" onsubmit="return confirm('Reset WiFi? Device will restart in setup mode.')">
        <button type="submit" style="background:none;border:none;color:#e94560;font-size:0.8rem;cursor:pointer;padding:0">Reset WiFi Settings</button>
      </form>
    </div>
  </div>
  <script>
    function uploadFirmware(file) {
      if (!file) return;
      if (!confirm('Firmware "' + file.name + '" jetzt installieren?')) return;
      const fd = new FormData();
      fd.append('update', file);
      const pWrap = document.getElementById('progress-wrap');
      const pBar = document.getElementById('progress-bar');
      const pTxt = document.getElementById('progress-txt');
      pWrap.style.display = 'block';
      pTxt.innerText = 'Upload laeuft...';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);
      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          pBar.style.width = pct + '%';
          pTxt.innerText = 'Flashe: ' + pct + '%';
        }
      };
      xhr.onload = function() {
        if (xhr.status === 200 && xhr.responseText.trim() === 'OK') {
          pBar.style.width = '100%';
          pBar.style.background = '#4ecca3';
          pTxt.innerText = 'Erfolgreich! ESP32 startet neu...';
          setTimeout(function() { window.location.reload(); }, 5000);
        } else {
          pBar.style.background = '#e94560';
          pTxt.innerText = 'Fehler: ' + xhr.responseText;
        }
      };
      xhr.onerror = function() {
        pBar.style.background = '#e94560';
        pTxt.innerText = 'Verbindung unterbrochen';
      };
      xhr.send(fd);
    }
  </script>
</body>
</html>
)rawliteral";
