/**
* dashboard.js
* Main dashboard logic.
* Handles live readings, alert log, 24hr history table, and state machine.
*/
 
// ── Dashboard display thresholds ──────────────────────────────────────────────
const VOLTAGE_THRESHOLDS = { warning: 1.60, danger: 1.90, extreme: 2.20 };
const PPM_THRESHOLDS = { warning: 300, danger: 500, extreme: 800 };
 
// ── State ──────────────────────────────────────────────────────────────────────
let alertCount = 0;
let prevState  = 'safe';
const alertLog = [];
 
// 24-hour history table data
const historyRows = [];
const MAX_HISTORY_ROWS = 288; // 24hr at 1 reading per 5 min
 
// ── Helpers ────────────────────────────────────────────────────────────────────
 
function getStateFromPpm(ppm) {
  return getStateFromVoltage(gasVoltageFromPpm(ppm));
}
 
function getStateFromVoltage(voltage) {
  const value = Number(voltage);
  if (!Number.isFinite(value)) return 'safe';
  if (value >= VOLTAGE_THRESHOLDS.extreme) return 'extreme';
  if (value >= VOLTAGE_THRESHOLDS.danger)  return 'danger';
  if (value >= VOLTAGE_THRESHOLDS.warning) return 'warning';
  return 'safe';
}
 
function gasVoltageFromPpm(ppm) {
  const value = Number(ppm);
  if (!Number.isFinite(value) || value <= 0) return 0;
 
  if (value < PPM_THRESHOLDS.warning) {
    return (value / PPM_THRESHOLDS.warning) * VOLTAGE_THRESHOLDS.warning;
  }
 
  if (value < PPM_THRESHOLDS.danger) {
    const span = PPM_THRESHOLDS.danger - PPM_THRESHOLDS.warning;
    const pct = (value - PPM_THRESHOLDS.warning) / span;
    return VOLTAGE_THRESHOLDS.warning + pct * (VOLTAGE_THRESHOLDS.danger - VOLTAGE_THRESHOLDS.warning);
  }
 
  const span = PPM_THRESHOLDS.extreme - PPM_THRESHOLDS.danger;
  const pct = Math.min((value - PPM_THRESHOLDS.danger) / span, 1);
  return VOLTAGE_THRESHOLDS.danger + pct * (VOLTAGE_THRESHOLDS.extreme - VOLTAGE_THRESHOLDS.danger);
}
 
function formatVoltage(voltage) {
  const value = Number(voltage);
  return Number.isFinite(value) ? `${value.toFixed(2)} V` : '— V';
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
  extreme: 'EXTREME: gas voltage is critically high. Evacuate and ventilate immediately.',
};
 
const VOLTAGE_SUBS = {
  safe:    'Safe: < 1.60 V',
  warning: 'Warning: >= 1.60 V',
  danger:  'Danger: >= 1.90 V',
  extreme: 'Extreme: >= 2.20 V',
};

const NOTIFICATION_TITLES = {
  warning: 'LeakSense warning',
  danger:  'LeakSense danger alert',
  extreme: 'LeakSense extreme alert',
};

let notificationRegistrationPromise = null;

function getNotificationRegistration() {
  if (!('serviceWorker' in navigator)) return Promise.resolve(null);

  if (!notificationRegistrationPromise) {
    notificationRegistrationPromise = navigator.serviceWorker
      .register('service-worker.js')
      .then(() => navigator.serviceWorker.ready);
  }

  return notificationRegistrationPromise;
}

function setupAlertNotifications() {
  const btn = document.getElementById('enableNotificationsBtn');
  if (!btn || !('Notification' in window)) return;

  function syncButton() {
    if (Notification.permission === 'granted') {
      btn.hidden = false;
      btn.disabled = true;
      btn.textContent = 'Notifications on';
      return;
    }

    btn.hidden = false;
    btn.disabled = Notification.permission === 'denied';
    btn.textContent = Notification.permission === 'denied'
      ? 'Notifications blocked'
      : 'Enable notifications';
  }

  btn.addEventListener('click', async () => {
    try {
      const permission = await Notification.requestPermission();
      syncButton();

      if (permission === 'granted') {
        const registration = await getNotificationRegistration();
        await showLeakSenseNotification('LeakSense notifications enabled', {
          body: 'You will be notified when gas reaches warning, danger, or extreme levels.',
          tag: 'leaksense-notifications-enabled',
        }, registration);
      }
    } catch (err) {
      console.warn('Could not enable notifications:', err);
    }
  });

  getNotificationRegistration().catch(err => {
    console.warn('Could not register notification service worker:', err);
  });
  syncButton();
}

