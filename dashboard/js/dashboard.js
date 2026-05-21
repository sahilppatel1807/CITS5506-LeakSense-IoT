/**
 * dashboard.js
 * Main dashboard logic.
 * Handles live readings, alert log, 24hr history table, and state machine.
 */

// ── Thresholds — must match firmware/src/config.h ─────────────────────────────
const THRESHOLDS = { warning: 300, danger: 500 };

// ── State ──────────────────────────────────────────────────────────────────────
let alertCount = 0;
let prevState  = 'safe';
const alertLog = [];

// 24-hour history table data
const historyRows = [];
const MAX_HISTORY_ROWS = 288; // 24hr at 1 reading per 5 min

// ── Helpers ────────────────────────────────────────────────────────────────────

function getStateFromPpm(ppm) {
  if (ppm >= THRESHOLDS.danger)  return 'danger';
  if (ppm >= THRESHOLDS.warning) return 'warning';
  return 'safe';
}

function formatTime(date = new Date()) {
  return date.toLocaleTimeString('en-AU', {
    hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false,
  });
}

function formatDateTime(date = new Date()) {
  return date.toLocaleString('en-AU', {
    day: '2-digit', month: '2-digit',
    hour: '2-digit', minute: '2-digit', hour12: false,
  });
}

function getReadingDate(timestamp) {
  if (!timestamp) return new Date();
  const n = Number(timestamp);
  if (!Number.isFinite(n)) return new Date();
  return new Date(n < 10000000000 ? n * 1000 : n);
}

// ── UI — status & metrics ──────────────────────────────────────────────────────

const STATE_MESSAGES = {
  safe:    'System operating normally. Gas levels within safe range.',
  warning: 'Warning: elevated gas concentration detected. Monitor the area.',
  danger:  'DANGER: critical gas level detected. Immediate action required. Fan and buzzer active.',
};

const PPM_SUBS = {
  safe:    'Within safe range',
  warning: 'Above warning threshold',
  danger:  'Above danger threshold',
};

function updateStatusBadge(state) {
  const el = document.getElementById('statusBadge');
  el.className  = `status-badge ${state}`;
  el.textContent = state.charAt(0).toUpperCase() + state.slice(1);
}

function updateStateBar(state) {
  const el = document.getElementById('stateBar');
  el.className  = `state-bar ${state}`;
  el.textContent = STATE_MESSAGES[state];
}

function updateMetrics(data) {
  document.getElementById('ppmVal').textContent  = `${data.ppm_compensated} ppm`;
  document.getElementById('ppmSub').textContent  = PPM_SUBS[data.state];
  document.getElementById('tempVal').textContent = `${data.temperature}°C`;
  document.getElementById('humVal').textContent  = `${data.humidity}%`;
  document.getElementById('lastSync').textContent = `Last sync: ${formatTime(getReadingDate(data.timestamp))}`;
}

// ── UI — device states with yellow LED ────────────────────────────────────────

function updateDeviceStates(data) {
  // Fan — only on in Danger
  const fan = document.getElementById('fanState');
  fan.textContent = data.fan ? 'on' : 'off';
  fan.className   = `pill${data.fan ? ' pill--on' : ''}`;

  // Buzzer — only on in Danger
  const buz = document.getElementById('buzzerState');
  buz.textContent = data.buzzer ? 'on' : 'off';
  buz.className   = `pill${data.buzzer ? ' pill--on' : ''}`;

  // Green LED — Safe only
  const green = document.getElementById('greenLed');
  green.textContent = data.state === 'safe' ? 'on' : 'off';
  green.className   = `pill${data.state === 'safe' ? ' pill--on' : ''}`;

  // Yellow LED — Warning only
  const yellow = document.getElementById('yellowLed');
  yellow.textContent = data.state === 'warning' ? 'on' : 'off';
  yellow.className   = `pill${data.state === 'warning' ? ' pill--warning' : ''}`;

  // Red LED — Danger only
  const red = document.getElementById('redLed');
  red.textContent = data.state === 'danger' ? 'on' : 'off';
  red.className   = `pill${data.state === 'danger' ? ' pill--danger' : ''}`;
}

// ── UI — alert log ─────────────────────────────────────────────────────────────

function addAlertEntry(ppm, state) {
  const time = formatTime();
  const msg  = state === 'danger'
    ? `Gas reached ${ppm} ppm — DANGER threshold crossed`
    : `Gas reached ${ppm} ppm — Warning threshold crossed`;

  alertLog.unshift({ time, ppm, state, msg });
  if (alertLog.length > 20) alertLog.pop();

  alertCount++;
  document.getElementById('alertCount').textContent = alertCount;
  renderAlertLog();
}

