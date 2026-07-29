/**
 * Convergence Chart -- Renders mesh convergence data on HTML5 Canvas.
 * Log-log plot with FEA data, analytical reference, and Richardson extrapolation.
 *
 * Usage:
 *   const chart = new ConvergenceChart(container);
 *   chart.loadData('assets/data/convergence.json');
 *   chart.render();
 */

class ConvergenceChart {
  constructor(container, options = {}) {
    this.container = container;
    this.canvas = null;
    this.ctx = null;
    this.data = null;

    this.padding = options.padding || { top: 40, right: 30, bottom: 50, left: 70 };
    this.feaColor = options.feaColor || '#00d4ff';
    this.analyticalColor = options.analyticalColor || '#ff4757';
    this.richardsonColor = options.richardsonColor || '#39d353';

    this._init();
  }

  _init() {
    this.canvas = document.createElement('canvas');
    this.canvas.style.width = '100%';
    this.canvas.style.height = '100%';
    this.canvas.style.display = 'block';
    this.container.appendChild(this.canvas);

    this.ctx = this.canvas.getContext('2d');

    this._resizeObserver = new ResizeObserver(() => this._resize());
    this._resizeObserver.observe(this.container);
    this._resize();
  }

  _resize() {
    const rect = this.container.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    this.canvas.width = rect.width * dpr;
    this.canvas.height = rect.height * dpr;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.W = rect.width;
    this.H = rect.height;

    if (this.data) this.render();
  }

  async loadData(url) {
    const resp = await fetch(url);
    this.data = await resp.json();
    this.render();
  }

  setData(data) {
    this.data = data;
    this.render();
  }

  _log10(v) {
    return Math.log10(Math.abs(v));
  }

  _pow10(v) {
    return Math.pow(10, v);
  }

