import { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { save, open } from '@tauri-apps/plugin-dialog';
import { writeFile, readTextFile } from '@tauri-apps/plugin-fs';
import type {
  ProjectState,
  SolveResult,
  Shape,
  Material,
  DirichletBC,
  NeumannBC,
  BCTool,
  SweepResult,
} from './types';
import GeometryEditor from './components/GeometryEditor';
import BCLoadEditor from './components/BCLoadEditor';
import MeshCanvas from './components/MeshCanvas';
import ResultsCanvas from './components/ResultsCanvas';
import SolverPanel from './components/SolverPanel';
import ToolBar from './components/ToolBar';
import MaterialLibrary from './components/MaterialLibrary';
import ParameterSweep from './components/ParameterSweep';
import ConvergenceChart from './components/ConvergenceChart';

const DEFAULT_MATERIAL: Material = {
  E: 200e9,
  nu: 0.3,
  rho: 7850,
  t: 0.01,
};

const EMPTY_PROJECT: ProjectState = {
  shapes: [],
  mesh: null,
  dirichlet: [],
  neumann: [],
  material: DEFAULT_MATERIAL,
  planeType: 'stress',
  solverType: 'cg',
  result: null,
  nx: 16,
  ny: 8,
  elemType: 0,
};

function App() {
  const [project, setProject] = useState<ProjectState>(EMPTY_PROJECT);
  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const [meshGenerating, setMeshGenerating] = useState(false);
  const [meshError, setMeshError] = useState<string | null>(null);
  const [isSolving, setIsSolving] = useState(false);
  const [solveError, setSolveError] = useState<string | null>(null);
  const [shapesDirty, setShapesDirty] = useState(false);
  const [activeBCTool, setActiveBCTool] = useState<BCTool>(null);
  const [sweepResults, setSweepResults] = useState<SweepResult[]>([]);
  const [isDirty, setIsDirty] = useState(false);
  const [solverTimeMs, setSolverTimeMs] = useState<number | null>(null);

  // Update document title when dirty state changes
  useEffect(() => {
    document.title = isDirty ? 'Cauchy *' : 'Cauchy';
  }, [isDirty]);

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const mod = e.metaKey || e.ctrlKey;
      if (mod && e.key === 's') {
        e.preventDefault();
        handleSave();
      }
      if (mod && e.key === 'o') {
        e.preventDefault();
        handleOpen();
      }
      if (mod && e.key === 'n') {
        e.preventDefault();
        handleNew();
      }
      if (mod && e.shiftKey && e.key === 'E') {
        e.preventDefault();
        void handleExportPng();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [project]);

  const togglePanel = (name: string) => {
    setCollapsed((prev) => ({ ...prev, [name]: !prev[name] }));
  };

  const handleShapesChange = (shapes: Shape[]) => {
    setProject((prev) => ({ ...prev, shapes, mesh: null, result: null }));
    setShapesDirty(true);
    setIsDirty(true);
  };

  const handleMaterialChange = (material: Material) => {
    if (!Number.isFinite(material.E) || material.E <= 0) return;
    if (!Number.isFinite(material.nu) || material.nu <= 0 || material.nu >= 0.5) return;
    if (!Number.isFinite(material.rho) || material.rho <= 0) return;
    if (!Number.isFinite(material.t) || material.t <= 0) return;
    setProject((prev) => ({ ...prev, material }));
    setIsDirty(true);
  };

  const handleNxChange = (nx: number) => {
    const clamped = Math.max(2, Math.min(200, Math.floor(nx) || 2));
    setProject((prev) => ({ ...prev, nx: clamped }));
    setIsDirty(true);
  };

  const handleNyChange = (ny: number) => {
    const clamped = Math.max(2, Math.min(200, Math.floor(ny) || 2));
    setProject((prev) => ({ ...prev, ny: clamped }));
    setIsDirty(true);
  };

  const handleElemTypeChange = (elemType: number) => {
    setProject((prev) => ({ ...prev, elemType }));
    setIsDirty(true);
  };

  const handlePlaneTypeChange = (planeType: 'stress' | 'strain') => {
    setProject((prev) => ({ ...prev, planeType }));
    setIsDirty(true);
  };

  const handleSolverTypeChange = (solverType: 'cg' | 'cholesky') => {
    setProject((prev) => ({ ...prev, solverType }));
    setIsDirty(true);
  };

  const handleDirichletChange = (dirichlet: DirichletBC[]) => {
    setProject((prev) => ({ ...prev, dirichlet }));
    setIsDirty(true);
  };

  const handleNeumannChange = (neumann: NeumannBC[]) => {
    setProject((prev) => ({ ...prev, neumann }));
    setIsDirty(true);
  };

  const handleBCToolChange = (tool: BCTool) => {
    setActiveBCTool(tool);
  };

  const handleNodeClick = (nodeIndex: number) => {
    if (!activeBCTool) return;

    setProject((prev) => {
      if (!prev.mesh) return prev;
      if (nodeIndex < 0 || nodeIndex >= prev.mesh.nodes.length) return prev;

      // Fixed UX+UY
      if (activeBCTool === 'fixed_ux_uy') {
        const existing = prev.dirichlet.filter((bc) => bc.node === nodeIndex);
        if (existing.length > 0) return prev;
        setIsDirty(true);
        return {
          ...prev,
          dirichlet: [
            ...prev.dirichlet,
            { node: nodeIndex, dof: 0, value: 0 },
            { node: nodeIndex, dof: 1, value: 0 },
          ],
        };
      }

      // Fixed UX only
      if (activeBCTool === 'fixed_ux') {
        const existing = prev.dirichlet.filter(
          (bc) => bc.node === nodeIndex && bc.dof === 0,
        );
        if (existing.length > 0) return prev;
        setIsDirty(true);
        return {
          ...prev,
          dirichlet: [
            ...prev.dirichlet,
            { node: nodeIndex, dof: 0, value: 0 },
          ],
        };
      }

      // Fixed UY only
      if (activeBCTool === 'fixed_uy') {
        const existing = prev.dirichlet.filter(
          (bc) => bc.node === nodeIndex && bc.dof === 1,
        );
        if (existing.length > 0) return prev;
        setIsDirty(true);
        return {
          ...prev,
          dirichlet: [
            ...prev.dirichlet,
            { node: nodeIndex, dof: 1, value: 0 },
          ],
        };
      }

      // Roller X: constrained in Y
      if (activeBCTool === 'roller_x') {
        const existing = prev.dirichlet.filter(
          (bc) => bc.node === nodeIndex && bc.dof === 1,
        );
        if (existing.length > 0) return prev;
        setIsDirty(true);
        return {
          ...prev,
          dirichlet: [
            ...prev.dirichlet,
            { node: nodeIndex, dof: 1, value: 0 },
          ],
        };
      }

      // Roller Y: constrained in X
      if (activeBCTool === 'roller_y') {
        const existing = prev.dirichlet.filter(
          (bc) => bc.node === nodeIndex && bc.dof === 0,
        );
        if (existing.length > 0) return prev;
        setIsDirty(true);
        return {
          ...prev,
          dirichlet: [
            ...prev.dirichlet,
            { node: nodeIndex, dof: 0, value: 0 },
          ],
        };
      }

      // Force X
      if (activeBCTool === 'force_x') {
        setIsDirty(true);
        return {
          ...prev,
          neumann: [
            ...prev.neumann,
            { node: nodeIndex, dof: 0, value: -1000 },
          ],
        };
      }

      // Force Y
      if (activeBCTool === 'force_y') {
        setIsDirty(true);
        return {
          ...prev,
          neumann: [
            ...prev.neumann,
            { node: nodeIndex, dof: 1, value: -1000 },
          ],
        };
      }

      return prev;
    });
  };

  const handleGenerateMesh = async () => {
    if (project.shapes.length === 0) return;
    setMeshGenerating(true);
    setMeshError(null);
    try {
      const meshJson = await invoke('generate_mesh', {
        shapesJson: JSON.stringify(project.shapes),
        nx: project.nx,
        ny: project.ny,
        elemType: project.elemType,
      });
      setProject((prev) => ({ ...prev, mesh: meshJson as ProjectState['mesh'] }));
      setShapesDirty(false);
      setIsDirty(true);
    } catch (err) {
      console.error('Mesh generation failed:', err);
      setMeshError('Mesh generation failed. Check geometry and mesh density.');
    } finally {
      setMeshGenerating(false);
    }
  };

  const handleSolve = async () => {
    if (!project.mesh) return;
    setIsSolving(true);
    setSolveError(null);
    setSolverTimeMs(null);
    try {
      const configJson = JSON.stringify({
        planeType: project.planeType,
        solverType: project.solverType,
      });
      const startTime = performance.now();
      const result = await invoke('run_fea_solve', {
        meshJson: JSON.stringify(project.mesh),
        configJson,
      });
      const elapsed = performance.now() - startTime;
      setSolverTimeMs(elapsed);

      // Runtime validation before type assertion
      const parsed = result as Record<string, unknown>;
      if (
        !parsed?.displacements ||
        !parsed?.stresses ||
        parsed.max_displacement == null
      ) {
        setSolveError('Invalid solver response.');
        return;
      }
      setProject((prev) => ({
        ...prev,
        result: parsed as unknown as SolveResult,
      }));
      setIsDirty(true);
    } catch (err) {
      console.error('Solve failed:', err);
      setSolveError('Solver failed. Check mesh and boundary conditions.');
    } finally {
      setIsSolving(false);
    }
  };

  const handleNew = () => {
    setProject(EMPTY_PROJECT);
    setShapesDirty(false);
    setIsDirty(false);
    setSweepResults([]);
  };

  const handleOpen = async () => {
    try {
      const path = await open({
        filters: [{ name: 'Cauchy Project', extensions: ['cauchy'] }],
        multiple: false,
      });
      if (path) {
        const content = await readTextFile(path);
        const loaded = JSON.parse(content) as ProjectState;
        setProject(loaded);
        setIsDirty(false);
      }
    } catch (err) {
      console.error('Open failed:', err);
    }
  };

  const handleSave = async () => {
    try {
      const path = await save({
        defaultPath: 'project.cauchy',
        filters: [{ name: 'Cauchy Project', extensions: ['cauchy'] }],
      });
      if (path) {
        const projectJson = JSON.stringify(project, null, 2);
        const encoder = new TextEncoder();
        await writeFile(path, encoder.encode(projectJson));
        setIsDirty(false);
      }
    } catch (err) {
      console.error('Save failed:', err);
    }
  };

  const handleExportPng = async () => {
    try {
      const canvas = document.querySelector('.canvas-container canvas') as HTMLCanvasElement;
      if (!canvas) return;

      const path = await save({
        defaultPath: 'fea-result.png',
        filters: [{ name: 'PNG Image', extensions: ['png'] }],
      });
      if (path) {
        const dataUrl = canvas.toDataURL('image/png');
        const base64 = dataUrl.split(',')[1];
        const binary = atob(base64);
        const bytes = new Uint8Array(binary.length);
        for (let i = 0; i < binary.length; i++) {
          bytes[i] = binary.charCodeAt(i);
        }
        await writeFile(path, bytes);
      }
    } catch (err) {
      console.error('Export failed:', err);
    }
  };

  const elemTypeLabel = (t: number) => {
    if (t === 0) return 'Q4';
    if (t === 1) return 'Q8';
    return 'T3';
  };

  return (
    <div className="app">
      <header className="app-header">
        <h1>Cauchy</h1>
        <span className="subtitle">2D FEA Structural Solver</span>
        <ToolBar
          onNew={handleNew}
          onOpen={handleOpen}
          onSave={handleSave}
          onExportPng={handleExportPng}
          isDirty={isDirty}
          hasResults={!!project.result}
        />
      </header>

      <div className="main-layout">
        <aside className="sidebar">
          {/* Geometry Panel */}
          <div className="panel">
            <div
              className="panel-header"
              onClick={() => togglePanel('geometry')}
            >
              <h2>Geometry</h2>
              <span
                className={`panel-toggle ${collapsed['geometry'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['geometry'] ? 'collapsed' : ''}`}
            >
              <GeometryEditor
                shapes={project.shapes}
                nx={project.nx}
                ny={project.ny}
                onChange={handleShapesChange}
              />
            </div>
          </div>

          {/* Material Panel */}
          <div className="panel">
            <div
              className="panel-header"
              onClick={() => togglePanel('material')}
            >
              <h2>Material</h2>
              <span
                className={`panel-toggle ${collapsed['material'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['material'] ? 'collapsed' : ''}`}
            >
              <MaterialLibrary
                material={project.material}
                onChange={handleMaterialChange}
              />
            </div>
          </div>

          {/* Mesh Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('mesh')}>
              <h2>Mesh</h2>
              <span
                className={`panel-toggle ${collapsed['mesh'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['mesh'] ? 'collapsed' : ''}`}
            >
              <div className="form-row">
                <div className="form-group">
                  <label>Elements X</label>
                  <input
                    type="number"
                    min="2"
                    max="200"
                    value={project.nx}
                    onChange={(e) => handleNxChange(+e.target.value)}
                  />
                </div>
                <div className="form-group">
                  <label>Elements Y</label>
                  <input
                    type="number"
                    min="2"
                    max="200"
                    value={project.ny}
                    onChange={(e) => handleNyChange(+e.target.value)}
                  />
                </div>
              </div>

              <div className="form-group">
                <label>Element Type</label>
                <select
                  value={project.elemType}
                  onChange={(e) => handleElemTypeChange(+e.target.value)}
                >
                  <option value={0}>Q4 (4-node quad)</option>
                  <option value={1}>Q8 (8-node serendipity)</option>
                  <option value={2}>T3 (3-node triangle)</option>
                </select>
              </div>

              {/* Generate Mesh button */}
              <button
                className="btn-primary"
                onClick={handleGenerateMesh}
                disabled={project.shapes.length === 0 || meshGenerating}
                style={{ marginTop: 8 }}
              >
                {meshGenerating
                  ? 'Generating...'
                  : `Generate Mesh (${elemTypeLabel(project.elemType)}, ${project.nx}x${project.ny})`}
              </button>

              {project.mesh && (
                <div className="mesh-info" style={{ marginTop: 8 }}>
                  <span>
                    Nodes: <span className="value">{project.mesh.num_nodes}</span>
                  </span>
                  <span>
                    Elements:{' '}
                    <span className="value">{project.mesh.num_elements}</span>
                  </span>
                  <span>
                    DOFs: <span className="value">{project.mesh.num_dofs}</span>
                  </span>
                </div>
              )}

              {meshError && (
                <div className="solve-error" style={{ marginTop: 8 }}>
                  <span className="solve-error-icon">&#10007;</span>
                  <span>{meshError}</span>
                </div>
              )}
            </div>
          </div>

          {/* Boundary Conditions Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('bc')}>
              <h2>Boundary Conditions</h2>
              <span
                className={`panel-toggle ${collapsed['bc'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['bc'] ? 'collapsed' : ''}`}
            >
              <BCLoadEditor
                mesh={project.mesh}
                dirichlet={project.dirichlet}
                neumann={project.neumann}
                activeBCTool={activeBCTool}
                onBCToolChange={handleBCToolChange}
                onDirichletChange={handleDirichletChange}
                onNeumannChange={handleNeumannChange}
              />
            </div>
          </div>

          {/* Solver Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('solver')}>
              <h2>Solver</h2>
              <span
                className={`panel-toggle ${collapsed['solver'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['solver'] ? 'collapsed' : ''}`}
            >
              <SolverPanel
                planeType={project.planeType}
                solverType={project.solverType}
                onPlaneTypeChange={handlePlaneTypeChange}
                onSolverTypeChange={handleSolverTypeChange}
                onSolve={handleSolve}
                result={project.result}
                isSolving={isSolving}
                solveError={solveError}
              />
            </div>
          </div>

          {/* Results Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('results')}>
              <h2>Results</h2>
              <span
                className={`panel-toggle ${collapsed['results'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['results'] ? 'collapsed' : ''}`}
            >
              {project.result ? (
                <div className="results-summary">
                  <div className="result-card">
                    <div className="label">Max Displacement</div>
                    <div className="value cyan">
                      {project.result.max_displacement.toExponential(3)} m
                    </div>
                  </div>
                  <div className="result-card">
                    <div className="label">Max Stress</div>
                    <div className="value magenta">
                      {(project.result.max_stress / 1e6).toFixed(1)} MPa
                    </div>
                  </div>
                  <div className="result-card">
                    <div className="label">Solve Time</div>
                    <div className="value green">
                      {project.result.solve_time_ms.toFixed(0)} ms
                    </div>
                  </div>
                  <div className="result-card">
                    <div className="label">CG Iterations</div>
                    <div className="value">{project.result.cg_iterations}</div>
                  </div>
                </div>
              ) : (
                <div className="placeholder">No results yet. Run solver.</div>
              )}
            </div>
          </div>

          {/* Parameter Sweep Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('sweep')}>
              <h2>Parameter Sweep</h2>
              <span
                className={`panel-toggle ${collapsed['sweep'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body ${collapsed['sweep'] ? 'collapsed' : ''}`}
            >
              <ParameterSweep
                mesh={project.mesh}
                dirichlet={project.dirichlet}
                neumann={project.neumann}
                material={project.material}
                planeType={project.planeType}
                nx={project.nx}
                ny={project.ny}
                elemType={project.elemType}
                shapesJson={JSON.stringify(project.shapes)}
                onSweepComplete={setSweepResults}
              />
            </div>
          </div>

          {/* Keyboard Shortcuts */}
          <div className="panel shortcuts-panel">
            <div className="panel-header" onClick={() => togglePanel('shortcuts')}>
              <h2>Shortcuts</h2>
              <span
                className={`panel-toggle ${collapsed['shortcuts'] ? 'collapsed' : ''}`}
              >
                &#9660;
              </span>
            </div>
            <div
              className={`panel-body shortcuts-body ${collapsed['shortcuts'] ? 'collapsed' : ''}`}
            >
              <div className="shortcuts-grid">
                <div className="shortcut-row">
                  <kbd>Ctrl+S</kbd>
                  <span>Save project</span>
                </div>
                <div className="shortcut-row">
                  <kbd>Ctrl+O</kbd>
                  <span>Open project</span>
                </div>
                <div className="shortcut-row">
                  <kbd>Ctrl+N</kbd>
                  <span>New project</span>
                </div>
                <div className="shortcut-row">
                  <kbd>Ctrl+Shift+E</kbd>
                  <span>Export PNG</span>
                </div>
                <div className="shortcut-row">
                  <kbd>Scroll</kbd>
                  <span>Zoom canvas</span>
                </div>
                <div className="shortcut-row">
                  <kbd>Drag</kbd>
                  <span>Pan canvas</span>
                </div>
              </div>
            </div>
          </div>
        </aside>

        <main className="content">
          <div className="canvas-area">
            {project.result && project.mesh ? (
              <div className="canvas-container">
                <ResultsCanvas mesh={project.mesh} result={project.result} />
              </div>
            ) : project.mesh ? (
              <div className="canvas-container">
                <MeshCanvas
                  mesh={project.mesh}
                  showGrid
                  showNodes
                  bcTool={activeBCTool}
                  dirichlet={project.dirichlet}
                  neumann={project.neumann}
                  selectedNode={null}
                  onNodeClick={handleNodeClick}
                />
              </div>
            ) : (
              <div className="canvas-placeholder">
                <div className="canvas-placeholder-icon">
                  <svg width="64" height="64" viewBox="0 0 64 64" fill="none">
                    <rect x="8" y="16" width="48" height="32" rx="2" stroke="currentColor" strokeWidth="2" strokeDasharray="4 2" opacity="0.4" />
                    <circle cx="32" cy="32" r="8" stroke="currentColor" strokeWidth="2" strokeDasharray="4 2" opacity="0.3" />
                    <path d="M24 32 L40 32 M32 24 L32 40" stroke="currentColor" strokeWidth="2" opacity="0.5" />
                  </svg>
                </div>
                <div className="canvas-placeholder-text">
                  Draw geometry to define your structural domain
                </div>
                <div className="canvas-placeholder-steps">
                  <div className="placeholder-step">
                    <span className="step-number">1</span>
                    <span>Add shapes in the Geometry panel</span>
                  </div>
                  <div className="placeholder-step">
                    <span className="step-number">2</span>
                    <span>Generate mesh and assign boundary conditions</span>
                  </div>
                  <div className="placeholder-step">
                    <span className="step-number">3</span>
                    <span>Run solver and visualize results</span>
                  </div>
                </div>
                <div className="canvas-placeholder-presets">
                  <span className="presets-label">Quick Start:</span>
                  <button
                    className="preset-btn"
                    onClick={() => {
                      const preset = {
                        id: '1',
                        type: 'rectangle' as const,
                        name: 'Cantilever',
                        x: Math.round(project.nx * 0.1),
                        y: Math.round(project.ny * 0.25),
                        width: Math.round(project.nx * 0.8),
                        height: Math.round(project.ny * 0.5),
                      };
                      setProject((prev) => ({ ...prev, shapes: [preset] }));
                      setShapesDirty(true);
                      setIsDirty(true);
                    }}
                  >
                    Cantilever
                  </button>
                  <button
                    className="preset-btn"
                    onClick={() => {
                      const preset = {
                        id: '1',
                        type: 'lbracket' as const,
                        name: 'L-Bracket',
                        x: Math.round(project.nx / 2),
                        y: Math.round(project.ny / 2),
                        width: Math.round(project.nx * 0.6),
                        height: Math.round(project.ny * 0.8),
                        flange: Math.round(Math.min(project.nx, project.ny) * 0.1),
                      };
                      setProject((prev) => ({ ...prev, shapes: [preset] }));
                      setShapesDirty(true);
                      setIsDirty(true);
                    }}
                  >
                    L-Bracket
                  </button>
                  <button
                    className="preset-btn"
                    onClick={() => {
                      const shapes = [
                        {
                          id: '1',
                          type: 'rectangle' as const,
                          name: 'Plate',
                          x: 0,
                          y: 0,
                          width: project.nx,
                          height: project.ny,
                        },
                        {
                          id: '2',
                          type: 'circle' as const,
                          name: 'Hole',
                          x: Math.round(project.nx / 2),
                          y: Math.round(project.ny / 2),
                          radius: Math.round(Math.min(project.nx, project.ny) * 0.2),
                        },
                      ];
                      setProject((prev) => ({ ...prev, shapes }));
                      setShapesDirty(true);
                      setIsDirty(true);
                    }}
                  >
                    Plate+Hole
                  </button>
                </div>
                {shapesDirty && project.shapes.length > 0 && (
                  <button
                    className="btn-primary"
                    onClick={handleGenerateMesh}
                    disabled={meshGenerating}
                    style={{ marginTop: 12, width: 'auto', padding: '8px 24px' }}
                  >
                    {meshGenerating ? 'Generating...' : 'Generate Mesh'}
                  </button>
                )}
              </div>
            )}
          </div>

          {/* Convergence chart overlay when sweep data exists */}
          {sweepResults.length > 0 && (
            <div className="sweep-chart-overlay">
              <ConvergenceChart
                results={sweepResults}
                title="Parameter Sweep: Max Displacement vs Mesh Density"
              />
            </div>
          )}
        </main>
      </div>

      {/* Status Bar */}
      <div className="status-bar">
        <div className="status-item">
          <div
            className={`status-dot ${isSolving ? 'running' : project.result ? '' : 'idle'}`}
          />
          <span>
            {isSolving
              ? 'Solving...'
              : meshGenerating
                ? 'Generating mesh...'
                : project.result
                  ? 'Solved'
                  : 'Ready'}
          </span>
        </div>
        {activeBCTool && (
          <>
            <div className="status-separator" />
            <div className="status-item status-tool">
              <span className="status-tool-badge">
                {activeBCTool.replace(/_/g, ' ').replace('fixed', 'Fix').replace('force', 'F').replace('roller', 'Roll')}
              </span>
              <span>Click a node to apply</span>
            </div>
          </>
        )}
        <div className="status-separator" />
        <div className="status-item">
          <span>Nodes: {project.mesh?.num_nodes ?? 0}</span>
        </div>
        <div className="status-item">
          <span>Elements: {project.mesh?.num_elements ?? 0}</span>
        </div>
        <div className="status-item">
          <span>DOFs: {project.mesh?.num_dofs ?? 0}</span>
        </div>
        <div className="status-separator" />
        <div className="status-item">
          <span>
            {elemTypeLabel(project.elemType)} | {project.nx}x{project.ny}
          </span>
        </div>
        <div className="status-separator" />
        <div className="status-item">
          <span>
            {project.planeType === 'stress' ? 'Plane Stress' : 'Plane Strain'}
          </span>
        </div>
        {solverTimeMs !== null && (
          <>
            <div className="status-separator" />
            <div className="status-item">
              <span className="status-solve-time">
                {solverTimeMs.toFixed(0)} ms
              </span>
            </div>
          </>
        )}
        {project.result?.cg_iterations != null && project.result.cg_iterations > 0 && (
          <>
            <div className="status-separator" />
            <div className="status-item">
              <span>{project.result.cg_iterations} CG iters</span>
            </div>
          </>
        )}
      </div>
    </div>
  );
}

export default App;
