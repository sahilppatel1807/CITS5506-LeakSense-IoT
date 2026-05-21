/**
* charts.js
* Manages two Chart.js charts:
*  - trendChart  : live readings, last 60 points
*  - historyChart: 24-hour voltage + temperature + humidity
*/
 
const THRESHOLD_WARNING_V = 1.60;
const THRESHOLD_DANGER_V  = 1.90;
const THRESHOLD_EXTREME_V = 2.20;
const GAS_AXIS_MAX_V      = 3.00; // max voltage shown on y-axis, for better visual scaling
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
          label:            'Gas (V)',
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
          label:       'Warning (1.60 V)',
          data:        [],
          borderColor: 'rgba(217,119,6,0.6)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
        },
        {
          label:       'Danger (1.90 V)',
          data:        [],
          borderColor: 'rgba(185,28,28,0.6)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
        },
        {
          label:       'Extreme (2.20 V)',
          data:        [],
          borderColor: 'rgba(127,29,29,0.6)',
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
            label: ctx => ctx.datasetIndex === 0 ? `${Number(ctx.raw).toFixed(2)} V` : null,
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
          max:    GAS_AXIS_MAX_V,
          ticks:  { font: { size: 10 }, color: '#9ca3af', maxTicksLimit: 6 },
          grid:   { color: '#f3f4f6' },
          border: { display: false },
        },
      },
    },
  });
}
 
function updateChart(voltage, time) {
  const [gasData, warnData, dangerData, extremeData] = trendChart.data.datasets.map(d => d.data);
  const labels = trendChart.data.labels;
 
  gasData.push(voltage);
  warnData.push(THRESHOLD_WARNING_V);
  dangerData.push(THRESHOLD_DANGER_V);
  extremeData.push(THRESHOLD_EXTREME_V);
  labels.push(time);
 
  if (gasData.length > MAX_LIVE_POINTS) {
    gasData.shift(); warnData.shift(); dangerData.shift(); extremeData.shift(); labels.shift();
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
          label:            'Gas (V)',
          data:             [],
          borderColor:      '#3b82f6',
          backgroundColor:  'rgba(59,130,246,0.08)',
          borderWidth:      2,
          pointRadius:      2,
          pointHoverRadius: 5,
          tension:          0.35,
          fill:             true,
          yAxisID:          'yGas',
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
          label:       'Warning (1.60 V)',
          data:        [],
          borderColor: 'rgba(217,119,6,0.5)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
          yAxisID:     'yGas',
        },
        {
          label:       'Danger (1.90 V)',
          data:        [],
          borderColor: 'rgba(185,28,28,0.5)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
          yAxisID:     'yGas',
        },
        {
          label:       'Extreme (2.20 V)',
          data:        [],
          borderColor: 'rgba(127,29,29,0.5)',
          borderWidth: 1,
          borderDash:  [5,4],
          pointRadius: 0,
          fill:        false,
          yAxisID:     'yGas',
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
              if (ctx.datasetIndex === 0) return `Gas: ${Number(ctx.raw).toFixed(2)} V`;
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
        yGas: {
          type:     'linear',
          position: 'left',
          min:      0,
          max:      GAS_AXIS_MAX_V,
          title:    { display: true, text: 'Gas (V)', color: '#3b82f6', font: { size: 11 } },
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
 
  const [gasDs, tempDs, humDs, warnDs, dangerDs, extremeDs] = historyChart.data.datasets;
 
  gasDs.data.push(reading.voltage);
  tempDs.data.push(reading.temperature);
  humDs.data.push(reading.humidity);
  warnDs.data.push(THRESHOLD_WARNING_V);
  dangerDs.data.push(THRESHOLD_DANGER_V);
  extremeDs.data.push(THRESHOLD_EXTREME_V);
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
 
 