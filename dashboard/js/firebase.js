/**
* firebase.js
* Firebase Realtime Database connection.
* - leaksense/latest  → live reading (every 2s from ESP32)
* - leaksense/history → one entry every 5 minutes, kept for 24 hours
*/
const firebaseConfig = window.LEAKSENSE_FIREBASE_CONFIG;
const hasFirebaseConfig = firebaseConfig && Object.values(firebaseConfig).every(value =>
  typeof value === 'string' && value.trim() !== '' && !value.includes('YOUR_')
);

if (hasFirebaseConfig) {
  firebase.initializeApp(firebaseConfig);
} else {
  console.warn('Firebase config missing. Copy js/secrets.example.js to js/secrets.js and fill it in.');
}

const db = hasFirebaseConfig ? firebase.database() : null;
 
// ── Helpers ────────────────────────────────────────────────────────────────────
 
function numberFrom(...values) {
  const found = values.find(v => v !== undefined && v !== null && v !== '');
  const n = Number(found);
  return Number.isFinite(n) ? n : 0;
}
 
function booleanFrom(value) {
  if (typeof value === 'boolean') return value;
  if (typeof value === 'string')  return value.toLowerCase() === 'true' || value.toLowerCase() === 'on';
  return Boolean(value);
}
 
function normaliseReading(data) {
  const ppm   = numberFrom(data.ppm_compensated, data.ppm, data.gas_ppm, data.gas, data.value);
  const suppliedVoltage = numberFrom(data.voltage, data.gas_voltage, data.voltage_v, data.sensor_voltage);
  const voltage = suppliedVoltage > 0 ? suppliedVoltage : gasVoltageFromPpm(ppm);
  const state = mostSevereState(data.state, getStateFromVoltage(voltage));
  const alarmActive = state === 'danger' || state === 'extreme';
 
  return {
    ppm_compensated: Math.round(ppm),
    ppm_raw:         Math.round(numberFrom(data.ppm_raw, data.raw_ppm, ppm)),
    voltage,
    temperature:     numberFrom(data.temperature, data.temp),
    humidity:        numberFrom(data.humidity, data.hum),
    state,
    thermal_risk: data.thermal_risk === undefined ? booleanFrom(data.thermalRisk) : booleanFrom(data.thermal_risk),
    fan:    data.fan    === undefined ? alarmActive : booleanFrom(data.fan),
    buzzer: data.buzzer === undefined ? alarmActive : booleanFrom(data.buzzer),
    timestamp: numberFrom(data.timestamp, data.time, Date.now()),
  };
}
 
function snapshotToReadings(snapshot) {
  const value = snapshot.val();
  if (!value || typeof value !== 'object' || Array.isArray(value)) return [];
 
  return Object.values(value)
    .filter(item => item && typeof item === 'object')
    .map(normaliseReading)
    .sort((a, b) => a.timestamp - b.timestamp);
}
 
// ── 24-hour history ────────────────────────────────────────────────────────────
 
const TWENTY_FOUR_HOURS_MS = 24 * 60 * 60 * 1000;
 
/**
* Loads all history entries from the last 24 hours.
* Called once on page load to populate the history chart and table.
* @param {function} callback - receives array of normalised reading objects
*/
function loadHistory(callback) {
  if (!db) {
    callback([]);
    return;
  }

  const cutoff = Date.now() - TWENTY_FOUR_HOURS_MS;
 
  db.ref('leaksense/history')
    .orderByChild('timestamp')
    .startAt(cutoff)
    .once('value')
    .then(snapshot => {
      const readings = snapshotToReadings(snapshot);
      callback(readings);
    })
    .catch(err => {
      console.warn('Could not load 24hr history:', err);
      callback([]);
    });
}
 
/**
* Listens for new history entries in real time.
* Only fires for entries newer than page load time.
* @param {function} callback - receives a single normalised reading
*/
function listenToHistory(callback) {
  if (!db) return false;

  const since = Date.now() - TWENTY_FOUR_HOURS_MS;
 
  db.ref('leaksense/history')
    .orderByChild('timestamp')
    .startAt(since)
    .on('child_added', snapshot => {
      const data = snapshot.val();
      if (!data) return;
      callback(normaliseReading(data));
    });

  return true;
}
 
// ── Live latest reading ────────────────────────────────────────────────────────
 
/**
* Subscribes to leaksense/latest for real-time dashboard updates.
* @param {function} callback - onNewReading from dashboard.js
*/
function listenToSensorData(callback) {
  if (!db) return false;

  db.ref('leaksense/latest').on('value', snapshot => {
    const data = snapshot.val();
    if (!data) return;
    callback(normaliseReading(data));
  });

  return true;
}
