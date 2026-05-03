/**
 * mock.js
 * Simulates ESP32 sensor data for development.
 * Remove this file (and the script tag in index.html) when Firebase is connected.
 * Replace with firebase.js instead.
 */

// Controlled by the simulator slider in the dashboard
let mockBasePpm = 120;

/**
 * Adds realistic noise to a base value — mimics real sensor jitter
 * @param {number} base
 * @returns {number}
 */
function addNoise(base) {
  return Math.max(0, Math.round(base + (Math.random() - 0.5) * 20));
}

/**
 * Returns a simulated sensor reading matching the exact
 * same structure the ESP32 will push to Firebase.
 * Field names must match firebase.js and the ESP32 firmware.
 *
 * @returns {{
 *   ppm_compensated: number,
 *   ppm_raw: number,
 *   temperature: number,
 *   humidity: number,
 *   state: string,
 *   fan: boolean,
 *   buzzer: boolean,
 *   timestamp: number
 * }}
 */
function getMockReading() {
  const ppm   = addNoise(mockBasePpm);
  const state = getStateFromPpm(ppm);

  return {
    ppm_compensated: ppm,
    ppm_raw:         Math.round(ppm * 1.08),  // raw is slightly higher before compensation
    temperature:     parseFloat((22 + Math.random() * 2).toFixed(1)),
    humidity:        parseFloat((45 + Math.random() * 5).toFixed(1)),
    state:           state,
    fan:             state !== 'safe',
    buzzer:          state !== 'safe',
    timestamp:       Date.now(),
  };
}

/**
 * Allows dashboard.js to update the mock base ppm via the slider
 * @param {number} val
 */
function setMockBasePpm(val) {
  mockBasePpm = val;
}