  render() {
    const ctx = this.ctx;
    const p = this.padding;

    ctx.clearRect(0, 0, this.W, this.H);

    if (!this.data || !this.data.samples || this.data.samples.length === 0) {
      ctx.fillStyle = '#8b949e';
      ctx.font = '12px "JetBrains Mono", monospace';
      ctx.textAlign = 'center';
      ctx.fillText('No convergence data', this.W / 2, this.H / 2);
      return;
    }

    const samples = this.data.samples;
    const hValues = samples.map(s => s.h);
    const yValues = samples.map(s => s.value);

    // Use absolute values for log scale
    const logH = hValues.map(h => this._log10(h));
    const logY = yValues.map(y => this._log10(Math.abs(y)));

    const hMin = Math.min(...logH);
    const hMax = Math.max(...logH);
    const yMin = Math.min(...logY);
    const yMax = Math.max(...logY);

    const hPad = (hMax - hMin) * 0.1 || 0.5;
    const yPad = (yMax - yMin) * 0.1 || 0.5;

    const plotW = this.W - p.left - p.right;
    const plotH = this.H - p.top - p.bottom;

    const toX = (lh) => p.left + ((lh - (hMin - hPad)) / ((hMax + hPad) - (hMin - hPad))) * plotW;
    const toY = (ly) => p.top + plotH - ((ly - (yMin - yPad)) / ((yMax + yPad) - (yMin - yPad))) * plotH;

    // Grid lines
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 0.5;

    for (let i = Math.ceil(hMin - hPad); i <= Math.floor(hMax + hPad); i++) {
      const x = toX(i);
      ctx.beginPath();
      ctx.moveTo(x, p.top);
      ctx.lineTo(x, p.top + plotH);
      ctx.stroke();
    }
    for (let i = Math.ceil(yMin - yPad); i <= Math.floor(yMax + yPad); i++) {
      const y = toY(i);
      ctx.beginPath();
      ctx.moveTo(p.left, y);
      ctx.lineTo(p.left + plotW, y);
      ctx.stroke();
    }

    // Analytical reference line
    if (this.data.analytical) {
      const logA = this._log10(Math.abs(this.data.analytical));
      ctx.strokeStyle = this.analyticalColor;
      ctx.lineWidth = 1.5;
      ctx.setLineDash([6, 4]);
      ctx.beginPath();
      ctx.moveTo(p.left, toY(logA));
      ctx.lineTo(p.left + plotW, toY(logA));
      ctx.stroke();
      ctx.setLineDash([]);

      // Label
      ctx.fillStyle = this.analyticalColor;
      ctx.font = '9px "JetBrains Mono", monospace';
      ctx.textAlign = 'left';
      ctx.fillText(`Analytical: ${this.data.analytical.toExponential(3)}`, p.left + 5, toY(logA) - 5);
    }

    // Richardson extrapolation line
    const gci = this.data.gci;
    if (gci && gci.extrapolated_value) {
      const logR = this._log10(Math.abs(gci.extrapolated_value));
      ctx.strokeStyle = this.richardsonColor;
      ctx.lineWidth = 1;
      ctx.setLineDash([3, 3]);
      ctx.beginPath();
      ctx.moveTo(p.left, toY(logR));
      ctx.lineTo(p.left + plotW, toY(logR));
      ctx.stroke();
      ctx.setLineDash([]);

      ctx.fillStyle = this.richardsonColor;
      ctx.font = '9px "JetBrains Mono", monospace';
      ctx.textAlign = 'left';
      ctx.fillText(`Richardson: ${gci.extrapolated_value.toExponential(3)}`, p.left + 5, toY(logR) + 12);
    }

    // FEA data line + points
    ctx.strokeStyle = this.feaColor;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let i = 0; i < logH.length; i++) {
      const x = toX(logH[i]);
      const y = toY(logY[i]);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Points
    for (let i = 0; i < logH.length; i++) {
      const x = toX(logH[i]);
      const y = toY(logY[i]);

      ctx.fillStyle = this.feaColor;
      ctx.beginPath();
      ctx.arc(x, y, 4, 0, Math.PI * 2);
      ctx.fill();

      ctx.strokeStyle = '#0d1117';
      ctx.lineWidth = 1.5;
      ctx.stroke();

      // Label with h value
      ctx.fillStyle = '#8b949e';
      ctx.font = '8px "JetBrains Mono", monospace';
      ctx.textAlign = 'center';
      ctx.fillText(`h=${hValues[i].toFixed(4)}`, x, y - 8);
    }

    // Axes
    ctx.strokeStyle = '#30363d';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(p.left, p.top);
    ctx.lineTo(p.left, p.top + plotH);
    ctx.lineTo(p.left + plotW, p.top + plotH);
    ctx.stroke();

    // Axis labels
    ctx.fillStyle = '#c9d1d9';
    ctx.font = '11px "JetBrains Mono", monospace';
    ctx.textAlign = 'center';
    ctx.fillText('Element size h (log scale)', p.left + plotW / 2, this.H - 8);

    ctx.save();
    ctx.translate(15, p.top + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText(`${this.data.quantity || 'Value'} (log scale)`, 0, 0);
    ctx.restore();

    // Tick labels
    ctx.fillStyle = '#8b949e';
    ctx.font = '9px "JetBrains Mono", monospace';
    ctx.textAlign = 'center';
    for (let i = Math.ceil(hMin - hPad); i <= Math.floor(hMax + hPad); i++) {
      const x = toX(i);
      ctx.fillText(`10^${i}`, x, p.top + plotH + 18);
    }

    ctx.textAlign = 'right';
    for (let i = Math.ceil(yMin - yPad); i <= Math.floor(yMax + yPad); i++) {
      const y = toY(i);
      ctx.fillText(`10^${i}`, p.left - 8, y + 3);
    }

    // Title
    ctx.fillStyle = '#ffb347';
    ctx.font = 'bold 12px "JetBrains Mono", monospace';
    ctx.textAlign = 'center';
    let title = `${this.data.case || 'Case'} Mesh Convergence`;
    if (gci && gci.observed_order) title += ` (p=${gci.observed_order.toFixed(2)})`;
    ctx.fillText(title, this.W / 2, 20);

    // GCI info box
    if (gci) {
      const boxX = p.left + plotW - 180;
      const boxY = p.top + 5;
      ctx.fillStyle = 'rgba(13,17,23,0.85)';
      ctx.strokeStyle = 'rgba(255,255,255,0.15)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.roundRect(boxX, boxY, 175, 55, 4);
      ctx.fill();
      ctx.stroke();

      ctx.fillStyle = '#8b949e';
      ctx.font = '9px "JetBrains Mono", monospace';
      ctx.textAlign = 'left';
      if (gci.observed_order) ctx.fillText(`Order: ${gci.observed_order.toFixed(3)}`, boxX + 6, boxY + 15);
      if (gci.gci_fine) ctx.fillText(`GCI fine: ${gci.gci_fine.toExponential(3)}`, boxX + 6, boxY + 30);
      if (gci.is_oscillatory !== undefined) ctx.fillText(`Oscillatory: ${gci.is_oscillatory}`, boxX + 6, boxY + 45);
    }
  }
}
