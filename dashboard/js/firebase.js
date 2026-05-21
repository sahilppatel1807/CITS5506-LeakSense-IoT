/**
 * firebase.js
 * Firebase Realtime Database connection.
 * Loads recent readings for the graph, then listens to leaksense/latest.
 */

// ── Firebase config — paste from Firebase Console ─────────────────────────────
// Firebase Console → Project Settings → General → Your apps → SDK setup
// const firebaseConfig = {
//   apiKey:            'YOUR_API_KEY',
//   authDomain:        'YOUR_PROJECT.firebaseapp.com',
//   databaseURL:       'https://YOUR_PROJECT-default-rtdb.firebaseio.com',
//   projectId:         'YOUR_PROJECT',
//   storageBucket:     'YOUR_PROJECT.appspot.com',
//   messagingSenderId: 'YOUR_SENDER_ID',
//   appId:             'YOUR_APP_ID',
// };

  const firebaseConfig = {
    apiKey: "AIzaSyCbKMj2SODj0h8ywLfhknlg5nFAK1MGyEw",
    authDomain: "leaksense-iot.firebaseapp.com",
    databaseURL: "https://leaksense-iot-default-rtdb.firebaseio.com",
    projectId: "leaksense-iot",
    storageBucket: "leaksense-iot.firebasestorage.app",
    messagingSenderId: "634659563301",
    appId: "1:634659563301:web:2652686e881e94a47b5ccc"
  };


// Initialise Firebase
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

const HISTORY_PATHS = [
  'leaksense/readings',
  'leaksense/history',
  'readings',
  'history',
];

function numberFrom(...values) {
  const found = values.find((value) => value !== undefined && value !== null && value !== '');
  const number = Number(found);
  return Number.isFinite(number) ? number : 0;
}

function booleanFrom(value) {
  if (typeof value === 'boolean') return value;
  if (typeof value === 'string') return value.toLowerCase() === 'true' || value.toLowerCase() === 'on';
  return Boolean(value);
}

function normaliseReading(data) {
  const ppm = numberFrom(
    data.ppm_compensated,
    data.ppm,
    data.gas_ppm,
    data.gasPpm,
    data.lpg_ppm,
    data.lpgPpm,
    data.gas,
    data.value
  );
  const state = String(data.state || getStateFromPpm(ppm)).toLowerCase();

  return {
    ppm_compensated: Math.round(ppm),
    ppm_raw: Math.round(numberFrom(data.ppm_raw, data.raw_ppm, ppm)),
    temperature: numberFrom(data.temperature, data.temp, data.temp_c),
    humidity: numberFrom(data.humidity, data.hum),
    state,
    fan: data.fan === undefined ? state !== 'safe' : booleanFrom(data.fan),
    buzzer: data.buzzer === undefined ? state !== 'safe' : booleanFrom(data.buzzer),
    timestamp: numberFrom(data.timestamp, data.time, Date.now()),
  };
}

function snapshotToReadings(snapshot) {
  const value = snapshot.val();
  if (!value) return [];

  if (typeof value === 'object' && !Array.isArray(value)) {
    return Object.values(value)
      .filter((item) => item && typeof item === 'object')
      .map(normaliseReading)
      .sort((a, b) => a.timestamp - b.timestamp);
  }

  return [];
}

function loadRecentReadings(callback) {
  const requests = HISTORY_PATHS.map((path) => (
    db.ref(path).orderByChild('timestamp').limitToLast(60).once('value')
  ));

  Promise.all(requests).then((snapshots) => {
    const readings = snapshots
      .flatMap(snapshotToReadings)
      .sort((a, b) => a.timestamp - b.timestamp)
      .slice(-60);

    if (readings.length === 0) return;
    readings.forEach(callback);
  }).catch((error) => {
    console.warn('Could not load Firebase reading history:', error);
  });
}

/**
 * Subscribes to live sensor data from Firebase.
 * Calls the provided callback every time ESP32 pushes a new reading.
 *
 * The data shape matches getMockReading() in mock.js exactly —
 * so onNewReading() in dashboard.js works with both without any changes.
 *
 * @param {function} callback - onNewReading from dashboard.js
 */
function listenToSensorData(callback) {
  loadRecentReadings(callback);

  const ref = db.ref('leaksense/latest');

  ref.on('value', (snapshot) => {
    const data = snapshot.val();
    if (!data) return;

    // Map Firebase fields to the same shape as getMockReading().
    callback(normaliseReading(data));
  });
}
