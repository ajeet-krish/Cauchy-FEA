import { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type { ProjectState, Shape, Material, DirichletBC, NeumannBC } from './types';
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
    result: null,
    nx: 16,
    ny: 8,
    elemType: 0,
  });

  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const [meshGenerating, setMeshGenerating] = useState(false);
  const [shapesDirty, setShapesDirty] = useState(false);

  const togglePanel = (name: string) => {
    setCollapsed((prev) => ({ ...prev, [name]: !prev[name] }));
  };

  const handleShapesChange = (shapes: Shape[]) => {
    setProject((prev) => ({ ...prev, shapes, mesh: null, result: null }));
    setShapesDirty(true);
  };

  const handleMaterialChange = (material: Material) => {
    setProject((prev) => ({ ...prev, material }));
  };

  const handleNxChange = (nx: number) => {
    setProject((prev) => ({ ...prev, nx }));
  };

  const handleNyChange = (ny: number) => {
    setProject((prev) => ({ ...prev, ny }));
  };

  const handleElemTypeChange = (elemType: number) => {
    setProject((prev) => ({ ...prev, elemType }));
  };

  const handlePlaneTypeChange = (planeType: 'stress' | 'strain') => {
    setProject((prev) => ({ ...prev, planeType }));
  };

  const handleDirichletChange = (dirichlet: DirichletBC[]) => {
    setProject((prev) => ({ ...prev, dirichlet }));
  };

  const handleNeumannChange = (neumann: NeumannBC[]) => {
    setProject((prev) => ({ ...prev, neumann }));
  };

  const handleGenerateMesh = async () => {
    if (project.shapes.length === 0) return;
    setMeshGenerating(true);
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
    } finally {
      setMeshGenerating(false);
    }
  };

  const handleSolve = async () => {
    if (!project.mesh) return;
    try {
      const result = await invoke('solve', {
        meshJson: JSON.stringify(project.mesh),
        materialJson: JSON.stringify(project.material),
        dirichletJson: JSON.stringify(project.dirichlet),
        neumannJson: JSON.stringify(project.neumann),
        planeType: project.planeType,
      });
      setProject((prev) => ({ ...prev, result: result as ProjectState['result'] }));
    } catch (err) {
      console.error('Solve failed:', err);
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
                dirichlet={project.dirichlet}
                neumann={project.neumann}
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
                onPlaneTypeChange={handlePlaneTypeChange}
                onSolve={handleSolve}
                result={project.result}
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
                <MeshCanvas mesh={project.mesh} showGrid showNodes />
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
          <div className={`status-dot ${project.result ? '' : 'idle'}`} />
          <span>{project.result ? 'Solved' : meshGenerating ? 'Generating...' : 'Ready'}</span>
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
