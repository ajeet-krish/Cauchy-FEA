/**
 * FEA Viewer 3D -- Interactive 3D hexahedral finite element mesh visualization
 * using Three.js WebGL. Renders deformed H8 meshes with stress/displacement
 * contours, principal stress vectors, and boundary condition symbols.
 *
 * Usage:
 *   const viewer = new FEAViewer3D(document.getElementById('container'));
 *   viewer.loadData('assets/data/cantilever_3d.json');
 */

class FEAViewer3D {
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
        this.directionalLight2 = null;

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
        this.bounds = { xmin: 0, xmax: 1, ymin: 0, ymax: 1, zmin: 0, zmax: 1 };

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
        this.camera = new THREE.PerspectiveCamera(45, aspect, 0.001, 1000);
        this.camera.position.set(2.0, 2.0, 3.0);
        this.camera.lookAt(0.5, 0.5, 0.5);

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
        this.controls.minDistance = 0.01;
        this.controls.maxDistance = 1000;

        // Create lighting (two directional lights for better 3D shading)
        this.ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
        this.scene.add(this.ambientLight);

        this.directionalLight = new THREE.DirectionalLight(0xffffff, 0.7);
        this.directionalLight.position.set(1, 1, 1);
        this.scene.add(this.directionalLight);

        this.directionalLight2 = new THREE.DirectionalLight(0xffffff, 0.3);
        this.directionalLight2.position.set(-1, -0.5, -1);
        this.scene.add(this.directionalLight2);

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
            const progressBar = document.getElementById('animationProgress3D');
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
        // Support both 'hex_elements' and 'elements' keys
        this.hexElements = this.data.hex_elements || this.data.elements || [];
        this.numElements = this.hexElements.length;

        // Compute bounds from nodes
        let xmin = Infinity, xmax = -Infinity;
        let ymin = Infinity, ymax = -Infinity;
        let zmin = Infinity, zmax = -Infinity;

        for (const node of this.data.nodes) {
            if (node.x < xmin) xmin = node.x;
            if (node.x > xmax) xmax = node.x;
            if (node.y < ymin) ymin = node.y;
            if (node.y > ymax) ymax = node.y;
            if (node.z < zmin) zmin = node.z;
            if (node.z > zmax) zmax = node.z;
        }

        this.bounds = { xmin, xmax, ymin, ymax, zmin, zmax };

