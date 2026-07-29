/**
 * FEA Viewer -- Interactive 2D finite element mesh visualization using Three.js WebGL.
 * Renders deformed mesh with stress/displacement contours, principal stress vectors,
 * and boundary condition symbols.
 *
 * Usage:
 *   const viewer = new FEAViewer(document.getElementById('container'));
 *   viewer.loadData('assets/data/cantilever_32.json');
 */

class FEAViewer {
  constructor(container, options = {}) {
    this.container = container;
    
    // Three.js objects
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.controls = null;
    
    // Lighting
    this.ambientLight = null;
    this.directionalLight = null;
    
    // Mesh groups
    this.undeformedGroup = null;
    this.deformedGroup = null;
    this.edgesGroup = null;
    this.arrowsGroup = null;
    this.boundaryGroup = null;
    
    // Data
    this.data = null;
    this.numNodes = 0;
    this.numElements = 0;
    
    // State
    this.dispScale = 100;
    this.maxScale = 10000;
    this.contourType = 'von_mises';
    this.showUndeformed = true;
    this.showDeformed = true;
    this.showEdges = true;
    this.showArrows = false;
    this.showBoundary = false;
    
    // Animation
    this.animationProgress = 1.0;
    this.animationDuration = 10000; // 10 seconds
    this.animationStartTime = 0;
    this.isPlaying = false;
    
    // Bounds
    this.bounds = { xmin: 0, xmax: 1, ymin: 0, ymax: 1 };
    
    // Colorbar reference
    this.colorbar = null;
    
    // Options
    this.padding = options.padding || 50;
    
    this._init();
  }
  
  _init() {
    // Create scene
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1a1a2e);
    
    // Create camera
    const aspect = this.container.clientWidth / this.container.clientHeight;
    this.camera = new THREE.PerspectiveCamera(45, aspect, 0.1, 1000);
    this.camera.position.set(0.5, 0.5, 2.0);
    this.camera.lookAt(0.5, 0.5, 0);
    
    // Create renderer
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.container.appendChild(this.renderer.domElement);
    
    // Create controls
    this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.05;
    this.controls.screenSpacePanning = true;
    this.controls.minDistance = 0.1;
    this.controls.maxDistance = 100;
    
    // Create lighting
    this.ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    this.scene.add(this.ambientLight);
    
    this.directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
    this.directionalLight.position.set(1, 1, 1);
    this.scene.add(this.directionalLight);
    
    // Create mesh groups
    this.undeformedGroup = new THREE.Group();
    this.deformedGroup = new THREE.Group();
    this.edgesGroup = new THREE.Group();
    this.arrowsGroup = new THREE.Group();
    this.boundaryGroup = new THREE.Group();
    
    this.scene.add(this.undeformedGroup);
    this.scene.add(this.deformedGroup);
    this.scene.add(this.edgesGroup);
    this.scene.add(this.arrowsGroup);
    this.scene.add(this.boundaryGroup);
    
    // Resize observer
    this._resizeObserver = new ResizeObserver(() => this._resize());
    this._resizeObserver.observe(this.container);
    
