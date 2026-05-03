/**
 * dashboard.js
 * Main dashboard logic — reads from mock.js (now) or firebase.js (later),
 * updates all UI elements, manages alert log and state machine.
 *
 * To switch to real Firebase data:
 * 1. Add firebase.js script tag in index.html
 * 2. Comment out the mock interval at the bottom of this file
 * 3. Uncomment the Firebase listener section
 */

// ── Thresholds — must match firmware/src/config.h ─────────────────────────────
const THRESHOLDS = {
  warning: 300,
  danger:  500,
};

// ── State ──────────────────────────────────────────────────────────────────────
let alertCount    = 0;
let prevState     = 'safe';
const alertLog    = [];   // stores alert entries for the log panel

// ── State helpers ──────────────────────────────────────────────────────────────

/**
 * Derives system state from ppm value.
 * Exposed globally so mock.js can use it too.
 * @param {number} ppm
 * @returns {'safe'|'warning'|'danger'}
 */
function getStateFromPpm(ppm) {
  if (ppm >= THRESHOLDS.danger)  return 'danger';
  if (ppm >= THRESHOLDS.warning) return 'warning';
  return 'safe';
}

function formatTime(date = new Date()) {
  return date.toLocaleTimeString('en-AU', {
    hour:   '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  });
}

// ── UI update functions ────────────────────────────────────────────────────────

const STATE_MESSAGES = {
  safe:    'System operating normally. Gas levels within safe range.',
  warning: 'Warning: elevated gas concentration detected. Fan and buzzer activated.',
  danger:  'DANGER: critical gas level detected. Immediate action required. All alarms active.',
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
  document.getElementById('lastSync').textContent = `Last sync: ${formatTime()}`;
}

function updateDeviceStates(data) {
  // Fan
  const fan = document.getElementById('fanState');
  fan.textContent = data.fan ? 'on' : 'off';
  fan.className   = `pill${data.fan ? ' pill--on' : ''}`;

  // Buzzer
  const buz = document.getElementById('buzzerState');
  buz.textContent = data.buzzer ? 'on' : 'off';
  buz.className   = `pill${data.buzzer ? ' pill--on' : ''}`;

  // Green LED — on when safe
  const green = document.getElementById('greenLed');
  green.textContent = data.state === 'safe' ? 'on' : 'off';
  green.className   = `pill${data.state === 'safe' ? ' pill--on' : ''}`;

  // Red LED — on when warning or danger
  const red = document.getElementById('redLed');
  red.textContent = data.state !== 'safe' ? 'on' : 'off';
  red.className   = `pill${data.state !== 'safe' ? ' pill--on' : ''}`;
}

function addAlertEntry(ppm, state) {
  const time = formatTime();
  const msg  = state === 'danger'
    ? `Gas level reached ${ppm} ppm — DANGER threshold crossed`
    : `Gas level reached ${ppm} ppm — Warning threshold crossed`;

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
      <span class="alert-pill alert-pill--${a.state}">
        ${a.state.toUpperCase()}
      </span>
    </div>
  `).join('');
}

// ── Main update — called on every new reading ──────────────────────────────────
/**
 * Takes a sensor reading object and updates the entire dashboard.
 * This function is called by both mock mode and Firebase mode —
 * the shape of `data` is identical in both cases.
 *
 * @param {{
 *   ppm_compensated: number,
 *   temperature: number,
 *   humidity: number,
 *   state: string,
 *   fan: boolean,
 *   buzzer: boolean
 * }} data
 */
function onNewReading(data) {
  const time = formatTime();

  updateStatusBadge(data.state);
  updateStateBar(data.state);
  updateMetrics(data);
  updateDeviceStates(data);
  updateChart(data.ppm_compensated, time);  // from charts.js

  // Log alert when state escalates
  if (data.state !== 'safe' && data.state !== prevState) {
    addAlertEntry(data.ppm_compensated, data.state);
  }

  prevState = data.state;
}

// ── Simulator slider ───────────────────────────────────────────────────────────
document.getElementById('simSlider').addEventListener('input', function () {
  const val = parseInt(this.value);
  document.getElementById('simVal').textContent = `${val} ppm`;
  setMockBasePpm(val);  // from mock.js
});

// ── Initialise ─────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  initChart();  // from charts.js

  // ── MOCK MODE (active now) ─────────────────────────────────────────────────
  // Comment out this block and uncomment Firebase section below when ready
  const mockInterval = setInterval(() => {
    const reading = getMockReading();  // from mock.js
    onNewReading(reading);
  }, 2000);

  // Run once immediately so dashboard isn't blank on load
  onNewReading(getMockReading());


  // ── FIREBASE MODE (uncomment when Sohaib has Firebase ready) ──────────────
  // Delete the mock interval above and uncomment this block.
  // Also uncomment the firebase.js script tags in index.html.
  //
  // import { listenToSensorData } from './firebase.js';
  // listenToSensorData(onNewReading);
});