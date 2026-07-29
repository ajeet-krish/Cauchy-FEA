/**
 * Vertical colorbar component for FEA Viewer.
 * Displays a vertical colorbar on the right side of the viewer with min/max labels.
 *
 * Usage:
 *   const colorbar = new FEAColorbar(container);
 *   colorbar.create();
 *   colorbar.update(0, 1e6, 'Von Mises Stress (Pa)');
 *   colorbar.updateGradient(Colormaps.hot);
 */

class FEAColorbar {
  constructor(container) {
    this.container = container;
    this.colorbar = null;
    this.canvas = null;
    this.min = 0;
    this.max = 1;
    this.title = 'Von Mises Stress (Pa)';
    this.currentColormap = Colormaps.hot;
  }
  
  create() {
    this.colorbar = document.createElement('div');
    this.colorbar.id = 'fea-colorbar';
    this.colorbar.className = 'fea-colorbar';
    
    // Create vertical gradient canvas
    this.canvas = document.createElement('canvas');
    this.canvas.width = 30;
    this.canvas.height = 400;
    this._drawGradient(this.currentColormap);
    
    // Create labels
    const maxLabel = document.createElement('span');
    maxLabel.className = 'colorbar-max';
    maxLabel.textContent = this._formatValue(this.max);
    
    const minLabel = document.createElement('span');
    minLabel.className = 'colorbar-min';
    minLabel.textContent = this._formatValue(this.min);
    
    const titleLabel = document.createElement('span');
    titleLabel.className = 'colorbar-title';
    titleLabel.textContent = this.title;
    
    // Assemble (vertical layout: title, max, canvas, min)
    this.colorbar.appendChild(titleLabel);
    this.colorbar.appendChild(maxLabel);
    this.colorbar.appendChild(this.canvas);
    this.colorbar.appendChild(minLabel);
    
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
  
  updateGradient(colormap) {
    this.currentColormap = colormap;
    this._drawGradient(colormap);
  }
  
  _drawGradient(colormap) {
    if (!this.canvas) return;
    const ctx = this.canvas.getContext('2d');
    const width = this.canvas.width;
    const height = this.canvas.height;
    
    // Clear canvas
    ctx.clearRect(0, 0, width, height);
    
    // Draw vertical gradient (top = max, bottom = min)
    for (let y = 0; y < height; y++) {
      const t = 1 - (y / height); // Invert so top is max
      const color = colormap(t);
      ctx.fillStyle = `rgb(${Math.round(color.r * 255)}, ${Math.round(color.g * 255)}, ${Math.round(color.b * 255)})`;
      ctx.fillRect(0, y, width, 1);
    }
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
        right: 20px;
        top: 50%;
        transform: translateY(-50%);
        background: rgba(0, 0, 0, 0.8);
        padding: 10px;
        border-radius: 4px;
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 5px;
        z-index: 100;
      }
      
      .fea-colorbar canvas {
        border: 1px solid rgba(255, 255, 255, 0.3);
      }
      
      .colorbar-min, .colorbar-max {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 11px;
      }
      
      .colorbar-title {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 11px;
        writing-mode: vertical-rl;
        text-orientation: mixed;
        transform: rotate(180deg);
        margin-bottom: 5px;
      }
    `;
    document.head.appendChild(style);
  }
}
