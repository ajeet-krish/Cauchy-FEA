/**
 * Colorbar component for FEA Viewer.
 * Displays a horizontal colorbar at the bottom of the viewer with min/max labels.
 *
 * Usage:
 *   const colorbar = new FEAColorbar(container);
 *   colorbar.create();
 *   colorbar.update(0, 1e6, 'Von Mises Stress (Pa)');
 */

class FEAColorbar {
  constructor(container) {
    this.container = container;
    this.colorbar = null;
    this.min = 0;
    this.max = 1;
    this.title = 'Von Mises Stress (Pa)';
  }
  
  create() {
    this.colorbar = document.createElement('div');
    this.colorbar.id = 'fea-colorbar';
    this.colorbar.className = 'fea-colorbar';
    
    // Create gradient canvas
    const canvas = document.createElement('canvas');
    canvas.width = 400;
    canvas.height = 30;
    const ctx = canvas.getContext('2d');
    
    // Draw gradient
    const gradient = ctx.createLinearGradient(0, 0, canvas.width, 0);
    const colormap = Colormaps.turbo;
    
    for (let i = 0; i <= 10; i++) {
      const t = i / 10;
      const color = colormap(t);
      gradient.addColorStop(t, `rgb(${Math.round(color.r * 255)}, ${Math.round(color.g * 255)}, ${Math.round(color.b * 255)})`);
    }
    
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    // Create labels
    const minLabel = document.createElement('span');
    minLabel.className = 'colorbar-min';
    minLabel.textContent = this._formatValue(this.min);
    
    const maxLabel = document.createElement('span');
    maxLabel.className = 'colorbar-max';
    maxLabel.textContent = this._formatValue(this.max);
    
    const titleLabel = document.createElement('span');
    titleLabel.className = 'colorbar-title';
    titleLabel.textContent = this.title;
    
    // Assemble
    this.colorbar.appendChild(minLabel);
    this.colorbar.appendChild(canvas);
    this.colorbar.appendChild(maxLabel);
    this.colorbar.appendChild(titleLabel);
    
    this.container.appendChild(this.colorbar);
    
    // Add CSS
    this._addStyles();
  }
  
  update(min, max, title) {
    this.min = min;
    this.max = max;
    if (title) this.title = title;
    
    const minLabel = this.colorbar.querySelector('.colorbar-min');
    const maxLabel = this.colorbar.querySelector('.colorbar-max');
    const titleLabel = this.colorbar.querySelector('.colorbar-title');
    
    if (minLabel) minLabel.textContent = this._formatValue(min);
    if (maxLabel) maxLabel.textContent = this._formatValue(max);
    if (titleLabel) titleLabel.textContent = this.title;
  }
  
  _formatValue(value) {
    if (Math.abs(value) < 1e-10) return '0';
    if (Math.abs(value) >= 1e6) return `${(value / 1e6).toFixed(2)}e6`;
    if (Math.abs(value) >= 1e3) return `${(value / 1e3).toFixed(2)}e3`;
    if (Math.abs(value) < 1e-3) return `${(value * 1e6).toFixed(2)}e-6`;
    return value.toFixed(4);
  }
  
  _addStyles() {
    const style = document.createElement('style');
    style.textContent = `
      .fea-colorbar {
        position: absolute;
        bottom: 20px;
        left: 50%;
        transform: translateX(-50%);
        background: rgba(0, 0, 0, 0.8);
        padding: 10px 20px;
        border-radius: 4px;
        display: flex;
        align-items: center;
        gap: 10px;
        z-index: 100;
      }
      
      .colorbar-min, .colorbar-max {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
      }
      
      .colorbar-title {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
        margin-left: 10px;
      }
    `;
    document.head.appendChild(style);
  }
}
