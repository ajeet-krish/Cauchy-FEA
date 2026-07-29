/**
 * UI Controls for FEA Viewer.
 * Creates toolbar with contour selector, scale slider, toggle buttons,
 * animation controls, and export functionality.
 *
 * Usage:
 *   const controls = new FEAControls(viewer);
 *   controls.createToolbar(document.getElementById('controls'));
 */

class FEAControls {
  constructor(viewer) {
    this.viewer = viewer;
    this.toolbar = null;
  }
  
  createToolbar(container) {
    this.toolbar = document.createElement('div');
    this.toolbar.className = 'viewer-toolbar';
    
    // Contour selector
    const contourGroup = this._createControlGroup('Contour:');
    const contourSelect = document.createElement('select');
    contourSelect.id = 'contourType';
    const options = [
      { value: 'von_mises', text: 'Von Mises' },
      { value: 'sigma_xx', text: 'Sigma XX' },
      { value: 'sigma_yy', text: 'Sigma YY' },
      { value: 'sigma_xy', text: 'Sigma XY' },
      { value: 'sigma_1', text: 'Sigma 1 (Principal)' },
      { value: 'sigma_2', text: 'Sigma 2 (Principal)' },
      { value: 'displacement', text: '|u|' }
    ];
    for (const opt of options) {
      const option = document.createElement('option');
      option.value = opt.value;
      option.textContent = opt.text;
      contourSelect.appendChild(option);
    }
    contourSelect.addEventListener('change', (e) => {
      this.viewer.setContourType(e.target.value);
    });
    contourGroup.appendChild(contourSelect);
    this.toolbar.appendChild(contourGroup);
    
    // Scale slider
    const scaleGroup = this._createControlGroup('Scale:');
    const scaleSlider = document.createElement('input');
    scaleSlider.type = 'range';
    scaleSlider.id = 'dispScale';
    scaleSlider.min = '1';
    scaleSlider.max = '10000';
    scaleSlider.value = String(this.viewer.dispScale);
    scaleSlider.addEventListener('input', (e) => {
      this.viewer.setDisplacementScale(parseInt(e.target.value));
    });
    scaleGroup.appendChild(scaleSlider);
    
    const scaleValue = document.createElement('span');
    scaleValue.id = 'scaleValue';
    scaleValue.textContent = `${this.viewer.dispScale}x`;
    scaleGroup.appendChild(scaleValue);
    this.toolbar.appendChild(scaleGroup);
    
    // Toggle buttons
    const toggleGroup = this._createControlGroup('');
    
    const toggleUndeformed = this._createButton('Undeformed', () => {
      this.viewer.toggleUndeformed();
      toggleUndeformed.classList.toggle('active');
    });
    toggleUndeformed.classList.add('active');
    toggleGroup.appendChild(toggleUndeformed);
    
    const toggleDeformed = this._createButton('Deformed', () => {
      this.viewer.toggleDeformed();
      toggleDeformed.classList.toggle('active');
    });
    toggleDeformed.classList.add('active');
    toggleGroup.appendChild(toggleDeformed);
    
    const toggleEdges = this._createButton('Edges', () => {
      this.viewer.toggleEdges();
      toggleEdges.classList.toggle('active');
    });
    toggleEdges.classList.add('active');
    toggleGroup.appendChild(toggleEdges);
    
    this.toolbar.appendChild(toggleGroup);
    
    // Advanced toggles
    const advancedGroup = this._createControlGroup('');
    
    const toggleArrows = this._createButton('Stress Arrows', () => {
      this.viewer.toggleArrows();
      toggleArrows.classList.toggle('active');
    });
    advancedGroup.appendChild(toggleArrows);
    
    const toggleBoundary = this._createButton('Boundary', () => {
      this.viewer.toggleBoundary();
      toggleBoundary.classList.toggle('active');
    });
    advancedGroup.appendChild(toggleBoundary);
    
    this.toolbar.appendChild(advancedGroup);
    
    // Animation controls
    const animGroup = this._createControlGroup('');
    
    const animateBtn = this._createButton('Animate (10s)', () => {
      this.viewer.playAnimation();
    });
    animGroup.appendChild(animateBtn);
    
    const progressBar = document.createElement('div');
    progressBar.className = 'progress-bar';
    const progressFill = document.createElement('div');
    progressFill.id = 'animationProgress';
    progressFill.className = 'progress-fill';
    progressBar.appendChild(progressFill);
    animGroup.appendChild(progressBar);
    
    this.toolbar.appendChild(animGroup);
    
    // Export button
    const exportGroup = this._createControlGroup('');
    const exportBtn = this._createButton('Export PNG', () => {
      this.viewer.exportPNG();
    });
    exportGroup.appendChild(exportBtn);
    this.toolbar.appendChild(exportGroup);
    
    container.appendChild(this.toolbar);
    
    // Add styles
    this._addStyles();
  }
  
  _createControlGroup(labelText) {
    const group = document.createElement('div');
    group.className = 'control-group';
    
    if (labelText) {
      const label = document.createElement('label');
      label.textContent = labelText;
      group.appendChild(label);
    }
    
    return group;
  }
  
  _createButton(text, onClick) {
    const button = document.createElement('button');
    button.textContent = text;
    button.addEventListener('click', onClick);
    return button;
  }
  
  _addStyles() {
    const style = document.createElement('style');
    style.textContent = `
      .viewer-toolbar {
        position: absolute;
        top: 10px;
        left: 10px;
        background: rgba(0, 0, 0, 0.8);
        padding: 10px;
        border-radius: 4px;
        display: flex;
        flex-wrap: wrap;
        gap: 10px;
        z-index: 100;
      }
      
      .control-group {
        display: flex;
        align-items: center;
        gap: 5px;
      }
      
      .control-group label {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
      }
      
      .control-group select {
        background: #2a2a3e;
        color: white;
        border: 1px solid #444;
        padding: 4px 8px;
        border-radius: 4px;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
      }
      
      .control-group input[type="range"] {
        width: 100px;
      }
      
      .control-group span {
        color: white;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
        min-width: 50px;
      }
      
      .control-group button {
        background: #2a2a3e;
        color: white;
        border: 1px solid #444;
        padding: 4px 8px;
        border-radius: 4px;
        cursor: pointer;
        font-family: 'JetBrains Mono', monospace;
        font-size: 12px;
      }
      
      .control-group button:hover {
        background: #3a3a4e;
      }
      
      .control-group button.active {
        background: #4a7a9a;
        border-color: #6a9aba;
      }
      
      .progress-bar {
        width: 100px;
        height: 6px;
        background: #2a2a3e;
        border-radius: 3px;
        overflow: hidden;
      }
      
      .progress-fill {
        width: 0%;
        height: 100%;
        background: #4a7a9a;
        transition: width 0.1s linear;
      }
    `;
    document.head.appendChild(style);
  }
}