        // Compute default displacement scale
        if (this.data.displacement) {
            let maxDisp = 0;
            for (const d of this.data.displacement) {
                const mag = Math.sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz);
                if (mag > maxDisp) maxDisp = mag;
            }
            if (maxDisp > 0) {
                const xRange = this.bounds.xmax - this.bounds.xmin;
                const yRange = this.bounds.ymax - this.bounds.ymin;
                const zRange = this.bounds.zmax - this.bounds.zmin;
                this.dispScale = Math.round(0.1 * Math.max(xRange, yRange, zRange) / maxDisp);
            }
        }
    }

    _fitView() {
        if (!this.bounds) return;

        const xCenter = (this.bounds.xmin + this.bounds.xmax) / 2;
        const yCenter = (this.bounds.ymin + this.bounds.ymax) / 2;
        const zCenter = (this.bounds.zmin + this.bounds.zmax) / 2;
        const xRange = this.bounds.xmax - this.bounds.xmin;
        const yRange = this.bounds.ymax - this.bounds.ymin;
        const zRange = this.bounds.zmax - this.bounds.zmin;
        const maxRange = Math.max(xRange, yRange, zRange);

        this.camera.position.set(
            xCenter + maxRange * 1.5,
            yCenter + maxRange * 1.0,
            zCenter + maxRange * 2.0
        );
        this.camera.lookAt(xCenter, yCenter, zCenter);
        this.controls.target.set(xCenter, yCenter, zCenter);
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
            if (child.material) {
                if (Array.isArray(child.material)) {
                    child.material.forEach(m => m.dispose());
                } else {
                    child.material.dispose();
                }
            }
            group.remove(child);
        }
    }

    _buildUndeformedMesh() {
        const geometry = this._buildHexGeometry(this.data.nodes, null, 0);

        const material = new THREE.MeshPhongMaterial({
            color: 0x4488aa,
            transparent: true,
            opacity: 0.15,
            side: THREE.DoubleSide
        });

        const mesh = new THREE.Mesh(geometry, material);
        this.undeformedGroup.add(mesh);
    }

    _buildDeformedMesh() {
        if (!this.data.displacement) return;

        const geometry = this._buildHexGeometry(this.data.nodes, this.data.displacement, this.dispScale);

        const material = new THREE.MeshPhongMaterial({
            color: 0x66aacc,
            side: THREE.DoubleSide
        });

        const mesh = new THREE.Mesh(geometry, material);
        this.deformedGroup.add(mesh);
    }

    /**
     * Build a BufferGeometry from H8 hex elements.
     * Each hex has 6 faces, each face is 2 triangles = 12 triangles = 36 indices per hex.
     */
    _buildHexGeometry(nodes, displacement, scale) {
        const hexElements = this.hexElements;

        // Each hex: 6 faces * 2 tris * 3 verts = 36 vertices (non-indexed for flat shading
        // and per-vertex coloring). We duplicate vertices so each triangle gets its own
        // vertex color.
        const vertexCount = hexElements.length * 36;
        const positions = new Float32Array(vertexCount * 3);

        // H8 face definitions: each face is 4 node indices, wound for outward normals
        // Using standard right-hand rule for face winding
        const faceNodeIndices = [
            // Bottom face (z=-1): n0-n1-n2-n3
            [0, 1, 2, 3],
            // Top face (z=+1): n4-n5-n6-n7
            [4, 5, 6, 7],
            // Front face (y=-1): n0-n1-n5-n4
            [0, 1, 5, 4],
            // Back face (y=+1): n3-n2-n6-n7
            [3, 2, 6, 7],
            // Left face (x=-1): n0-n3-n7-n4
            [0, 3, 7, 4],
            // Right face (x=+1): n1-n2-n6-n5
            [1, 2, 6, 5]
        ];

        // Two triangles per quad face: (0,1,2) and (0,2,3)
        const triIndices = [
            [0, 1, 2],
            [0, 2, 3]
        ];

        let vertexOffset = 0;

        for (let e = 0; e < hexElements.length; e++) {
            const elem = hexElements[e];
            const nodeIndices = [elem.n0, elem.n1, elem.n2, elem.n3,
                                 elem.n4, elem.n5, elem.n6, elem.n7];

            for (const face of faceNodeIndices) {
                for (const tri of triIndices) {
                    for (const idx of tri) {
                        const nodeIdx = nodeIndices[face[idx]];
                        const node = nodes[nodeIdx];

                        let x = node.x;
                        let y = node.y;
                        let z = node.z;

                        if (displacement) {
                            const d = displacement[nodeIdx];
                            x += d.ux * scale;
                            y += d.uy * scale;
                            z += d.uz * scale;
                        }

                        positions[vertexOffset * 3] = x;
                        positions[vertexOffset * 3 + 1] = y;
                        positions[vertexOffset * 3 + 2] = z;
                        vertexOffset++;
                    }
                }
            }
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        return geometry;
    }

    _buildEdges() {
        const material = new THREE.LineBasicMaterial({ color: 0xffffff, linewidth: 1 });

        // H8 hex: 12 edges connecting node pairs
        const edgePairs = [
            // Bottom face edges
            [0, 1], [1, 2], [2, 3], [3, 0],
            // Top face edges
            [4, 5], [5, 6], [6, 7], [7, 4],
            // Vertical edges connecting bottom to top
            [0, 4], [1, 5], [2, 6], [3, 7]
        ];

        for (const elem of this.hexElements) {
            const nodeIndices = [elem.n0, elem.n1, elem.n2, elem.n3,
                                 elem.n4, elem.n5, elem.n6, elem.n7];

            for (const [a, b] of edgePairs) {
                const na = this.data.nodes[nodeIndices[a]];
                const nb = this.data.nodes[nodeIndices[b]];

                const geometry = new THREE.BufferGeometry();
                const vertices = new Float32Array([
                    na.x, na.y, na.z,
                    nb.x, nb.y, nb.z
                ]);
                geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
                const line = new THREE.Line(geometry, material);
                this.edgesGroup.add(line);
            }
        }
    }

    _buildPrincipalStressArrows() {
        if (!this.data.stress || !this.data.stress.sigma_1) return;

        const scale = 0.1;

        for (let i = 0; i < this.numElements; i++) {
            const elem = this.hexElements[i];
            const nodeIndices = [elem.n0, elem.n1, elem.n2, elem.n3,
                                 elem.n4, elem.n5, elem.n6, elem.n7];

            // Compute centroid of hex
            let cx = 0, cy = 0, cz = 0;
            for (const idx of nodeIndices) {
                cx += this.data.nodes[idx].x;
                cy += this.data.nodes[idx].y;
                cz += this.data.nodes[idx].z;
            }
            cx /= 8;
            cy /= 8;
            cz /= 8;

            const sigma1 = this.data.stress.sigma_1[i];
            const sigma2 = this.data.stress.sigma_2[i];
            const sigma3 = this.data.stress.sigma_3 ? this.data.stress.sigma_3[i] : 0;

            // For 3D, principal stress arrows need direction vectors.
            // If theta1/theta2 are provided (2D projection), use them in the xy-plane.
            // Otherwise, draw arrows along the three principal axes.
            const theta1 = this.data.stress.theta1 ? this.data.stress.theta1[i] : 0;
            const theta2 = this.data.stress.theta2 ? this.data.stress.theta2[i] : 0;

            // Arrow for sigma_1 (in xy-plane from theta1)
            if (Math.abs(sigma1) > 1e-6) {
                const dir = new THREE.Vector3(
                    Math.cos(theta1),
                    Math.sin(theta1),
                    0
                );
                const length = Math.abs(sigma1) * scale;
                const color = sigma1 > 0 ? 0xff0000 : 0x0000ff;
                const origin = new THREE.Vector3(cx, cy, cz);
                const arrow = new THREE.ArrowHelper(dir, origin, length, color, length * 0.2, length * 0.12);
                this.arrowsGroup.add(arrow);
            }

            // Arrow for sigma_2 (perpendicular to sigma_1 in xy-plane)
            if (Math.abs(sigma2) > 1e-6) {
                const dir = new THREE.Vector3(
                    Math.cos(theta2),
                    Math.sin(theta2),
                    0
                );
                const length = Math.abs(sigma2) * scale;
                const color = sigma2 > 0 ? 0xff6666 : 0x6666ff;
                const origin = new THREE.Vector3(cx, cy, cz);
                const arrow = new THREE.ArrowHelper(dir, origin, length, color, length * 0.2, length * 0.12);
                this.arrowsGroup.add(arrow);
            }

            // Arrow for sigma_3 along z-axis
            if (Math.abs(sigma3) > 1e-6) {
                const dir = new THREE.Vector3(0, 0, 1);
                const length = Math.abs(sigma3) * scale;
                const color = sigma3 > 0 ? 0xff9999 : 0x9999ff;
                const origin = new THREE.Vector3(cx, cy, cz);
                const arrow = new THREE.ArrowHelper(dir, origin, length, color, length * 0.2, length * 0.12);
                this.arrowsGroup.add(arrow);
            }
        }
    }

    _buildBoundaryConditions() {
        if (!this.data.boundary) return;

        // Fixed supports (yellow tetrahedra)
        if (this.data.boundary.dirichlet) {
            const fixedNodes = new Set();
            for (const bc of this.data.boundary.dirichlet) {
                fixedNodes.add(bc.node);
            }

            for (const nodeIdx of fixedNodes) {
                const node = this.data.nodes[nodeIdx];
                const tet = this._createTetrahedron(node.x, node.y, node.z, 0.06);
                this.boundaryGroup.add(tet);
            }
        }

        // Forces (colored arrows)
        if (this.data.boundary.neumann) {
            for (const load of this.data.boundary.neumann) {
                const node = this.data.nodes[load.node];
                const dir = new THREE.Vector3(
                    load.dof === 0 ? 1 : 0,
                    load.dof === 1 ? 1 : 0,
                    load.dof === 2 ? 1 : 0
                );
                const length = Math.abs(load.value) * 0.0002;
                const color = load.value > 0 ? 0x00ff00 : 0xff0000;
                const origin = new THREE.Vector3(node.x, node.y, node.z);
                const arrow = new THREE.ArrowHelper(dir, origin, length, color, length * 0.3, length * 0.2);
                this.boundaryGroup.add(arrow);
            }
        }
    }

    /**
     * Create a small tetrahedron at a point for fixed BC symbol.
     */
    _createTetrahedron(x, y, z, scale) {
        const geometry = new THREE.BufferGeometry();
        const s = scale;

        // Tetrahedron vertices: apex at (x,y,z), base below
        const vertices = new Float32Array([
            // Apex to base face 1
            x, y, z,
            x - s, y - s * 1.5, z - s,
            x + s, y - s * 1.5, z - s,

            // Apex to base face 2
            x, y, z,
            x + s, y - s * 1.5, z - s,
            x, y - s * 1.5, z + s,

            // Apex to base face 3
            x, y, z,
            x, y - s * 1.5, z + s,
            x - s, y - s * 1.5, z - s,

            // Base face
            x - s, y - s * 1.5, z - s,
            x, y - s * 1.5, z + s,
            x + s, y - s * 1.5, z - s
        ]);

        geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
        geometry.computeVertexNormals();

        const material = new THREE.MeshBasicMaterial({
            color: 0xffff00,
            side: THREE.DoubleSide
        });

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
            const geometry = mesh.geometry;
            const positions = geometry.getAttribute('position');

            // The deformed mesh has 36 vertices per hex element.
            // We need to map each vertex to a scalar value.
            // Since vertices are duplicated per triangle, we need to figure out which
            // original node each vertex corresponds to. We rebuild the mapping.
            const colors = new Float32Array(positions.count * 3);

            const hexElements = this.hexElements;

            const faceNodeIndices = [
                [0, 1, 2, 3],   // Bottom
                [4, 5, 6, 7],   // Top
                [0, 1, 5, 4],   // Front
                [3, 2, 6, 7],   // Back
                [0, 3, 7, 4],   // Left
                [1, 2, 6, 5]    // Right
            ];

            const triIndices = [
                [0, 1, 2],
                [0, 2, 3]
            ];

            let vertexOffset = 0;

            for (let e = 0; e < hexElements.length; e++) {
                const elem = hexElements[e];
                const nodeIndices = [elem.n0, elem.n1, elem.n2, elem.n3,
                                     elem.n4, elem.n5, elem.n6, elem.n7];

                for (const face of faceNodeIndices) {
                    for (const tri of triIndices) {
                        for (const idx of tri) {
                            const nodeIdx = nodeIndices[face[idx]];
                            const val = values[nodeIdx];
                            const t = (val - min) / (max - min || 1);
                            const color = colormap(t);
                            colors[vertexOffset * 3] = color.r;
                            colors[vertexOffset * 3 + 1] = color.g;
                            colors[vertexOffset * 3 + 2] = color.b;
                            vertexOffset++;
                        }
                    }
                }
            }

            geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
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
        if (type === 'sigma_3' && nodalStress) return nodalStress.sigma_3;
        if (type === 'displacement' && disp) {
            return disp.map(d => Math.sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz));
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
            'von_mises': 'Pa', 'sigma_xx': 'Pa', 'sigma_yy': 'Pa', 'sigma_zz': 'Pa',
            'sigma_xy': 'Pa', 'sigma_yz': 'Pa', 'sigma_xz': 'Pa',
            'sigma_1': 'Pa', 'sigma_2': 'Pa', 'sigma_3': 'Pa',
            'displacement': 'm'
        };
        return units[this.contourType] || '';
    }

    _updateColorbar(min, max) {
        const colorbar = document.getElementById('fea-colorbar-3d');
        if (!colorbar) return;

        const minLabel = colorbar.querySelector('.colorbar-min');
        const maxLabel = colorbar.querySelector('.colorbar-max');
        const titleLabel = colorbar.querySelector('.colorbar-title');

        if (minLabel) minLabel.textContent = this._formatValue(min);
        if (maxLabel) maxLabel.textContent = this._formatValue(max);
        if (titleLabel) titleLabel.textContent = `${this.contourType.replace(/_/g, ' ')} (${this._getUnits()})`;
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
        const scaleDisplay = document.getElementById('scaleValue3D');
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

        const progressBar = document.getElementById('animationProgress3D');
        if (progressBar) progressBar.style.width = '0%';
    }

    exportPNG() {
        this.renderer.render(this.scene, this.camera);
        const link = document.createElement('a');
        link.download = 'fea_3d_visualization.png';
        link.href = this.renderer.domElement.toDataURL('image/png');
        link.click();
    }

    render() {
        this.renderer.render(this.scene, this.camera);
    }
}

