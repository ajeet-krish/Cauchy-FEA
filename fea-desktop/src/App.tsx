import { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type { ProjectState, SolveResult, Shape, Material, DirichletBC, NeumannBC, BCTool } from './types';
import GeometryEditor from './components/GeometryEditor';
import BCLoadEditor from './components/BCLoadEditor';
import MeshCanvas from './components/MeshCanvas';
import ResultsCanvas from './components/ResultsCanvas';
import SolverPanel from './components/SolverPanel';
import ToolBar from './components/ToolBar';

const DEFAULT_MATERIAL: Material = {
  E: 200e9,
  nu: 0.3,
  rho: 7850,
  t: 0.01,
};

function App() {
  const [project, setProject] = useState<ProjectState>({
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
  });

  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const [meshGenerating, setMeshGenerating] = useState(false);
  const [meshError, setMeshError] = useState<string | null>(null);
  const [isSolving, setIsSolving] = useState(false);
  const [solveError, setSolveError] = useState<string | null>(null);
  const [shapesDirty, setShapesDirty] = useState(false);
  const [activeBCTool, setActiveBCTool] = useState<BCTool>(null);

  const togglePanel = (name: string) => {
    setCollapsed((prev) => ({ ...prev, [name]: !prev[name] }));
  };

  const handleShapesChange = (shapes: Shape[]) => {
    setProject((prev) => ({ ...prev, shapes, mesh: null, result: null }));
    setShapesDirty(true);
  };

  const handleMaterialChange = (material: Material) => {
    if (!Number.isFinite(material.E) || material.E <= 0) return;
    if (!Number.isFinite(material.nu) || material.nu <= 0 || material.nu >= 0.5) return;
    if (!Number.isFinite(material.rho) || material.rho <= 0) return;
    if (!Number.isFinite(material.t) || material.t <= 0) return;
    setProject((prev) => ({ ...prev, material }));
  };

  const handleNxChange = (nx: number) => {
    const clamped = Math.max(2, Math.min(200, Math.floor(nx) || 2));
    setProject((prev) => ({ ...prev, nx: clamped }));
  };

  const handleNyChange = (ny: number) => {
    const clamped = Math.max(2, Math.min(200, Math.floor(ny) || 2));
    setProject((prev) => ({ ...prev, ny: clamped }));
  };

  const handleElemTypeChange = (elemType: number) => {
    setProject((prev) => ({ ...prev, elemType }));
  };

  const handlePlaneTypeChange = (planeType: 'stress' | 'strain') => {
    setProject((prev) => ({ ...prev, planeType }));
  };

  const handleSolverTypeChange = (solverType: 'cg' | 'cholesky') => {
    setProject((prev) => ({ ...prev, solverType }));
  };

  const handleDirichletChange = (dirichlet: DirichletBC[]) => {
    setProject((prev) => ({ ...prev, dirichlet }));
  };

  const handleNeumannChange = (neumann: NeumannBC[]) => {
    setProject((prev) => ({ ...prev, neumann }));
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
    try {
      const configJson = JSON.stringify({
        planeType: project.planeType,
        solverType: project.solverType,
      });
      const result = await invoke('run_fea_solve', {
        meshJson: JSON.stringify(project.mesh),
        configJson,
      });

      // Runtime validation before type assertion
      const parsed = result as Record<string, unknown>;
      if (!parsed?.displacements || !parsed?.stresses || parsed.max_displacement == null) {
        setSolveError('Invalid solver response.');
        return;
      }
      setProject((prev) => ({ ...prev, result: parsed as unknown as SolveResult }));
    } catch (err) {
      console.error('Solve failed:', err);
      setSolveError('Solver failed. Check mesh and boundary conditions.');
    } finally {
      setIsSolving(false);
    }
  };

  const handleNew = () => {
    setProject({
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
    });
    setShapesDirty(false);
  };

  const handleOpen = () => {
    console.log('Open project');
  };

  const handleSave = () => {
    console.log('Save project');
  };

  const handleExportPng = () => {
    console.log('Export PNG');
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
        />
      </header>

      <div className="main-layout">
        <aside className="sidebar">
          {/* Geometry Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('geometry')}>
              <h2>Geometry</h2>
              <span className={`panel-toggle ${collapsed['geometry'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['geometry'] ? 'collapsed' : ''}`}>
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
            <div className="panel-header" onClick={() => togglePanel('material')}>
              <h2>Material</h2>
              <span className={`panel-toggle ${collapsed['material'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['material'] ? 'collapsed' : ''}`}>
              <div className="form-group">
                <label>Preset</label>
                <select
                  value={`${project.material.E}-${project.material.nu}`}
                  onChange={(e) => {
                    const presets: Record<string, Material> = {
                      '200e9-0.3': { E: 200e9, nu: 0.3, rho: 7850, t: 0.01 },
                      '70e9-0.33': { E: 70e9, nu: 0.33, rho: 2700, t: 0.01 },
                      '110e9-0.34': { E: 110e9, nu: 0.34, rho: 4500, t: 0.01 },
                    };
                    const mat = presets[e.target.value];
                    if (mat) handleMaterialChange(mat);
                  }}
                >
                  <option value="200e9-0.3">Steel (E=200 GPa)</option>
                  <option value="70e9-0.33">Aluminum (E=70 GPa)</option>
                  <option value="110e9-0.34">Titanium (E=110 GPa)</option>
                  <option value="custom">Custom</option>
                </select>
              </div>

              <div className="form-row">
                <div className="form-group">
                  <label>E (Pa)</label>
                  <input
                    type="number"
                    value={project.material.E}
                    onChange={(e) =>
                      handleMaterialChange({ ...project.material, E: +e.target.value })
                    }
                  />
                </div>
                <div className="form-group">
                  <label>Poisson</label>
                  <input
                    type="number"
                    step="0.01"
                    value={project.material.nu}
                    onChange={(e) =>
                      handleMaterialChange({ ...project.material, nu: +e.target.value })
                    }
                  />
                </div>
              </div>

              <div className="form-row">
                <div className="form-group">
                  <label>Density (kg/m3)</label>
                  <input
                    type="number"
                    value={project.material.rho}
                    onChange={(e) =>
                      handleMaterialChange({ ...project.material, rho: +e.target.value })
                    }
                  />
                </div>
                <div className="form-group">
                  <label>Thickness (m)</label>
                  <input
                    type="number"
                    step="0.001"
                    value={project.material.t}
                    onChange={(e) =>
                      handleMaterialChange({ ...project.material, t: +e.target.value })
                    }
                  />
                </div>
              </div>
            </div>
          </div>

          {/* Mesh Panel */}
          <div className="panel">
            <div className="panel-header" onClick={() => togglePanel('mesh')}>
              <h2>Mesh</h2>
              <span className={`panel-toggle ${collapsed['mesh'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['mesh'] ? 'collapsed' : ''}`}>
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
                  <span>Nodes: <span className="value">{project.mesh.num_nodes}</span></span>
                  <span>Elements: <span className="value">{project.mesh.num_elements}</span></span>
                  <span>DOFs: <span className="value">{project.mesh.num_dofs}</span></span>
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
              <span className={`panel-toggle ${collapsed['bc'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['bc'] ? 'collapsed' : ''}`}>
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
              <span className={`panel-toggle ${collapsed['solver'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['solver'] ? 'collapsed' : ''}`}>
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
              <span className={`panel-toggle ${collapsed['results'] ? 'collapsed' : ''}`}>
                &#9660;
              </span>
            </div>
            <div className={`panel-body ${collapsed['results'] ? 'collapsed' : ''}`}>
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
                    <div className="value">
                      {project.result.cg_iterations}
                    </div>
                  </div>
                </div>
              ) : (
                <div className="placeholder">No results yet. Run solver.</div>
              )}
            </div>
          </div>
        </aside>

        <main className="content">
          <div className="canvas-area">
            {project.result && project.mesh ? (
              <div className="canvas-container">
                <ResultsCanvas
                  mesh={project.mesh}
                  result={project.result}
                />
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
                <div className="canvas-placeholder-icon">&#9632;</div>
                <div className="canvas-placeholder-text">
                  Draw geometry to define your structural domain
                </div>
                <div className="canvas-placeholder-hint">
                  Use the Geometry panel to add rectangles, circles, I-beams, or L-brackets
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
        </main>
      </div>

      {/* Status Bar */}
      <div className="status-bar">
        <div className="status-item">
          <div className={`status-dot ${isSolving ? 'running' : project.result ? '' : 'idle'}`} />
          <span>
            {isSolving
              ? 'Solving...'
              : project.result
                ? 'Solved'
                : meshGenerating
                  ? 'Generating...'
                  : 'Ready'}
          </span>
        </div>
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
          <span>{elemTypeLabel(project.elemType)} | {project.nx}x{project.ny}</span>
        </div>
        <div className="status-separator" />
        <div className="status-item">
          <span>{project.planeType === 'stress' ? 'Plane Stress' : 'Plane Strain'}</span>
        </div>
      </div>
    </div>
  );
}

export default App;
