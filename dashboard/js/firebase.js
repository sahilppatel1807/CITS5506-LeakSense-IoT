/**
 * firebase.js
 * Firebase Realtime Database connection.
 * Listens to leaksense/latest and calls onNewReading() on every update.
 *
 * HOW TO ACTIVATE:
 * 1. Sohaib pastes the Firebase config below
 * 2. Uncomment the Firebase script tags in index.html
 * 3. In dashboard.js, comment out the mock interval
 *    and uncomment the listenToSensorData() call
 */

// ── Firebase config — paste from Firebase Console ─────────────────────────────
// Firebase Console → Project Settings → General → Your apps → SDK setup
const firebaseConfig = {
  apiKey:            'YOUR_API_KEY',
  authDomain:        'YOUR_PROJECT.firebaseapp.com',
  databaseURL:       'https://YOUR_PROJECT-default-rtdb.firebaseio.com',
  projectId:         'YOUR_PROJECT',
  storageBucket:     'YOUR_PROJECT.appspot.com',
  messagingSenderId: 'YOUR_SENDER_ID',
  appId:             'YOUR_APP_ID',
};

// Initialise Firebase
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

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
  const ref = db.ref('leaksense/latest');

  ref.on('value', (snapshot) => {
    const data = snapshot.val();
    if (!data) return;

    // Map Firebase fields to the same shape as getMockReading()
    callback({
      ppm_compensated: Math.round(data.ppm_compensated ?? 0),
      ppm_raw:         Math.round(data.ppm_raw ?? 0),
      temperature:     data.temperature ?? 0,
      humidity:        data.humidity    ?? 0,
      state:           data.state       ?? 'safe',
      fan:             data.fan         ?? false,
      buzzer:          data.buzzer      ?? false,
      timestamp:       data.timestamp   ?? Date.now(),
    });
  });
}