async function showLeakSenseNotification(title, options, registration = null) {
  const serviceWorkerRegistration = registration || await getNotificationRegistration();

  if (serviceWorkerRegistration && serviceWorkerRegistration.showNotification) {
    return serviceWorkerRegistration.showNotification(title, options);
  }

  return new Notification(title, options);
}

function sendAlertNotification(voltage, state) {
  if (!('Notification' in window) || Notification.permission !== 'granted') return;
 
  const title = NOTIFICATION_TITLES[state] || 'LeakSense alert';
  const body = `${formatVoltage(voltage)} detected. ${STATE_MESSAGES[state]}`;

  showLeakSenseNotification(title, {
      body,
      tag: `leaksense-${state}`,
      renotify: true,
      requireInteraction: state === 'danger' || state === 'extreme',
    }).catch(err => {
    console.warn('Could not show notification:', err);
  });
}
 
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
  document.getElementById('ppmVal').textContent  = formatVoltage(data.voltage);
  document.getElementById('ppmSub').textContent  = VOLTAGE_SUBS[data.state];
  document.getElementById('tempVal').textContent = `${data.temperature}°C`;
  document.getElementById('humVal').textContent  = `${data.humidity}%`;
  document.getElementById('lastSync').textContent = `Last sync: ${formatTime(getReadingDate(data.timestamp))}`;
}
 
// ── UI — device states with yellow LED ────────────────────────────────────────
 
function updateDeviceStates(data) {
  // Fan — on in Danger/Extreme by default from Firebase normalization
  const fan = document.getElementById('fanState');
  fan.textContent = data.fan ? 'on' : 'off';
  fan.className   = `pill${data.fan ? ' pill--on' : ''}`;
 
  // Buzzer — on in Danger/Extreme by default from Firebase normalization
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
 
  // Red LED — Danger and Extreme
  const red = document.getElementById('redLed');
  const redActive = data.state === 'danger' || data.state === 'extreme';
  red.textContent = redActive ? 'on' : 'off';
  red.className   = `pill${redActive ? ` pill--${data.state}` : ''}`;
}
 
// ── UI — alert log ─────────────────────────────────────────────────────────────
 
function addAlertEntry(voltage, state) {
  const time = formatTime();
  const formattedVoltage = formatVoltage(voltage);
  const labels = {
    warning: 'Warning threshold crossed',
    danger:  'DANGER threshold crossed',
    extreme: 'EXTREME threshold crossed',
  };
  const msg = `Gas reached ${formattedVoltage} — ${labels[state] || 'Alert threshold crossed'}`;
 
  alertLog.unshift({ time, voltage, state, msg });
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
    voltage: reading.voltage,
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
      voltage: r.voltage,
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
    extreme: '<span class="hist-badge hist-badge--extreme">Extreme</span>',
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
      <td><strong>${formatVoltage(r.voltage)}</strong></td>
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
  const voltage = Number(data.voltage);
  if (!Number.isFinite(voltage)) {
    data.voltage = gasVoltageFromPpm(data.ppm_compensated);
  }
  data.state = getStateFromVoltage(data.voltage);
 
  updateStatusBadge(data.state);
  updateStateBar(data.state);
  updateMetrics(data);
  updateDeviceStates(data);
  updateChart(data.voltage, time);  // charts.js
 
  if (data.state !== 'safe' && data.state !== prevState) {
    addAlertEntry(data.voltage, data.state);
    sendAlertNotification(data.voltage, data.state);
  }
 
  prevState = data.state;
}
 
// ── Initialise ─────────────────────────────────────────────────────────────────
 
document.addEventListener('DOMContentLoaded', () => {
  initChart();         // live chart — charts.js
  initHistoryChart();  // 24hr chart — charts.js
  setupDashboardViews();
  setupAlertNotifications();
 
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
 
 