function renderAlertLog() {
  const el = document.getElementById('alertLog');
  if (alertLog.length === 0) {
    el.innerHTML = '<p class="alert-log__empty">No alerts yet — system safe.</p>';
    return;
  }
  el.innerHTML = alertLog.map(a => `
    <div class="alert-row">
      <span class="alert-row__time">${a.time}</span>
      <span class="alert-row__msg">${a.msg}</span>
      <span class="alert-pill alert-pill--${a.state}">${a.state.toUpperCase()}</span>
    </div>
  `).join('');
}

// ── UI — 24hr history table ────────────────────────────────────────────────────

/**
 * Adds a history reading to the table.
 * Prepends newest rows so most recent is always at the top.
 * @param {object} reading - normalised reading
 */
function addHistoryRow(reading) {
  const date = getReadingDate(reading.timestamp);
  historyRows.unshift({
    time:  formatDateTime(date),
    ppm:   reading.ppm_compensated,
    temp:  reading.temperature,
    hum:   reading.humidity,
    state: reading.state,
  });

  // Trim to 24hr window
  if (historyRows.length > MAX_HISTORY_ROWS) historyRows.pop();

  renderHistoryTable();
}

/**
 * Replaces table with full history array (initial load).
 * @param {Array} readings - array of normalised readings oldest→newest
 */
function populateHistoryTable(readings) {
  historyRows.length = 0;
  // Reverse so newest is first
  [...readings].reverse().forEach(r => {
    const date = getReadingDate(r.timestamp);
    historyRows.push({
      time:  formatDateTime(date),
      ppm:   r.ppm_compensated,
      temp:  r.temperature,
      hum:   r.humidity,
      state: r.state,
    });
  });
  renderHistoryTable();
}

function stateLabel(state) {
  const map = {
    safe:    '<span class="hist-badge hist-badge--safe">Safe</span>',
    warning: '<span class="hist-badge hist-badge--warning">Warning</span>',
    danger:  '<span class="hist-badge hist-badge--danger">Danger</span>',
  };
  return map[state] || state;
}

function renderHistoryTable() {
  const tbody = document.getElementById('historyTableBody');
  const empty = document.getElementById('historyEmpty');

  if (historyRows.length === 0) {
    tbody.innerHTML = '';
    if (empty) empty.style.display = 'block';
    return;
  }

  if (empty) empty.style.display = 'none';

  tbody.innerHTML = historyRows.map(r => `
    <tr>
      <td>${r.time}</td>
      <td><strong>${r.ppm}</strong> ppm</td>
      <td>${r.temp.toFixed(1)}°C</td>
      <td>${r.hum.toFixed(1)}%</td>
      <td>${stateLabel(r.state)}</td>
    </tr>
  `).join('');
}

// ── UI — live/history view switch ──────────────────────────────────────────────

function setupDashboardViews() {
  const liveView = document.getElementById('liveView');
  const historyView = document.getElementById('historyView');
  const showHistoryBtn = document.getElementById('showHistoryBtn');
  const showLiveBtn = document.getElementById('showLiveBtn');

  function showView(view) {
    const showingHistory = view === 'history';
    liveView.hidden = showingHistory;
    historyView.hidden = !showingHistory;

    if (showingHistory && historyChart) {
      historyChart.resize();
      historyChart.update('none');
    }

    if (!showingHistory && trendChart) {
      trendChart.resize();
      trendChart.update('none');
    }
  }

  showHistoryBtn.addEventListener('click', () => showView('history'));
  showLiveBtn.addEventListener('click', () => showView('live'));
  showView('live');
}

// ── Main update — called on every live reading ─────────────────────────────────

function onNewReading(data) {
  const time = formatTime(getReadingDate(data.timestamp));

  updateStatusBadge(data.state);
  updateStateBar(data.state);
  updateMetrics(data);
  updateDeviceStates(data);
  updateChart(data.ppm_compensated, time);  // charts.js

  if (data.state !== 'safe' && data.state !== prevState) {
    addAlertEntry(data.ppm_compensated, data.state);
  }

  prevState = data.state;
}

// ── Initialise ─────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  initChart();         // live chart — charts.js
  initHistoryChart();  // 24hr chart — charts.js
  setupDashboardViews();

  if (typeof listenToSensorData === 'function') {
    // Live readings → dashboard
    listenToSensorData(onNewReading);

    // 24hr history → chart + table (initial load)
    loadHistory(readings => {
      populateHistoryChart(readings);   // charts.js
      populateHistoryTable(readings);   // dashboard.js
    });

    // Stream new history entries in real time
    listenToHistory(reading => {
      addHistoryPoint(reading);  // charts.js
      addHistoryRow(reading);    // dashboard.js
    });

    return;
  }

  // ── Mock fallback ──────────────────────────────────────────────────────────
  onNewReading(getMockReading());
  setInterval(() => {
    const reading = getMockReading();
    onNewReading(reading);
  }, 2000);
});
