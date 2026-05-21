/**
 * charts.js
 * Manages two Chart.js charts:
 *  - trendChart  : live readings, last 60 points
 *  - historyChart: 24-hour ppm + temperature + humidity
 */

const THRESHOLD_WARNING = 300;
const THRESHOLD_DANGER  = 500;
const MAX_LIVE_POINTS   = 60;

let trendChart   = null;
let historyChart = null;

// ── Live trend chart ───────────────────────────────────────────────────────────

function initChart() {
  const ctx = document.getElementById('trendChart').getContext('2d');

  trendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels:   [],
      datasets: [
        {
          label:            'Gas (ppm)',
          data:             [],
          borderColor:      '#3b82f6',
          backgroundColor:  'rgba(59,130,246,0.07)',
          borderWidth:      2,
          pointRadius:      2,
          pointHoverRadius: 4,
          tension:          0.4,
          fill:             true,
        },
        {
          label:       'Warning (300 ppm)',
          data:        [],
          borderColor: 'rgba(217,119,6,0.6)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
        },
        {
          label:       'Danger (500 ppm)',
          data:        [],
          borderColor: 'rgba(185,28,28,0.6)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
        },
      ],
    },
    options: {
      responsive:          true,
      maintainAspectRatio: false,
      animation:           { duration: 300 },
      interaction:         { mode: 'index', intersect: false },
      plugins: {
        legend: { display: false },
        tooltip: {
          callbacks: {
            label: ctx => ctx.datasetIndex === 0 ? `${Math.round(ctx.raw)} ppm` : null,
          },
        },
      },
      scales: {
        x: {
          display: true,
          ticks:   { font: { size: 10 }, color: '#9ca3af', maxTicksLimit: 6 },
          grid:    { display: false },
          border:  { display: false },
        },
        y: {
          min:    0,
          max:    750,
          ticks:  { font: { size: 10 }, color: '#9ca3af', maxTicksLimit: 6 },
          grid:   { color: '#f3f4f6' },
          border: { display: false },
        },
      },
    },
  });
}

function updateChart(ppm, time) {
  const [gasData, warnData, dangerData] = trendChart.data.datasets.map(d => d.data);
  const labels = trendChart.data.labels;

  gasData.push(ppm);
  warnData.push(THRESHOLD_WARNING);
  dangerData.push(THRESHOLD_DANGER);
  labels.push(time);

  if (gasData.length > MAX_LIVE_POINTS) {
    gasData.shift(); warnData.shift(); dangerData.shift(); labels.shift();
  }

  trendChart.update('none');
}

function resetChart() {
  trendChart.data.labels = [];
  trendChart.data.datasets.forEach(d => { d.data = []; });
  trendChart.update('none');
}

// ── 24-hour history chart ──────────────────────────────────────────────────────

function initHistoryChart() {
  const ctx = document.getElementById('historyChart').getContext('2d');

  historyChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels:   [],
      datasets: [
        {
          label:            'Gas (ppm)',
          data:             [],
          borderColor:      '#3b82f6',
          backgroundColor:  'rgba(59,130,246,0.08)',
          borderWidth:      2,
          pointRadius:      2,
          pointHoverRadius: 5,
          tension:          0.35,
          fill:             true,
          yAxisID:          'yPpm',
        },
        {
          label:            'Temp (°C)',
          data:             [],
          borderColor:      '#f59e0b',
          backgroundColor:  'transparent',
          borderWidth:      1.5,
          pointRadius:      0,
          tension:          0.35,
          fill:             false,
          yAxisID:          'yEnv',
        },
        {
          label:            'Humidity (%)',
          data:             [],
          borderColor:      '#10b981',
          backgroundColor:  'transparent',
          borderWidth:      1.5,
          pointRadius:      0,
          tension:          0.35,
          fill:             false,
          yAxisID:          'yEnv',
        },
        {
          label:       'Warning (300 ppm)',
          data:        [],
          borderColor: 'rgba(217,119,6,0.5)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
          yAxisID:     'yPpm',
        },
        {
          label:       'Danger (500 ppm)',
          data:        [],
          borderColor: 'rgba(185,28,28,0.5)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
          yAxisID:     'yPpm',
        },
      ],
    },
    options: {
      responsive:          true,
      maintainAspectRatio: false,
      animation:           { duration: 200 },
      interaction:         { mode: 'index', intersect: false },
      plugins: {
        legend: {
          display: true,
          position: 'top',
          labels: { font: { size: 11 }, color: '#6b7280', boxWidth: 14, padding: 12 },
        },
        tooltip: {
          callbacks: {
            label: ctx => {
              if (ctx.datasetIndex === 0) return `Gas: ${Math.round(ctx.raw)} ppm`;
              if (ctx.datasetIndex === 1) return `Temp: ${ctx.raw.toFixed(1)}°C`;
              if (ctx.datasetIndex === 2) return `Humidity: ${ctx.raw.toFixed(1)}%`;
              return null;
            },
          },
        },
      },
      scales: {
        x: {
          display: true,
          ticks:   { font: { size: 10 }, color: '#9ca3af', maxTicksLimit: 8 },
          grid:    { display: false },
          border:  { display: false },
        },
        yPpm: {
          type:     'linear',
          position: 'left',
          min:      0,
          max:      750,
          title:    { display: true, text: 'Gas (ppm)', color: '#3b82f6', font: { size: 11 } },
          ticks:    { font: { size: 10 }, color: '#9ca3af' },
          grid:     { color: '#f3f4f6' },
          border:   { display: false },
        },
        yEnv: {
          type:     'linear',
          position: 'right',
          min:      0,
          max:      100,
          title:    { display: true, text: 'Temp °C / Humidity %', color: '#6b7280', font: { size: 11 } },
          ticks:    { font: { size: 10 }, color: '#9ca3af' },
          grid:     { drawOnChartArea: false },
          border:   { display: false },
        },
      },
    },
  });
}

/**
 * Adds a single history reading to the 24hr chart.
 * Called both when loading existing history and on new live entries.
 * @param {object} reading - normalised reading from firebase.js
 */
function addHistoryPoint(reading) {
  const date   = new Date(reading.timestamp);
  const label  = date.toLocaleTimeString('en-AU', { hour: '2-digit', minute: '2-digit', hour12: false });

  const [ppmDs, tempDs, humDs, warnDs, dangerDs] = historyChart.data.datasets;

  ppmDs.data.push(reading.ppm_compensated);
  tempDs.data.push(reading.temperature);
  humDs.data.push(reading.humidity);
  warnDs.data.push(THRESHOLD_WARNING);
  dangerDs.data.push(THRESHOLD_DANGER);
  historyChart.data.labels.push(label);

  historyChart.update('none');
}

/**
 * Replaces all history chart data at once (used on initial load).
 * @param {Array} readings - array of normalised reading objects
 */
function populateHistoryChart(readings) {
  historyChart.data.labels = [];
  historyChart.data.datasets.forEach(d => { d.data = []; });

  readings.forEach(r => addHistoryPoint(r));
}