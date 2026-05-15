/**
 * charts.js
 * Handles the gas concentration trend chart using Chart.js.
 * dashboard.js calls updateChart(ppm) on every new reading.
 */

// Thresholds — must match dashboard.js and firmware/src/config.h
const THRESHOLD_WARNING = 300;
const THRESHOLD_DANGER  = 500;
const MAX_POINTS        = 30;   // number of readings shown on chart
const MIN_Y_MAX         = 750;
const PEAK_HEADROOM     = 1.25; // highest point sits at about 80% of chart height

let trendChart = null;

function getNiceAxisMax(value) {
  if (value <= MIN_Y_MAX) return MIN_Y_MAX;

  const magnitude = Math.pow(10, Math.floor(Math.log10(value)));
  const normalized = value / magnitude;
  let niceNormalized = 10;

  if (normalized <= 1) {
    niceNormalized = 1;
  } else if (normalized <= 2) {
    niceNormalized = 2;
  } else if (normalized <= 5) {
    niceNormalized = 5;
  }

  return niceNormalized * magnitude;
}

function updateYAxisScale(gasData) {
  const maxReading = gasData.reduce((max, value) => Math.max(max, Number(value) || 0), 0);
  const targetMax = Math.max(THRESHOLD_DANGER, maxReading) * PEAK_HEADROOM;
  trendChart.options.scales.y.max = getNiceAxisMax(targetMax);
}

/**
 * Initialises the Chart.js line chart on the canvas element.
 * Called once on page load from dashboard.js
 */
function initChart() {
  const ctx = document.getElementById('trendChart').getContext('2d');

  trendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels:   [],
      datasets: [
        {
          label:           'Gas (ppm)',
          data:            [],
          borderColor:     '#3b82f6',
          backgroundColor: 'rgba(59, 130, 246, 0.07)',
          borderWidth:     2,
          pointRadius:     0,
          tension:         0.4,
          fill:            true,
        },
        {
          // Warning threshold line
          label:       'Warning (300 ppm)',
          data:        [],
          borderColor: 'rgba(217, 119, 6, 0.6)',
          borderWidth: 1,
          borderDash:  [5, 4],
          pointRadius: 0,
          fill:        false,
        },
        {
          // Danger threshold line
          label:       'Danger (500 ppm)',
          data:        [],
          borderColor: 'rgba(185, 28, 28, 0.6)',
          borderWidth: 1,
          borderDash:  [5, 4],
          pointRadius: 0,
          fill:        false,
        },
      ],
    },
    options: {
      responsive:          true,
      maintainAspectRatio: false,
      devicePixelRatio:    Math.min(window.devicePixelRatio || 1, 2),
      animation:           { duration: 300 },
      interaction:         { mode: 'index', intersect: false },
      layout:              { padding: { top: 8, right: 8, bottom: 2, left: 0 } },
      plugins: {
        legend: { display: false },
        tooltip: {
          callbacks: {
            label: (ctx) => {
              if (ctx.datasetIndex === 0) return `${Math.round(ctx.raw)} ppm`;
              return null;
            },
          },
        },
      },
      scales: {
        x: {
          display: true,
          ticks:   { font: { size: 12, lineHeight: 1.25 }, color: '#9ca3af', maxTicksLimit: 6 },
          grid:    { display: false },
          border:  { display: false },
        },
        y: {
          min:   0,
          max:   MIN_Y_MAX,
          ticks: { font: { size: 12, lineHeight: 1.25 }, color: '#9ca3af', maxTicksLimit: 6 },
          grid:  { color: '#f3f4f6' },
          border: { display: false },
        },
      },
    },
  });
}

/**
 * Adds a new ppm reading to the chart and scrolls the window.
 * Called from dashboard.js on every sensor update.
 *
 * @param {number} ppm   - compensated gas reading
 * @param {string} time  - formatted time label e.g. "14:32:05"
 */
function updateChart(ppm, time) {
  const gasData     = trendChart.data.datasets[0].data;
  const warnData    = trendChart.data.datasets[1].data;
  const dangerData  = trendChart.data.datasets[2].data;
  const labels      = trendChart.data.labels;

  // Add new point
  gasData.push(ppm);
  warnData.push(THRESHOLD_WARNING);
  dangerData.push(THRESHOLD_DANGER);
  labels.push(time);

  // Keep only the last MAX_POINTS readings
  if (gasData.length > MAX_POINTS) {
    gasData.shift();
    warnData.shift();
    dangerData.shift();
    labels.shift();
  }

  updateYAxisScale(gasData);
  trendChart.update('none');  // 'none' skips animation for smooth live updates
}
