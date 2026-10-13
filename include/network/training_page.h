#pragma once

#include <Arduino.h>

// --- Mobile Training & Live Dashboard Single-Page App ---
const char TRAINING_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="%LANG%">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>%TRAINING_PAGE_TITLE%</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background: #0d0e15; color: #f0f2f5;
      display: flex; flex-direction: column; align-items: center;
      min-height: 100vh; padding: 12px;
    }
    .container {
      width: 100%; max-width: 480px;
      display: flex; flex-direction: column; gap: 12px;
    }
    /* Top Header */
    .header {
      display: flex; justify-content: space-between; align-items: center;
      background: #181a24; padding: 10px 14px; border-radius: 12px;
      border: 1px solid #282b3a;
    }
    .header .title {
      font-weight: 800; font-size: 1.15rem; color: #4ecca3;
      display: flex; align-items: center; gap: 8px; letter-spacing: 0.5px;
    }
    .header-actions { display: flex; gap: 6px; align-items: center; }
    .btn-icon {
      background: #252838; border: 1px solid #3d425a; color: #aaa;
      border-radius: 8px; padding: 6px 10px; font-size: 0.8rem; cursor: pointer;
      display: flex; align-items: center; gap: 4px; font-weight: 600;
      transition: background 0.15s, border-color 0.15s, color 0.15s;
    }
    .btn-icon.active {
      background: #1b4332 !important; border-color: #2d6a4f !important;
      color: #4ecca3 !important; box-shadow: 0 0 8px rgba(78, 204, 163, 0.3);
    }

    /* Tournament Banner */
    .banner-tourn {
      display: none; background: #5c1d24; border: 1px solid #e94560; color: #ffccd5;
      padding: 10px 14px; border-radius: 10px; font-size: 0.85rem; text-align: center;
      font-weight: 600;
    }
    .remote-card {
      background: #181a24; border: 1px solid #303443; border-radius: 12px;
      padding: 12px; text-align: center; font-size: 0.85rem; color: #cbd5e1;
    }
    .pair-row { display: none; grid-template-columns: 1fr auto; gap: 8px; margin-top: 10px; }
    .pair-row input {
      min-width: 0; border: 1px solid #3f4558; background: #0d0e15; color: #fff;
      border-radius: 8px; padding: 10px; text-align: center; font-size: 1.1rem;
      letter-spacing: 0.25rem;
    }
    .pair-row button {
      border: none; border-radius: 8px; padding: 10px 14px; font-weight: 700;
      background: #4ecca3; color: #06251b; cursor: pointer;
    }
    .unpair-row { display: none; justify-content: center; margin-top: 10px; }
    .btn-unpair {
      background: #252838; border: 1px solid #4b232b; color: #f87171;
      border-radius: 8px; padding: 6px 14px; font-size: 0.82rem; cursor: pointer;
      font-weight: 600; transition: background 0.15s, border-color 0.15s, color 0.15s;
    }
    .btn-unpair:hover { background: #3b1820; border-color: #e94560; color: #ffccd5; }

    .stage-selector {
      display: flex; justify-content: space-between; align-items: center; gap: 12px;
      background: #181a24; border: 1px solid #303443; border-radius: 12px;
      padding: 10px 12px;
    }
    .stage-selector label { font-size: 0.9rem; font-weight: 800; color: #f0f2f5; }
    .stage-selector .local-note { font-size: 0.72rem; font-weight: 500; color: #777; margin-top: 2px; }
    .stage-selector select {
      min-width: 92px; border: 1px solid #3f4558; background: #0d0e15; color: #fff;
      border-radius: 8px; padding: 9px 12px; font-size: 1rem; font-weight: 700;
    }

    /* Big Timer Card */
    .timer-card {
      background: #141620; border-radius: 16px; padding: 20px 14px;
      text-align: center; border: 2px solid #232738;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
    }
    .state-badge {
      display: inline-block; font-size: 0.9rem; font-weight: 800;
      letter-spacing: 1.5px; padding: 4px 14px; border-radius: 20px;
      background: #282b3a; color: #aaa; margin-bottom: 8px; text-transform: uppercase;
    }
    .state-STANDBY { background: #1e293b; color: #94a3b8; }
    .state-DELAY { background: #78350f; color: #fde047; animation: pulse 0.8s infinite alternate; }
    .state-RUNNING { background: #064e3b; color: #34d399; }
    .state-FINISHED { background: #1e3a8a; color: #60a5fa; }
    .state-FALSE_START { background: #881337; color: #f43f5e; }
    @keyframes pulse { from { opacity: 0.7; } to { opacity: 1.0; } }

    .timer-display {
      font-size: 4.2rem; font-weight: 900; font-family: monospace;
      font-variant-numeric: tabular-nums; line-height: 1.05;
      color: #fff; margin: 6px 0;
    }
    .penalty-text { font-size: 0.95rem; color: #e94560; font-weight: 600; min-height: 20px; }

    /* Target Matrix */
    .target-grid {
      display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px;
      margin-top: 10px;
    }
    .target-cell {
      background: #1c1f2e; border: 1px solid #2d3248; border-radius: 8px;
      padding: 8px 4px; text-align: center;
    }
    .target-cell.hit { background: #064e3b; border-color: #10b981; }
    .target-cell .t-name { font-size: 0.75rem; font-weight: 700; color: #888; }
    .target-cell.hit .t-name { color: #34d399; }
    .target-cell .t-split { font-size: 0.75rem; font-family: monospace; font-weight: 700; color: #fff; margin-top: 2px; }

    /* Mode Selector */
    .mode-bar {
      display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px;
      background: #181a24; padding: 6px; border-radius: 10px;
    }
    .mode-btn {
      background: transparent; border: none; color: #888;
      padding: 8px 4px; border-radius: 6px; font-size: 0.75rem; font-weight: 700;
      cursor: pointer; text-align: center;
    }
    .mode-btn.active { background: #4ecca3; color: #0d0e15; }

    /* Main Action Controls */
    .controls { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .btn-action {
      border: none; border-radius: 12px; padding: 18px 12px;
      font-size: 1.15rem; font-weight: 800; cursor: pointer;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      gap: 4px; transition: transform 0.05s, opacity 0.2s;
    }
    .btn-action:active { transform: scale(0.97); }
    .btn-action:disabled { opacity: 0.35; cursor: not-allowed; transform: none; }
    .btn-start { background: #10b981; color: #022c22; grid-column: span 2; padding: 22px; font-size: 1.35rem; }
    .btn-stop { background: #e94560; color: #fff; }
    .btn-reset { background: #374151; color: #f3f4f6; }
    .btn-sub { font-size: 0.75rem; font-weight: 500; opacity: 0.85; }

    /* Split Details Card */
    .card-box {
      background: #181a24; border-radius: 12px; padding: 12px;
      border: 1px solid #282b3a;
    }
    .card-header {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 8px; font-size: 0.85rem; font-weight: 700; color: #888;
      text-transform: uppercase;
    }
    .split-row {
      display: flex; justify-content: space-between; padding: 6px 0;
      border-bottom: 1px solid #222638; font-size: 0.85rem; font-family: monospace;
    }
    .split-row:last-child { border-bottom: none; }
    .split-row .label { color: #aaa; }
    .split-row .val { font-weight: 700; color: #4ecca3; }
    .split-row .delta { color: #888; font-size: 0.8rem; margin-left: 6px; }

    /* History Filter & Summary */
    .hist-filter {
      display: flex; gap: 4px; margin-bottom: 8px; overflow-x: auto;
    }
    .filter-chip {
      background: #252838; border: 1px solid #363c52; color: #aaa;
      padding: 4px 10px; border-radius: 14px; font-size: 0.75rem; font-weight: 600;
      cursor: pointer; white-space: nowrap;
    }
    .filter-chip.active { background: #4ecca3; border-color: #4ecca3; color: #0d0e15; }

    .stats-bar {
      display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px;
      background: #12141c; padding: 8px; border-radius: 8px; margin-bottom: 8px;
      text-align: center; font-size: 0.75rem;
    }
    .stats-bar .s-val { font-size: 0.95rem; font-weight: 800; color: #fff; font-family: monospace; }
    .stats-bar .s-lbl { color: #888; font-size: 0.7rem; }

    .hist-item {
      padding: 8px 0; border-bottom: 1px solid #222638; font-size: 0.8rem;
    }
    .hist-item:last-child { border-bottom: none; }
    .hist-main {
      display: flex; justify-content: space-between; align-items: center; cursor: pointer;
    }
    .hist-tag {
      background: #282b3a; color: #aaa; padding: 2px 6px; border-radius: 4px;
      font-size: 0.7rem; font-weight: 700;
    }
    .hist-time { font-weight: 800; font-size: 1.05rem; font-family: monospace; color: #4ecca3; }
    .hist-penalty { color: #e94560; font-size: 0.75rem; margin-left: 4px; }
    .hist-splits {
      display: none; margin-top: 6px; padding: 6px 8px; background: #12141c;
      border-radius: 6px; font-size: 0.75rem; font-family: monospace; color: #bbb;
    }

    .hist-actions {
      display: flex; justify-content: space-between; align-items: center;
      margin-top: 10px; font-size: 0.75rem;
    }
    .btn-sm {
      background: #252838; border: 1px solid #3d425a; color: #aaa;
      padding: 4px 8px; border-radius: 6px; cursor: pointer;
    }
    .btn-sm:hover { color: #fff; }

    /* Navigation footer */
    .footer-links { text-align: center; margin-top: 8px; font-size: 0.8rem; }
    .footer-links a { color: #555; text-decoration: none; }
    .footer-links a:hover { color: #888; }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header with Crosshair Logo -->
    <div class="header">
      <div class="title">
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#4ecca3" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
          <circle cx="12" cy="12" r="8"></circle>
          <line x1="12" y1="2" x2="12" y2="6"></line>
          <line x1="12" y1="18" x2="12" y2="22"></line>
          <line x1="2" y1="12" x2="6" y2="12"></line>
          <line x1="18" y1="12" x2="22" y2="12"></line>
          <circle cx="12" cy="12" r="1.5" fill="#4ecca3"></circle>
        </svg>
        <span>%APP_NAME%</span>
      </div>
      <div class="header-actions">
        <button id="btn-wake" class="btn-icon" onclick="toggleWakeLock()">%TRAINING_WAKE_OFF%</button>
        <button id="btn-audio" class="btn-icon" onclick="toggleAudio()">%TRAINING_BEEP_OFF%</button>
      </div>
    </div>

    <!-- Tournament Mode Warning -->
    <div id="tourn-banner" class="banner-tourn">
      %TRAINING_PC_LOCKED%
    </div>

    <div id="remote-card" class="remote-card">
      <div id="remote-message">%TRAINING_RELEASE_REQUIRED%</div>
      <div id="pair-row" class="pair-row">
        <input id="pair-pin" type="text" inputmode="numeric" pattern="[0-9]*" maxlength="4" placeholder="%TRAINING_PIN_PLACEHOLDER%">
        <button onclick="pairPhone()">%TRAINING_PAIR%</button>
      </div>
      <div id="unpair-row" class="unpair-row">
        <button type="button" class="btn-unpair" onclick="unpairPhone()">%TRAINING_DISCONNECT%</button>
      </div>
    </div>

    <div class="stage-selector" id="stage-selector-wrap">
      <div>
        <label for="stage-select">%TRAINING_STAGE%</label>
        <div class="local-note">%TRAINING_LOCAL_ONLY%</div>
      </div>
      <select id="stage-select" onchange="saveSelectedStage()"></select>
    </div>

    <div class="stage-selector prep-delay-selector">
      <div>
        <label for="prep-delay-select">%TRAINING_PREP_DELAY%</label>
        <div class="local-note">%TRAINING_PREP_DELAY_HINT%</div>
      </div>
      <select id="prep-delay-select" onchange="setPrepDelay(this.value)">
        <option value="0">%TRAINING_PREP_DELAY_OFF%</option>
        <option value="1">1 s</option>
        <option value="2">2 s</option>
        <option value="3">3 s</option>
        <option value="4">4 s</option>
        <option value="5">5 s</option>
        <option value="6">6 s</option>
        <option value="7">7 s</option>
        <option value="8">8 s</option>
        <option value="10">10 s</option>
        <option value="12">12 s</option>
        <option value="15">15 s</option>
        <option value="20">20 s</option>
        <option value="30">30 s</option>
      </select>
    </div>

    <!-- Timer Card -->
    <div class="timer-card">
      <div id="state-tag" class="state-badge state-STANDBY">%TRAINING_STANDBY%</div>
      <div id="timer-val" class="timer-display">0.000</div>
      <div id="penalty-val" class="penalty-text"></div>

      <!-- Target Matrix -->
      <div class="target-grid" id="target-matrix">
        <div class="target-cell" id="tgt-1"><div class="t-name">%TRAINING_TARGET_PREFIX%1</div><div class="t-split" id="split-1">&ndash;</div></div>
        <div class="target-cell" id="tgt-2"><div class="t-name">%TRAINING_TARGET_PREFIX%2</div><div class="t-split" id="split-2">&ndash;</div></div>
        <div class="target-cell" id="tgt-3"><div class="t-name">%TRAINING_TARGET_PREFIX%3</div><div class="t-split" id="split-3">&ndash;</div></div>
        <div class="target-cell" id="tgt-4"><div class="t-name">%TRAINING_TARGET_PREFIX%4</div><div class="t-split" id="split-4">&ndash;</div></div>
        <div class="target-cell" id="tgt-0"><div class="t-name">%TRAINING_STOP_PLATE%</div><div class="t-split" id="split-0">&ndash;</div></div>
      </div>
    </div>

    <!-- Mode Selector -->
    <div class="mode-bar">
      <button class="mode-btn active" id="mode-0" onclick="setMode(0)">%TRAINING_MODE_COMPETITION%</button>
      <button class="mode-btn" id="mode-1" onclick="setMode(1)">%TRAINING_MODE_ALL_TARGETS%</button>
      <button class="mode-btn" id="mode-2" onclick="setMode(2)">%TRAINING_MODE_DRILL%</button>
    </div>

    <!-- Action Controls -->
    <div class="controls" id="ctrl-panel">
      <button id="btn-start" class="btn-action btn-start" onclick="sendAction('start')">
        <span>%TRAINING_START%</span>
        <span class="btn-sub">%TRAINING_COUNTDOWN_START%</span>
      </button>
      <button id="btn-stop" class="btn-action btn-stop" onclick="sendAction('stop')">
        <span>%TRAINING_STOP%</span>
        <span class="btn-sub">%TRAINING_ROUND_STOP%</span>
      </button>
      <button id="btn-reset" class="btn-action btn-reset" onclick="sendAction('reset')">
        <span>%TRAINING_RESET_ACTION%</span>
        <span class="btn-sub">%TRAINING_RESET%</span>
      </button>
    </div>

    <!-- Splits List of Current Round -->
    <div class="card-box">
      <div class="card-header">%TRAINING_CURRENT_SPLITS%</div>
      <div id="splits-list"><div style="color:#666;font-size:0.8rem;text-align:center;padding:6px;">%TRAINING_NO_HITS%</div></div>
    </div>

    <!-- Local Training History & Stats -->
    <div class="card-box">
      <div class="card-header">
        <span>%TRAINING_HISTORY_LOCAL%</span>
        <span id="hist-count-badge" style="font-size:0.75rem;color:#4ecca3;">0 %TRAINING_RUNS%</span>
      </div>

      <!-- Mode Filters -->
      <div class="hist-filter">
        <button class="filter-chip active" onclick="setHistoryFilter('all', this)">%TRAINING_ALL%</button>
        <button class="filter-chip" onclick="setHistoryFilter(0, this)">%TRAINING_MODE_COMPETITION%</button>
        <button class="filter-chip" onclick="setHistoryFilter(1, this)">%TRAINING_MODE_ALL_TARGETS%</button>
        <button class="filter-chip" onclick="setHistoryFilter(2, this)">%TRAINING_MODE_DRILL%</button>
      </div>

      <!-- Summary Stats -->
      <div class="stats-bar">
        <div>
          <div class="s-val" id="stat-best">&ndash;</div>
          <div class="s-lbl">%TRAINING_BEST_TIME%</div>
        </div>
        <div>
          <div class="s-val" id="stat-avg">&ndash;</div>
          <div class="s-lbl">%TRAINING_AVERAGE%</div>
        </div>
        <div>
          <div class="s-val" id="stat-count">0</div>
          <div class="s-lbl">%TRAINING_RUNS%</div>
        </div>
      </div>

      <!-- History Items List -->
      <div id="hist-list" style="max-height: 220px; overflow-y: auto;">
        <div style="color:#666;font-size:0.8rem;text-align:center;padding:8px;">%TRAINING_NO_RUNS_SAVED%</div>
      </div>

      <!-- History Actions -->
      <div class="hist-actions">
        <button class="btn-sm" onclick="exportHistoryCSV()">%TRAINING_COPY_CSV%</button>
        <button class="btn-sm" style="color:#e94560;" onclick="clearHistory()">%TRAINING_DELETE_HISTORY%</button>
      </div>
    </div>

    <p id="request-error" role="alert" style="display:none;color:#e94560"></p>
    <div class="footer-links">
      <a href="/settings">%TRAINING_SETTINGS_LINK%</a>
    </div>
  </div>

  <script>
    const TEXT = {
      connectionInterrupted: '%WEB_CONNECTION_INTERRUPTED%',
      commandBusy: '%WEB_API_COMMAND_BUSY%',
      wakeOff: '%TRAINING_WAKE_OFF%',
      wakeOn: '%TRAINING_WAKE_ON%',
      beepOff: '%TRAINING_BEEP_OFF%',
      beepOn: '%TRAINING_BEEP_ON%',
      modeCompetition: '%TRAINING_MODE_COMPETITION%',
      modeAllTargets: '%TRAINING_MODE_ALL_TARGETS%',
      modeDrill: '%TRAINING_MODE_DRILL%',
      stage: '%TRAINING_STAGE%',
      stage5ToGo: '%TRAINING_STAGE_5_TO_GO%',
      stageShowdown: '%TRAINING_STAGE_SHOWDOWN%',
      stageSmokeAndHope: '%TRAINING_STAGE_SMOKE_AND_HOPE%',
      stageOuterLimits: '%TRAINING_STAGE_OUTER_LIMITS%',
      stageAccelerator: '%TRAINING_STAGE_ACCELERATOR%',
      stageThePendulum: '%TRAINING_STAGE_THE_PENDULUM%',
      stageSpeedOption: '%TRAINING_STAGE_SPEED_OPTION%',
      stageRoundabout: '%TRAINING_STAGE_ROUNDABOUT%',
      unknown: '%TRAINING_UNKNOWN%',
      runs: '%TRAINING_RUNS%',
      noRunsCategory: '%TRAINING_NO_RUNS_CATEGORY%',
      noHits: '%TRAINING_NO_HITS%',
      noEntries: '%TRAINING_NO_ENTRIES%',
      csvHeader: '%TRAINING_CSV_HEADER%',
      csvCopied: '%TRAINING_CSV_COPIED%',
      csvPrompt: '%TRAINING_CSV_PROMPT%',
      clearConfirm: '%TRAINING_CLEAR_CONFIRM%',
      pinInvalid: '%TRAINING_PIN_INVALID%',
      pairFailed: '%TRAINING_PAIR_FAILED%',
      controller: '%TRAINING_CONTROLLER%',
      pairPrompt: '%TRAINING_PAIR_PROMPT%',
      otherPaired: '%TRAINING_OTHER_PAIRED%',
      pcPriority: '%TRAINING_PC_PRIORITY%',
      releaseMainMenu: '%TRAINING_RELEASE_MAIN_MENU%',
      nextRun: '%TRAINING_NEXT_RUN%',
      countdownStart: '%TRAINING_COUNTDOWN_START%',
      abortCountdown: '%TRAINING_ABORT_COUNTDOWN%',
      roundStop: '%TRAINING_ROUND_STOP%',
      backToStandby: '%TRAINING_BACK_TO_STANDBY%',
      discardRun: '%TRAINING_DISCARD_RUN%',
      penaltyLabel: '%TRAINING_PENALTY_LABEL%',
      stopPlate: '%TRAINING_STOP_PLATE%',
      targetLabel: '%TRAINING_TARGET_LABEL%',
      targetPrefix: '%TRAINING_TARGET_PREFIX%',
      secondsUnit: '%TRAINING_SECONDS_UNIT%',
      missAbbreviation: '%TRAINING_MISS_ABBREVIATION%',
      disconnect: '%TRAINING_DISCONNECT%',
      prepDelay: '%TRAINING_PREP_DELAY%',
      prepDelayHint: '%TRAINING_PREP_DELAY_HINT%',
      prepDelayOff: '%TRAINING_PREP_DELAY_OFF%',
      prepare: '%TRAINING_PREPARE%'
    };

    const STAGES = [
      {id: 'SC-101', name: TEXT.stage5ToGo},
      {id: 'SC-102', name: TEXT.stageShowdown},
      {id: 'SC-103', name: TEXT.stageSmokeAndHope},
      {id: 'SC-104', name: TEXT.stageOuterLimits},
      {id: 'SC-105', name: TEXT.stageAccelerator},
      {id: 'SC-106', name: TEXT.stageThePendulum},
      {id: 'SC-107', name: TEXT.stageSpeedOption},
      {id: 'SC-108', name: TEXT.stageRoundabout}
    ];

    let audioEnabled = false;
    let audioCtx = null;
    let wakeLock = null;
    let wakeLockEnabled = false;
    let wakeVideo = null;
    let lastState = 'STANDBY';
    let startBeepTimer = null;
    let scheduledCountdownId = 0;
    let beepedCountdownId = 0;
    let estimatedReturnLatencyMs = 0;
    let returnLatencyInitialized = false;
    let remoteToken = sessionStorage.getItem('es_remote_token') || '';
    let lastRecordedRoundId = localStorage.getItem('es_last_result_id') || '';
    let pollInterval = 200;
    let actionPending = false;
    let connectionFailed = false;

    function showRequestError(message) {
      const element = document.getElementById('request-error');
      element.textContent = message;
      element.style.display = message ? 'block' : 'none';
    }

    async function requestJson(url, options = {}) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), 4000);
      try {
        const response = await fetch(url, {...options, signal: controller.signal, cache: 'no-store'});
        if (response.status === 401) {
          remoteToken = '';
          sessionStorage.removeItem('es_remote_token');
        }
        let data;
        try { data = await response.json(); } catch (_) { data = {}; }
        if (!response.ok) {
          throw new Error(data.error || (response.status === 503 ? TEXT.commandBusy : TEXT.connectionInterrupted));
        }
        return data;
      } finally {
        clearTimeout(timer);
      }
    }

    let historyFilter = 'all';
    let selectedStageId = loadSelectedStageId();

    function findStage(stageId) {
      return STAGES.find(stage => stage.id === stageId);
    }

    function loadSelectedStageId() {
      try {
        const storedStage = localStorage.getItem('es_selected_stage');
        if (findStage(storedStage)) return storedStage;

        const legacyStage = parseInt(storedStage, 10);
        if (legacyStage >= 1 && legacyStage <= STAGES.length) {
          return STAGES[legacyStage - 1].id;
        }
      } catch (e) {}
      return STAGES[0].id;
    }

    function initStageSelection() {
      const select = document.getElementById('stage-select');
      STAGES.forEach(stage => {
        const option = document.createElement('option');
        option.value = stage.id;
        option.textContent = stage.name;
        select.appendChild(option);
      });
      select.value = selectedStageId;
    }

    function saveSelectedStage() {
      const select = document.getElementById('stage-select');
      const stageId = select.value;
      if (!findStage(stageId)) return;
      selectedStageId = stageId;
      try {
        localStorage.setItem('es_selected_stage', stageId);
      } catch (e) {}
    }

    function getHistoryStageName(run) {
      const stage = findStage(run.stageId);
      if (stage) return stage.name;
      if (Number.isInteger(run.stage) && run.stage >= 1 && run.stage <= 99) {
        return TEXT.stage + ' ' + run.stage;
      }
      return '';
    }

    // --- Screen Wake Lock (Dual API + NoSleep Fallback for HTTP) ---
    function initWakeVideo() {
      if (!wakeVideo) {
        wakeVideo = document.createElement('video');
        wakeVideo.setAttribute('playsinline', '');
        wakeVideo.setAttribute('webkit-playsinline', '');
        wakeVideo.setAttribute('loop', '');
        wakeVideo.muted = true;
        wakeVideo.playsInline = true;
        wakeVideo.src = 'data:video/mp4;base64,AAAAHGZ0eXBtcDQyAAAAAG1wNDJpc29tYXZjMQAAAAhmcmVlAAAAxm1kYXQAAAFyAAABt2xpYmZhaWMgMS40MC4wAAAAAAAAAB41bmNmQAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACbW9vdgAAAGxtdmhkAAAAAAAAAAAAAAAAAAAD6AAAA+gAAAPoAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAA==';
        wakeVideo.style.position = 'fixed';
        wakeVideo.style.bottom = '0';
        wakeVideo.style.left = '0';
        wakeVideo.style.width = '1px';
        wakeVideo.style.height = '1px';
        wakeVideo.style.opacity = '0.01';
        wakeVideo.style.pointerEvents = 'none';
        document.body.appendChild(wakeVideo);
      }
    }

    async function enableWakeLock() {
      wakeLockEnabled = true;
      updateWakeUI(true);

      if ('wakeLock' in navigator) {
        try {
          wakeLock = await navigator.wakeLock.request('screen');
          wakeLock.addEventListener('release', () => {
            wakeLock = null;
          });
        } catch (e) {}
      }

      try {
        initWakeVideo();
        if (wakeVideo) {
          const p = wakeVideo.play();
          if (p !== undefined) p.catch(() => {});
        }
      } catch (e) {}
    }

    function disableWakeLock() {
      wakeLockEnabled = false;
      updateWakeUI(false);

      if (wakeLock) {
        try { wakeLock.release(); } catch (e) {}
        wakeLock = null;
      }
      if (wakeVideo) {
        try { wakeVideo.pause(); } catch (e) {}
      }
    }

    function updateWakeUI(active) {
      const btn = document.getElementById('btn-wake');
      if (active) {
        btn.classList.add('active');
        btn.innerHTML = TEXT.wakeOn;
      } else {
        btn.classList.remove('active');
        btn.innerHTML = TEXT.wakeOff;
      }
    }

    function toggleWakeLock() {
      if (wakeLockEnabled) {
        disableWakeLock();
      } else {
        enableWakeLock();
      }
    }

    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible' && wakeLockEnabled) {
        enableWakeLock();
      }
    });

    // --- Optional phone feedback; the ESP buzzer remains the authoritative start signal ---
    function initAudio() {
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      }
      if (audioCtx.state === 'suspended') {
        audioCtx.resume();
      }
    }

    function toggleAudio() {
      initAudio();
      audioEnabled = !audioEnabled;
      const btn = document.getElementById('btn-audio');
      if (audioEnabled) {
        btn.classList.add('active');
        btn.innerHTML = TEXT.beepOn;
        playBeep(700, 0.08, 'sine');
      } else {
        btn.classList.remove('active');
        btn.innerHTML = TEXT.beepOff;
        cancelScheduledStartBeep(true);
      }
      if (!wakeLockEnabled) {
        enableWakeLock();
      }
    }

    function playStartBeep(duration = 0.32) {
      if (!audioEnabled || !audioCtx) return;
      try {
        initAudio();
        const now = audioCtx.currentTime;
        const compressor = audioCtx.createDynamicsCompressor();
        compressor.threshold.setValueAtTime(-2, now);
        compressor.knee.setValueAtTime(0, now);
        compressor.ratio.setValueAtTime(20, now);
        compressor.attack.setValueAtTime(0.001, now);
        compressor.release.setValueAtTime(0.04, now);
        compressor.connect(audioCtx.destination);

        const masterGain = audioCtx.createGain();
        masterGain.gain.setValueAtTime(1.0, now);
        masterGain.gain.setValueAtTime(1.0, now + duration - 0.015);
        masterGain.gain.linearRampToValueAtTime(0.0, now + duration);
        masterGain.connect(compressor);

        const square = audioCtx.createOscillator();
        square.type = 'square';
        square.frequency.setValueAtTime(2550, now);
        square.connect(masterGain);

        const saw = audioCtx.createOscillator();
        saw.type = 'sawtooth';
        saw.frequency.setValueAtTime(2800, now);
        const sawGain = audioCtx.createGain();
        sawGain.gain.setValueAtTime(0.7, now);
        saw.connect(sawGain);
        sawGain.connect(masterGain);

        square.start(now);
        saw.start(now);
        square.stop(now + duration);
        saw.stop(now + duration);
      } catch (e) {
        console.error('Start beep error:', e);
      }
    }

    function cancelScheduledStartBeep(resetCountdown) {
      if (startBeepTimer !== null) {
        clearTimeout(startBeepTimer);
        startBeepTimer = null;
      }
      if (resetCountdown) {
        scheduledCountdownId = 0;
      }
    }

    function updateEstimatedReturnLatency(roundTripMs) {
      const sampleMs = Math.max(0, roundTripMs / 2);
      if (!Number.isFinite(sampleMs)) return estimatedReturnLatencyMs;
      if (!returnLatencyInitialized) {
        estimatedReturnLatencyMs = sampleMs;
        returnLatencyInitialized = true;
      } else {
        const alpha = 0.2;
        estimatedReturnLatencyMs += alpha * (sampleMs - estimatedReturnLatencyMs);
      }
      return estimatedReturnLatencyMs;
    }

    function synchronizeStartBeep(data, estimatedReturnLatencyMs) {
      const countdownId = Number(data.countdownId) || 0;
      if (!audioEnabled || !data.isController) {
        cancelScheduledStartBeep(true);
        return;
      }

      if (data.state === 'DELAY' && countdownId > 0) {
        if (beepedCountdownId === countdownId) return;
        cancelScheduledStartBeep(false);
        scheduledCountdownId = countdownId;
        const remainingMs = Number(data.startInMs) || 0;
        const waitMs = Math.max(0, remainingMs - estimatedReturnLatencyMs);
        startBeepTimer = setTimeout(() => {
          startBeepTimer = null;
          beepedCountdownId = countdownId;
          playStartBeep();
        }, waitMs);
        return;
      }

      cancelScheduledStartBeep(false);
      if (data.state === 'RUNNING' && lastState === 'DELAY' &&
          countdownId > 0 && beepedCountdownId !== countdownId) {
        scheduledCountdownId = countdownId;
        beepedCountdownId = countdownId;
        playStartBeep();
      } else if (data.state !== 'RUNNING') {
        scheduledCountdownId = 0;
      }
    }

    function playBeep(freq = 650, duration = 0.4, type = 'sawtooth') {
      if (!audioEnabled || !audioCtx) return;
      try {
        initAudio();
        const now = audioCtx.currentTime;
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = type;
        osc.frequency.setValueAtTime(freq, now);
        gain.gain.setValueAtTime(0.9, now);
        gain.gain.exponentialRampToValueAtTime(0.001, now + duration);
        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.start(now);
        osc.stop(now + duration);
      } catch (e) {
        console.error('Audio error:', e);
      }
    }

    // --- Local Training History Management ---
    function loadHistory() {
      try {
        const data = localStorage.getItem('es_history');
        return data ? JSON.parse(data) : [];
      } catch (e) {
        return [];
      }
    }

    function saveHistory(hist) {
      try {
        localStorage.setItem('es_history', JSON.stringify(hist.slice(0, 100)));
      } catch (e) {}
    }

    function recordTrainingResult(result) {
      if (!result || !result.id || result.timeMs <= 0) return;
      const existingHistory = loadHistory();
      const existingIndex = existingHistory.findIndex(item => item.id === result.id);
      const existing = existingIndex >= 0 ? existingHistory[existingIndex] : null;
      if (existing && existing.finalTimeMs === result.timeMs &&
          existing.penaltyCount === result.penaltyCount && existing.missCount === result.missCount) return;
      // Preserve explicit local deletion when the device still publishes the same run.
      if (!existing && result.id === lastRecordedRoundId) return;
      const now = new Date();
      const timeStr = now.toLocaleString([], {
        year: 'numeric', month: '2-digit', day: '2-digit',
        hour: '2-digit', minute:'2-digit', second:'2-digit'
      });
      const modeNames = [TEXT.modeCompetition, TEXT.modeAllTargets, TEXT.modeDrill];

      const run = {
        id: result.id,
        timestamp: existing ? existing.timestamp : timeStr,
        stageId: existing ? existing.stageId : ((result.mode === 2) ? '' : selectedStageId),
        mode: result.mode,
        modeName: modeNames[result.mode] || TEXT.unknown,
        rawTimeMs: result.rawTimeMs,
        finalTimeMs: result.timeMs,
        missCount: (result.mode === 2) ? 0 : (result.missCount || 0),
        penaltyCount: (result.mode === 2) ? 0 : (result.penaltyCount || 0),
        targetCount: result.targetCount || 0,
        hitMask: result.hitMask || [],
        hits: result.hits || []
      };

      const hist = existingHistory;
      if (existingIndex >= 0) hist[existingIndex] = run;
      else hist.unshift(run);
      saveHistory(hist);
      lastRecordedRoundId = result.id;
      localStorage.setItem('es_last_result_id', result.id);
      renderHistoryUI();
    }

    function setHistoryFilter(mode, btn) {
      historyFilter = mode;
      document.querySelectorAll('.filter-chip').forEach(c => c.classList.remove('active'));
      if (btn) btn.classList.add('active');
      renderHistoryUI();
    }

    function renderHistoryUI() {
      const hist = loadHistory();
      const filtered = (historyFilter === 'all')
        ? hist
        : hist.filter(h => h.mode === parseInt(historyFilter));

      document.getElementById('hist-count-badge').innerText = filtered.length + ' ' + TEXT.runs;

      const bestEl = document.getElementById('stat-best');
      const avgEl = document.getElementById('stat-avg');
      const countEl = document.getElementById('stat-count');
      countEl.innerText = filtered.length;

      if (filtered.length === 0) {
        bestEl.innerText = '\u2013';
        avgEl.innerText = '\u2013';
        document.getElementById('hist-list').innerHTML = '<div style="color:#666;font-size:0.8rem;text-align:center;padding:8px;">' + TEXT.noRunsCategory + '</div>';
        return;
      }

      let minMs = Infinity;
      let sumMs = 0;
      let validCount = 0;
      filtered.forEach(h => {
        if (h.mode === 2 && (!h.hits || h.hits.length === 0)) {
          return;
        }
        if (h.finalTimeMs < minMs) minMs = h.finalTimeMs;
        sumMs += h.finalTimeMs;
        validCount++;
      });

      if (validCount > 0) {
        bestEl.innerText = (minMs / 1000).toFixed(3) + TEXT.secondsUnit;
        avgEl.innerText = ((sumMs / validCount) / 1000).toFixed(3) + TEXT.secondsUnit;
      } else {
        bestEl.innerText = '\u2013';
        avgEl.innerText = '\u2013';
      }

      let html = '';
      filtered.forEach(h => {
        const pText = (h.mode !== 2 && h.penaltyCount > 0) ? ('<span class="hist-penalty">+' + h.penaltyCount + TEXT.missAbbreviation + '</span>') : '';
        const stageName = (h.mode === 2) ? '' : getHistoryStageName(h);
        const stageText = stageName ? ('<span class="hist-tag">' + stageName + '</span> ') : '';
        const noHitText = (h.mode === 2 && (!h.hits || h.hits.length === 0)) ? (' <span class="hist-penalty">' + TEXT.noHits + '</span>') : '';
        let splitsHtml = '';
        if (h.hits && h.hits.length > 0) {
          splitsHtml = '<div class="hist-splits" id="h-split-' + h.id + '">';
          let prev = 0;
          h.hits.forEach((hit, idx) => {
            const delta = (idx === 0) ? hit.splitMs : (hit.splitMs - prev);
            prev = hit.splitMs;
            const tName = hit.target === 0 ? TEXT.stopPlate : (TEXT.targetPrefix + hit.target);
            splitsHtml += '<div>#' + (idx + 1) + ' ' + tName + ': ' + (hit.splitMs / 1000).toFixed(3) + TEXT.secondsUnit + ' (+' + (delta / 1000).toFixed(3) + TEXT.secondsUnit + ')</div>';
          });
          splitsHtml += '</div>';
        }

        html += '<div class="hist-item">' +
          '<div class="hist-main" onclick="toggleHistSplits(\'' + h.id + '\')">' +
            '<div>' + stageText + '<span class="hist-tag">' + h.modeName + '</span> <span style="color:#777;margin-left:4px;">' + h.timestamp + '</span></div>' +
            '<div><span class="hist-time">' + (h.finalTimeMs / 1000).toFixed(3) + TEXT.secondsUnit + '</span>' + pText + noHitText + ' &#9662;</div>' +
          '</div>' +
          splitsHtml +
        '</div>';
      });

      document.getElementById('hist-list').innerHTML = html;
    }

    function toggleHistSplits(id) {
      const el = document.getElementById('h-split-' + id);
      if (el) {
        el.style.display = (el.style.display === 'block') ? 'none' : 'block';
      }
    }

    function exportHistoryCSV() {
      const hist = loadHistory();
      if (hist.length === 0) {
        alert(TEXT.noEntries);
        return;
      }
      let csv = TEXT.csvHeader;
      hist.forEach(h => {
        const splitsStr = (h.hits || []).map(s => (s.target === 0 ? TEXT.stopPlate : TEXT.targetPrefix + s.target) + ':' + (s.splitMs / 1000).toFixed(3)).join(' | ');
        const stageName = getHistoryStageName(h);
        csv += '"' + h.timestamp + '","' + stageName + '","' + h.modeName + '",' + ((h.rawTimeMs || h.finalTimeMs) / 1000).toFixed(3) + ',' + (h.finalTimeMs / 1000).toFixed(3) + ',' + (h.missCount || 0) + ',"' + splitsStr + '"\n';
      });

      if (navigator.clipboard) {
        navigator.clipboard.writeText(csv).then(() => {
          alert(TEXT.csvCopied);
        });
      } else {
        prompt(TEXT.csvPrompt, csv);
      }
    }

    function clearHistory() {
      if (confirm(TEXT.clearConfirm)) {
        localStorage.removeItem('es_history');
        renderHistoryUI();
      }
    }

    async function sendAction(action, payload = {}) {
      if (!remoteToken || actionPending) return;
      actionPending = true;
      if (action === 'stop' || action === 'reset') cancelScheduledStartBeep(true);
      if (audioEnabled) initAudio();
      if (!wakeLockEnabled) enableWakeLock();
      try {
        const result = await requestJson('/api/action', {
          method: 'POST',
          headers: {'Content-Type': 'application/json', 'X-EasySteel-Token': remoteToken},
          body: JSON.stringify({...payload, action: action})
        });
        if (result.success) {
          showRequestError('');
          playBeep(700, 0.06, 'sine');
        }
      } catch (error) {
        showRequestError(error.message || TEXT.connectionInterrupted);
      } finally {
        actionPending = false;
      }
    }

    async function pairPhone() {
      if (actionPending) return;
      const input = document.getElementById('pair-pin');
      const pin = input.value.trim();
      if (!/^\d{4}$/.test(pin)) {
        showRequestError(TEXT.pinInvalid);
        return;
      }
      actionPending = true;
      try {
        const result = await requestJson('/api/pair', {
          method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({pin: pin})
        });
        if (result.success && result.token) {
          remoteToken = result.token;
          sessionStorage.setItem('es_remote_token', remoteToken);
          input.value = '';
          showRequestError('');
        } else {
          showRequestError(result.error || TEXT.pairFailed);
        }
      } catch (error) {
        showRequestError(error.message || TEXT.connectionInterrupted);
      } finally {
        actionPending = false;
      }
    }

    async function unpairPhone() {
      if (!remoteToken || actionPending) return;
      actionPending = true;
      cancelScheduledStartBeep(true);
      try {
        await requestJson('/api/action', {
          method: 'POST',
          headers: {'Content-Type': 'application/json', 'X-EasySteel-Token': remoteToken},
          body: JSON.stringify({action: 'disconnect'})
        });
      } catch (_) {
        // Clear session locally regardless of network outcome
      } finally {
        remoteToken = '';
        sessionStorage.removeItem('es_remote_token');
        actionPending = false;
        updateDashboard();
      }
    }

    function updateRemoteUI(data) {
      const remoteCard = document.getElementById('remote-card');
      const message = document.getElementById('remote-message');
      const pairRow = document.getElementById('pair-row');
      const unpairRow = document.getElementById('unpair-row');
      const banner = document.getElementById('tourn-banner');

      banner.style.display = data.pcConnected ? 'block' : 'none';
      pairRow.style.display = 'none';
      if (unpairRow) unpairRow.style.display = 'none';
      remoteCard.style.borderColor = '#303443';

      if (data.isController) {
        message.innerText = TEXT.controller;
        remoteCard.style.borderColor = '#2d6a4f';
        if (unpairRow) unpairRow.style.display = 'flex';
      } else if (data.remoteState === 'pairing') {
        message.innerText = TEXT.pairPrompt;
        pairRow.style.display = 'grid';
      } else if (data.remoteState === 'paired') {
        message.innerText = TEXT.otherPaired;
      } else if (data.remoteState === 'blocked') {
        message.innerText = TEXT.pcPriority;
      } else {
        message.innerText = TEXT.releaseMainMenu;
      }
    }

    function setMode(mode) {
      sendAction('mode', {mode: mode});
    }

    function setPrepDelay(delay) {
      sendAction('delay', {delay: parseInt(delay, 10)});
    }

    // --- State Polling & Render ---
    function updateDashboard() {
      const requestStartedAt = performance.now();
      const headers = remoteToken ? {'X-EasySteel-Token': remoteToken} : {};
      requestJson('/api/state', {headers: headers})
        .then(data => {
          if (connectionFailed) { showRequestError(''); connectionFailed = false; }
          const startBtn = document.getElementById('btn-start');
          const stopBtn = document.getElementById('btn-stop');
          const resetBtn = document.getElementById('btn-reset');
          const modeBtns = document.querySelectorAll('.mode-btn');

          if (remoteToken && !data.isController) {
            remoteToken = '';
            sessionStorage.removeItem('es_remote_token');
          }
          updateRemoteUI(data);

          const canControl = data.isController === true && !actionPending;
          startBtn.disabled = !canControl || (data.state !== 'STANDBY' && data.state !== 'FINISHED');
          stopBtn.disabled = !canControl || (data.state !== 'DELAY' && data.state !== 'RUNNING');
          resetBtn.disabled = !canControl || (data.state !== 'DELAY' && data.state !== 'RUNNING' && data.state !== 'FINISHED');
          modeBtns.forEach(b => b.disabled = !canControl || data.state !== 'STANDBY');
          const prepSelect = document.getElementById('prep-delay-select');
          if (prepSelect && !prepSelect.matches(':focus')) {
            prepSelect.value = String(data.prepDelay !== undefined ? data.prepDelay : 0);
            prepSelect.disabled = !canControl || data.state !== 'STANDBY';
          }

          startBtn.querySelector('.btn-sub').innerText = data.state === 'FINISHED' ? TEXT.nextRun : TEXT.countdownStart;
          stopBtn.querySelector('.btn-sub').innerText = data.state === 'DELAY' ? TEXT.abortCountdown : TEXT.roundStop;
          resetBtn.querySelector('.btn-sub').innerText = data.state === 'FINISHED' ? TEXT.backToStandby : TEXT.discardRun;

          // State badge
          const stateTag = document.getElementById('state-tag');
          if (data.state === 'DELAY' && data.prepInMs > 0) {
            stateTag.innerText = TEXT.prepare;
          } else {
            stateTag.innerText = data.state;
          }
          stateTag.className = 'state-badge state-' + data.state;

          const requestRoundTripMs = performance.now() - requestStartedAt;
          const returnLatencyMs = updateEstimatedReturnLatency(requestRoundTripMs);
          synchronizeStartBeep(data, returnLatencyMs);

          if (lastState === 'DELAY' && data.state === 'FALSE_START') {
            playBeep(450, 0.5, 'sawtooth');
          } else if (lastState === 'RUNNING' && data.state === 'FINISHED') {
            playBeep(800, 0.08, 'sine');
          }

          // Store each ESP-published training result exactly once on the paired phone.
          if (data.isController && data.trainingResult) {
            recordTrainingResult(data.trainingResult);
          }
          lastState = data.state;

          // Timer display
          if (data.state === 'DELAY' && data.prepInMs > 0) {
            const prepSecs = Math.ceil(data.prepInMs / 1000);
            document.getElementById('timer-val').innerText = prepSecs + ' ' + TEXT.secondsUnit;
          } else {
            const tVal = (data.timeMs / 1000).toFixed(3);
            document.getElementById('timer-val').innerText = tVal;
          }

          // Stage selector visibility
          const stageSelector = document.getElementById('stage-selector-wrap');
          if (stageSelector) {
            stageSelector.style.display = (data.mode === 2) ? 'none' : 'flex';
          }

          // Penalty text
          const pEl = document.getElementById('penalty-val');
          if (data.mode === 2) {
            if (data.state === 'FINISHED' && (!data.hits || data.hits.length === 0)) {
              pEl.innerText = TEXT.noHits;
            } else {
              pEl.innerText = '';
            }
          } else if (data.state === 'FINISHED' && data.penaltyCount > 0) {
            pEl.innerText = '+' + data.penaltyCount + ' ' + TEXT.penaltyLabel + ' (+' + (data.penaltyCount * 3.0).toFixed(1) + TEXT.secondsUnit + ')';
          } else {
            pEl.innerText = '';
          }

          // Mode buttons
          for (let m = 0; m <= 2; m++) {
            const b = document.getElementById('mode-' + m);
            if (b) {
              if (data.mode === m) b.classList.add('active');
              else b.classList.remove('active');
            }
          }

          // Targets matrix
          const mask = data.hitMask || [];
          for (let i = 0; i <= 4; i++) {
            const cell = document.getElementById('tgt-' + i);
            const sp = document.getElementById('split-' + i);
            if (cell) {
              if (mask[i]) {
                cell.classList.add('hit');
              } else {
                cell.classList.remove('hit');
                if (sp) sp.innerText = '\u2013';
              }
            }
          }

          // Splits list of active round
          const hits = data.hits || [];
          const listEl = document.getElementById('splits-list');
          if (hits.length === 0) {
            listEl.innerHTML = '<div style="color:#666;font-size:0.8rem;text-align:center;padding:6px;">' + TEXT.noHits + '</div>';
          } else {
            let html = '';
            let prev = 0;
            hits.forEach((h, idx) => {
              const delta = (idx === 0) ? h.splitMs : (h.splitMs - prev);
              prev = h.splitMs;
              const name = h.target === 0 ? TEXT.stopPlate : (TEXT.targetLabel + ' ' + h.target);
              const spEl = document.getElementById('split-' + h.target);
              if (spEl) spEl.innerText = (h.splitMs / 1000).toFixed(3) + TEXT.secondsUnit;

              html += '<div class="split-row">' +
                '<span class="label">#' + (idx + 1) + ' ' + name + '</span>' +
                '<span class="val">' + (h.splitMs / 1000).toFixed(3) + TEXT.secondsUnit + ' <span class="delta">(+' + (delta / 1000).toFixed(3) + TEXT.secondsUnit + ')</span></span>' +
                '</div>';
            });
            listEl.innerHTML = html;
          }

          // Active states use the normal live refresh rate; start audio is scheduled locally.
          if (data.state === 'RUNNING' || data.state === 'DELAY') {
            pollInterval = 200;
          } else {
            pollInterval = 450;
          }
        })
        .catch(err => {
          connectionFailed = true;
          showRequestError(TEXT.connectionInterrupted);
          document.querySelectorAll('#btn-start, #btn-stop, #btn-reset, .mode-btn').forEach(button => button.disabled = true);
          cancelScheduledStartBeep(true);
          pollInterval = 1000;
        })
        .finally(() => {
          setTimeout(updateDashboard, pollInterval);
        });
    }

    initStageSelection();
    renderHistoryUI();
    updateDashboard();
  </script>
</body>
</html>
)rawliteral";