    // Start render loop
    this._animate();
  }
  
  _resize() {
    const width = this.container.clientWidth;
    const height = this.container.clientHeight;
    
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }
  
  _animate() {
    requestAnimationFrame(() => this._animate());
    
    // Update animation
    if (this.isPlaying) {
      const elapsed = performance.now() - this.animationStartTime;
      this.animationProgress = Math.min(elapsed / this.animationDuration, 1.0);
      
      // Smooth ease-in-out cubic
      const t = this.animationProgress < 0.5
        ? 4 * this.animationProgress * this.animationProgress * this.animationProgress
        : 1 - Math.pow(-2 * this.animationProgress + 2, 3) / 2;
      
      this.setDisplacementScale(t * this.maxScale);
      
      // Update progress bar
      const progressBar = document.getElementById('animationProgress');
      if (progressBar) {
        progressBar.style.width = `${this.animationProgress * 100}%`;
      }
      
      if (this.animationProgress >= 1.0) {
        this.isPlaying = false;
      }
    }
    
    this.controls.update();
    this.renderer.render(this.scene, this.camera);
  }
  
  async loadData(url) {
    const resp = await fetch(url);
    this.data = await resp.json();
    this._parseData();
    this._fitView();
    this._buildMesh();
    
    // Hide static fallback image once WebGL loads successfully
    const fallback = this.container.parentElement.querySelector('.static-fallback');
    if (fallback) {
      fallback.style.display = 'none';
    }
  }
  
  setData(data) {
    this.data = data;
    this._parseData();
    this._fitView();
    this._buildMesh();
  }
  
  _parseData() {
    this.numNodes = this.data.nodes.length;
    this.numElements = this.data.elements.length;
    
    // Compute bounds
    let xmin = Infinity, xmax = -Infinity;
    let ymin = Infinity, ymax = -Infinity;
    
    for (const node of this.data.nodes) {
      if (node.x < xmin) xmin = node.x;
      if (node.x > xmax) xmax = node.x;
      if (node.y < ymin) ymin = node.y;
      if (node.y > ymax) ymax = node.y;
    }
    
    this.bounds = { xmin, xmax, ymin, ymax };
    
    // Compute default displacement scale
    if (this.data.displacement) {
      let maxDisp = 0;
      for (const d of this.data.displacement) {
        const mag = Math.sqrt(d.ux * d.ux + d.uy * d.uy);
        if (mag > maxDisp) maxDisp = mag;
      }
      if (maxDisp > 0) {
        const xRange = this.bounds.xmax - this.bounds.xmin;
        const yRange = this.bounds.ymax - this.bounds.ymin;
        this.dispScale = Math.round(0.1 * Math.max(xRange, yRange) / maxDisp);
      }
    }
  }
  
  _fitView() {
    if (!this.bounds) return;
    
    const xCenter = (this.bounds.xmin + this.bounds.xmax) / 2;
    const yCenter = (this.bounds.ymin + this.bounds.ymax) / 2;
    const xRange = this.bounds.xmax - this.bounds.xmin;
    const yRange = this.bounds.ymax - this.bounds.ymin;
    const maxRange = Math.max(xRange, yRange);
    
    this.camera.position.set(xCenter, yCenter, maxRange * 2);
    this.camera.lookAt(xCenter, yCenter, 0);
    this.controls.target.set(xCenter, yCenter, 0);
    this.controls.update();
  }
  
  _buildMesh() {
    // Clear existing meshes
    this._clearGroup(this.undeformedGroup);
    this._clearGroup(this.deformedGroup);
    this._clearGroup(this.edgesGroup);
    this._clearGroup(this.arrowsGroup);
    this._clearGroup(this.boundaryGroup);
    
    if (!this.data) return;
    
    // Build undeformed mesh
    this._buildUndeformedMesh();
    
    // Build deformed mesh
    this._buildDeformedMesh();
    
    // Build edges
    this._buildEdges();
    
    // Build principal stress arrows
    this._buildPrincipalStressArrows();
    
    // Build boundary conditions
    this._buildBoundaryConditions();
    
    // Apply visibility
    this.undeformedGroup.visible = this.showUndeformed;
    this.deformedGroup.visible = this.showDeformed;
    this.edgesGroup.visible = this.showEdges;
    this.arrowsGroup.visible = this.showArrows;
    this.boundaryGroup.visible = this.showBoundary;
    
    // Apply contour coloring
    this._applyContour();
  }
  
  _clearGroup(group) {
    while (group.children.length > 0) {
      const child = group.children[0];
      if (child.geometry) child.geometry.dispose();
      if (child.material) child.material.dispose();
      group.remove(child);
    }
  }
  
  _buildUndeformedMesh() {
    const geometry = new THREE.BufferGeometry();
    const vertices = new Float32Array(this.numNodes * 3);
    
    for (let i = 0; i < this.numNodes; i++) {
      vertices[i * 3] = this.data.nodes[i].x;
      vertices[i * 3 + 1] = this.data.nodes[i].y;
      vertices[i * 3 + 2] = 0;
    }
    
    geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
    
    const material = new THREE.MeshPhongMaterial({
      color: 0x4488aa,
      transparent: true,
      opacity: 0.3,
      side: THREE.DoubleSide
    });
    
    const mesh = new THREE.Mesh(geometry, material);
    this.undeformedGroup.add(mesh);
  }
  
  _buildDeformedMesh() {
    if (!this.data.displacement) return;
    
    const geometry = new THREE.BufferGeometry();
    const vertices = new Float32Array(this.numNodes * 3);
    
    for (let i = 0; i < this.numNodes; i++) {
      const node = this.data.nodes[i];
      const disp = this.data.displacement[i];
      
      vertices[i * 3] = node.x + disp.ux * this.dispScale;
      vertices[i * 3 + 1] = node.y + disp.uy * this.dispScale;
      vertices[i * 3 + 2] = 0;
    }
    
    geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
    
    const material = new THREE.MeshPhongMaterial({
      color: 0x66aacc,
      side: THREE.DoubleSide
    });
    
    const mesh = new THREE.Mesh(geometry, material);
    this.deformedGroup.add(mesh);
  }
  
  _buildEdges() {
    const material = new THREE.LineBasicMaterial({ color: 0xffffff, linewidth: 1 });
    
    for (const elem of this.data.elements) {
      const geometry = new THREE.BufferGeometry();
      const vertices = new Float32Array([
        this.data.nodes[elem.n0].x, this.data.nodes[elem.n0].y, 0,
        this.data.nodes[elem.n1].x, this.data.nodes[elem.n1].y, 0,
        this.data.nodes[elem.n2].x, this.data.nodes[elem.n2].y, 0,
        this.data.nodes[elem.n3].x, this.data.nodes[elem.n3].y, 0,
        this.data.nodes[elem.n0].x, this.data.nodes[elem.n0].y, 0
      ]);
      
      geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
      const line = new THREE.Line(geometry, material);
      this.edgesGroup.add(line);
    }
  }
  
  _buildPrincipalStressArrows() {
    if (!this.data.stress || !this.data.stress.sigma_1) return;
    
    const scale = 0.1;
    
    for (let i = 0; i < this.numElements; i++) {
      const elem = this.data.elements[i];
      
      // Compute centroid
      const cx = (this.data.nodes[elem.n0].x + this.data.nodes[elem.n1].x +
                  this.data.nodes[elem.n2].x + this.data.nodes[elem.n3].x) / 4;
      const cy = (this.data.nodes[elem.n0].y + this.data.nodes[elem.n1].y +
                  this.data.nodes[elem.n2].y + this.data.nodes[elem.n3].y) / 4;
      
      const sigma1 = this.data.stress.sigma_1[i];
      const sigma2 = this.data.stress.sigma_2[i];
      const theta1 = this.data.stress.theta1[i];
      const theta2 = this.data.stress.theta2[i];
      
      // Arrow for sigma_1
      if (Math.abs(sigma1) > 1e-6) {
        const dir = new THREE.Vector3(Math.cos(theta1), Math.sin(theta1), 0);
        const length = Math.abs(sigma1) * scale;
        const color = sigma1 > 0 ? 0xff0000 : 0x0000ff; // Red=tension, Blue=compression
        const origin = new THREE.Vector3(cx, cy, 0);
        const arrow = new THREE.ArrowHelper(dir, origin, length, color, 0.05, 0.03);
        this.arrowsGroup.add(arrow);
      }
      
      // Arrow for sigma_2
      if (Math.abs(sigma2) > 1e-6) {
        const dir = new THREE.Vector3(Math.cos(theta2), Math.sin(theta2), 0);
        const length = Math.abs(sigma2) * scale;
        const color = sigma2 > 0 ? 0xff6666 : 0x6666ff;
        const origin = new THREE.Vector3(cx, cy, 0);
        const arrow = new THREE.ArrowHelper(dir, origin, length, color, 0.05, 0.03);
        this.arrowsGroup.add(arrow);
      }
    }
  }
  
  _buildBoundaryConditions() {
    if (!this.data.boundary) return;
    
    // Fixed supports (triangles)
    if (this.data.boundary.dirichlet) {
      const fixedNodes = new Set();
      for (const bc of this.data.boundary.dirichlet) {
        fixedNodes.add(bc.node);
      }
      
      for (const nodeIdx of fixedNodes) {
        const node = this.data.nodes[nodeIdx];
        const triangle = this._createTriangle(node.x, node.y, 0.08);
        this.boundaryGroup.add(triangle);
      }
    }
    
    // Forces (arrows)
    if (this.data.boundary.neumann) {
      for (const load of this.data.boundary.neumann) {
        const node = this.data.nodes[load.node];
        const dir = new THREE.Vector3(
          load.dof === 0 ? 1 : 0,
          load.dof === 1 ? 1 : 0,
          0
        );
        const length = Math.abs(load.value) * 0.0002;
        const color = load.value > 0 ? 0x00ff00 : 0xff0000;
        const origin = new THREE.Vector3(node.x, node.y, 0);
        const arrow = new THREE.ArrowHelper(dir, origin, length, color, 0.08, 0.05);
        this.boundaryGroup.add(arrow);
      }
    }
  }
  
  _createTriangle(x, y, scale) {
    const geometry = new THREE.BufferGeometry();
    const vertices = new Float32Array([
      x, y, 0,
      x - scale, y - scale * 1.5, 0,
      x + scale, y - scale * 1.5, 0
    ]);
    geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
    const material = new THREE.MeshBasicMaterial({ color: 0xffff00, side: THREE.DoubleSide });
    return new THREE.Mesh(geometry, material);
  }
  
  _applyContour() {
    if (!this.data.stress && !this.data.displacement) return;
    
    const values = this._getScalarField();
    if (!values) return;
    
    const { min, max } = this._getScalarRange(values);
    const colormap = this._getColormap();
    
    // Apply colors to deformed mesh
    if (this.deformedGroup.children.length > 0) {
      const mesh = this.deformedGroup.children[0];
      const colors = new Float32Array(this.numNodes * 3);
      
      for (let i = 0; i < this.numNodes; i++) {
        const t = (values[i] - min) / (max - min);
        const color = colormap(t);
        colors[i * 3] = color.r;
        colors[i * 3 + 1] = color.g;
        colors[i * 3 + 2] = color.b;
      }
      
      mesh.geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
      mesh.material.vertexColors = true;
      mesh.material.needsUpdate = true;
    }
    
    // Update colorbar
    this._updateColorbar(min, max);
    
    // Update colorbar gradient
    if (this.colorbar) {
      this.colorbar.updateGradient(colormap);
    }
  }
  
  _getScalarField() {
    if (!this.data) return null;
    
    const nodalStress = this.data.nodalStress;
    const disp = this.data.displacement;
    const type = this.contourType;
    
    // Use nodalStress for smooth nodal coloring (matches node count)
    if (type === 'von_mises' && nodalStress) return nodalStress.von_mises;
    if (type === 'sigma_1' && nodalStress) return nodalStress.sigma_1;
    if (type === 'sigma_2' && nodalStress) return nodalStress.sigma_2;
    if (type === 'displacement' && disp) {
      return disp.map(d => Math.sqrt(d.ux * d.ux + d.uy * d.uy));
    }
    return null;
  }
  
  _getScalarRange(values) {
    if (!values || values.length === 0) return { min: 0, max: 1 };
    let min = Infinity, max = -Infinity;
    for (const v of values) {
      if (v < min) min = v;
      if (v > max) max = v;
    }
    if (min === max) { min -= 0.5; max += 0.5; }
    return { min, max };
  }
  
  _getColormap() {
    return Colormaps.hot;
  }
  
  _getUnits() {
    const units = {
      'von_mises': 'Pa', 'sigma_xx': 'Pa', 'sigma_yy': 'Pa', 'sigma_xy': 'Pa',
      'sigma_1': 'Pa', 'sigma_2': 'Pa', 'displacement': 'm'
    };
    return units[this.contourType] || '';
  }
  
  _updateColorbar(min, max) {
    const colorbar = document.getElementById('fea-colorbar');
    if (!colorbar) return;
    
    const minLabel = colorbar.querySelector('.colorbar-min');
    const maxLabel = colorbar.querySelector('.colorbar-max');
    const titleLabel = colorbar.querySelector('.colorbar-title');
    
    if (minLabel) minLabel.textContent = this._formatValue(min);
    if (maxLabel) maxLabel.textContent = this._formatValue(max);
    if (titleLabel) titleLabel.textContent = `${this.contourType.replace('_', ' ')} (${this._getUnits()})`;
  }
  
  _formatValue(value) {
    if (Math.abs(value) < 1e-10) return '0';
    if (Math.abs(value) >= 1e6) return `${(value / 1e6).toFixed(2)}e6`;
    if (Math.abs(value) >= 1e3) return `${(value / 1e3).toFixed(2)}e3`;
    if (Math.abs(value) < 1e-3) return `${(value * 1e6).toFixed(2)}e-6`;
    return value.toFixed(4);
  }
  
  // Public API
  
  setDisplacementScale(scale) {
    this.dispScale = scale;
    this._buildDeformedMesh();
    this._applyContour();
    
    // Update scale display
    const scaleDisplay = document.getElementById('scaleValue');
    if (scaleDisplay) scaleDisplay.textContent = `${Math.round(scale)}x`;
  }
  
  setContourType(type) {
    this.contourType = type;
    this._applyContour();
    
    // Update colorbar gradient
    if (this.colorbar) {
      this.colorbar.updateGradient(this._getColormap());
    }
  }
  
  toggleUndeformed() {
    this.showUndeformed = !this.showUndeformed;
    this.undeformedGroup.visible = this.showUndeformed;
  }
  
  toggleDeformed() {
    this.showDeformed = !this.showDeformed;
    this.deformedGroup.visible = this.showDeformed;
  }
  
  toggleEdges() {
    this.showEdges = !this.showEdges;
    this.edgesGroup.visible = this.showEdges;
  }
  
  toggleArrows() {
    this.showArrows = !this.showArrows;
    this.arrowsGroup.visible = this.showArrows;
  }
  
  toggleBoundary() {
    this.showBoundary = !this.showBoundary;
    this.boundaryGroup.visible = this.showBoundary;
  }
  
  setColorbar(colorbar) {
    this.colorbar = colorbar;
  }
  
  playAnimation() {
    this.isPlaying = true;
    this.animationProgress = 0;
    this.animationStartTime = performance.now();
  }
  
  pauseAnimation() {
    this.isPlaying = false;
  }
  
  resetAnimation() {
    this.isPlaying = false;
    this.animationProgress = 0;
    this.setDisplacementScale(0);
    
    const progressBar = document.getElementById('animationProgress');
    if (progressBar) progressBar.style.width = '0%';
  }
  
  exportPNG() {
    this.renderer.render(this.scene, this.camera);
    const link = document.createElement('a');
    link.download = 'fea_visualization.png';
    link.href = this.renderer.domElement.toDataURL('image/png');
    link.click();
  }
  
  render() {
    this.renderer.render(this.scene, this.camera);
  }
}