/**
 * UI Controls for 3D FEA Viewer.
 * Creates toolbar with 3D-specific contour selector, scale slider, toggle buttons,
 * animation controls, and export functionality.
 *
 * Usage:
 *   const controls = new FEAControls3D(viewer);
 *   controls.createToolbar(document.getElementById('controls'));
 */

class FEAControls3D {
    constructor(viewer) {
        this.viewer = viewer;
        this.toolbar = null;
    }

    createToolbar(container) {
        this.toolbar = document.createElement('div');
        this.toolbar.className = 'viewer-toolbar viewer-toolbar-3d';

        // Contour selector (3D-specific options)
        const contourGroup = this._createControlGroup('Contour:');
        const contourSelect = document.createElement('select');
        contourSelect.id = 'contourType3D';
        const options = [
            { value: 'von_mises', text: 'Von Mises' },
            { value: 'sigma_xx', text: 'Sigma XX' },
            { value: 'sigma_yy', text: 'Sigma YY' },
            { value: 'sigma_zz', text: 'Sigma ZZ' },
            { value: 'sigma_xy', text: 'Sigma XY' },
            { value: 'sigma_yz', text: 'Sigma YZ' },
            { value: 'sigma_xz', text: 'Sigma XZ' },
            { value: 'sigma_1', text: 'Sigma 1 (Principal)' },
            { value: 'sigma_2', text: 'Sigma 2 (Principal)' },
            { value: 'sigma_3', text: 'Sigma 3 (Principal)' },
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
        scaleSlider.id = 'dispScale3D';
        scaleSlider.min = '1';
        scaleSlider.max = '10000';
        scaleSlider.value = String(this.viewer.dispScale);
        scaleSlider.addEventListener('input', (e) => {
            this.viewer.setDisplacementScale(parseInt(e.target.value));
        });
        scaleGroup.appendChild(scaleSlider);

        const scaleValue = document.createElement('span');
        scaleValue.id = 'scaleValue3D';
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
        progressFill.id = 'animationProgress3D';
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
        // Only inject styles once
        if (document.getElementById('fea-3d-controls-styles')) return;

        const style = document.createElement('style');
        style.id = 'fea-3d-controls-styles';
        style.textContent = `
            .viewer-toolbar-3d {
                position: absolute;
                top: 10px;
                left: 10px;
                background: rgba(0, 0, 0, 0.85);
                padding: 10px;
                border-radius: 4px;
                display: flex;
                flex-wrap: wrap;
                gap: 10px;
                z-index: 100;
                max-width: calc(100% - 20px);
            }

            .viewer-toolbar-3d .control-group {
                display: flex;
                align-items: center;
                gap: 5px;
            }

            .viewer-toolbar-3d .control-group label {
                color: white;
                font-family: 'JetBrains Mono', monospace;
                font-size: 12px;
            }

            .viewer-toolbar-3d .control-group select {
                background: #2a2a3e;
                color: white;
                border: 1px solid #444;
                padding: 4px 8px;
                border-radius: 4px;
                font-family: 'JetBrains Mono', monospace;
                font-size: 12px;
            }

            .viewer-toolbar-3d .control-group input[type="range"] {
                width: 100px;
            }

            .viewer-toolbar-3d .control-group span {
                color: white;
                font-family: 'JetBrains Mono', monospace;
                font-size: 12px;
                min-width: 50px;
            }

            .viewer-toolbar-3d .control-group button {
                background: #2a2a3e;
                color: white;
                border: 1px solid #444;
                padding: 4px 8px;
                border-radius: 4px;
                cursor: pointer;
                font-family: 'JetBrains Mono', monospace;
                font-size: 12px;
            }

            .viewer-toolbar-3d .control-group button:hover {
                background: #3a3a4e;
            }

            .viewer-toolbar-3d .control-group button.active {
                background: #4a7a9a;
                border-color: #6a9aba;
            }

            .viewer-toolbar-3d .progress-bar {
                width: 100px;
                height: 6px;
                background: #2a2a3e;
                border-radius: 3px;
                overflow: hidden;
            }

            .viewer-toolbar-3d .progress-fill {
                width: 0%;
                height: 100%;
                background: #4a7a9a;
                transition: width 0.1s linear;
            }
        `;
        document.head.appendChild(style);
    }
}
