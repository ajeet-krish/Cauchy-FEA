import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  SweepType,
  SweepConfig,
  SweepResult,
  Material,
  MeshData,
  DirichletBC,
  NeumannBC,
} from '../types';
import ConvergenceChart from './ConvergenceChart';

interface ParameterSweepProps {
  mesh: MeshData | null;
  dirichlet: DirichletBC[];
  neumann: NeumannBC[];
  material: Material;
  planeType: 'stress' | 'strain';
  nx: number;
  ny: number;
  elemType: number;
  shapesJson: string;
  onSweepComplete: (results: SweepResult[]) => void;
}

const DEFAULT_CONFIG: SweepConfig = {
  sweepType: 'mesh_convergence',
  meshStartN: 8,
  meshEndN: 64,
  meshSteps: 5,
  materialProperty: 'E',
  materialStart: 69,
  materialEnd: 200,
  materialSteps: 5,
  loadStart: 500,
  loadEnd: 5000,
  loadSteps: 5,
};

function ParameterSweep({
  mesh,
  dirichlet,
  neumann,
  material,
  planeType,
  nx,
  ny,
  elemType,
  shapesJson,
  onSweepComplete,
}: ParameterSweepProps) {
  const [config, setConfig] = useState<SweepConfig>(DEFAULT_CONFIG);
  const [isRunning, setIsRunning] = useState(false);
  const [progress, setProgress] = useState({ current: 0, total: 0 });
  const [results, setResults] = useState<SweepResult[]>([]);
  const [error, setError] = useState<string | null>(null);

  const generateMesh = useCallback(
    async (targetNx: number, targetNy: number) => {
      const meshJson = await invoke('generate_mesh', {
        shapesJson,
        nx: targetNx,
        ny: targetNy,
        elemType,
      });
      return meshJson as MeshData;
    },
    [shapesJson, elemType],
  );

  const runSolver = useCallback(
    async (meshData: MeshData, overrides?: Partial<Material>) => {
      const mat = overrides ? { ...material, ...overrides } : material;
      const meshWithMat = { ...meshData, material: mat, plane: planeType };
      const configJson = JSON.stringify({ planeType, solverType: 'cg' });
      const result = await invoke('run_fea_solve', {
        meshJson: JSON.stringify(meshWithMat),
        configJson,
      });
      return result as Record<string, unknown>;
    },
    [material, planeType],
  );

  const runMeshConvergence = useCallback(async () => {
    const steps = config.meshSteps;
    const startN = config.meshStartN;
    const endN = config.meshEndN;
    const nValues = Array.from({ length: steps }, (_, i) =>
      Math.round(startN + (i / (steps - 1)) * (endN - startN)),
    );

    setProgress({ current: 0, total: steps });
    const sweepResults: SweepResult[] = [];

    for (let i = 0; i < nValues.length; i++) {
      const n = nValues[i];
      setProgress({ current: i + 1, total: steps });

      try {
        const meshData = await generateMesh(n, n);
        const parsed = await runSolver(meshData);

        if (!parsed?.displacements || parsed.max_displacement == null) {
          continue;
        }

        sweepResults.push({
          nx: n,
          ny: n,
          maxDisplacement: parsed.max_displacement as number,
          maxStress: (parsed.max_stress as number) ?? 0,
          solveTimeMs: (parsed.solve_time_ms as number) ?? 0,
          cgIterations: (parsed.cg_iterations as number) ?? 0,
        });
      } catch (err) {
        console.error(`Sweep step ${i} failed:`, err);
      }
    }

    setResults(sweepResults);
    onSweepComplete(sweepResults);
    return sweepResults;
  }, [config, generateMesh, runSolver, onSweepComplete]);

  const runMaterialStudy = useCallback(async () => {
    if (!mesh) {
      setError('Generate a mesh first before running a material study.');
      return [];
    }

    const steps = config.materialSteps;
    const start = config.materialStart;
    const end = config.materialEnd;
    const prop = config.materialProperty;
    const values = Array.from({ length: steps }, (_, i) =>
      start + (i / (steps - 1)) * (end - start),
    );

    setProgress({ current: 0, total: steps });
    const sweepResults: SweepResult[] = [];

    for (let i = 0; i < values.length; i++) {
      const val = values[i];
      setProgress({ current: i + 1, total: steps });

      try {
        const overrides: Partial<Material> = {};
        if (prop === 'E') overrides.E = val * 1e9;
        else if (prop === 'nu') overrides.nu = val;
        else overrides.rho = val;

        const parsed = await runSolver(mesh, overrides);

        if (!parsed?.displacements || parsed.max_displacement == null) {
          continue;
        }

        sweepResults.push({
          nx,
          ny,
          maxDisplacement: parsed.max_displacement as number,
          maxStress: (parsed.max_stress as number) ?? 0,
          solveTimeMs: (parsed.solve_time_ms as number) ?? 0,
          cgIterations: (parsed.cg_iterations as number) ?? 0,
        });
      } catch (err) {
        console.error(`Material study step ${i} failed:`, err);
      }
    }

    setResults(sweepResults);
    onSweepComplete(sweepResults);
    return sweepResults;
  }, [config, mesh, nx, ny, runSolver, onSweepComplete]);

  const runLoadStudy = useCallback(async () => {
    if (!mesh) {
      setError('Generate a mesh first before running a load study.');
      return [];
    }
    if (neumann.length === 0) {
      setError('Add at least one force boundary condition for a load study.');
      return [];
    }

    const steps = config.loadSteps;
    const start = config.loadStart;
    const end = config.loadEnd;
    const values = Array.from({ length: steps }, (_, i) =>
      start + (i / (steps - 1)) * (end - start),
    );

    setProgress({ current: 0, total: steps });
    const sweepResults: SweepResult[] = [];

    for (let i = 0; i < values.length; i++) {
      const force = values[i];
      setProgress({ current: i + 1, total: steps });

      try {
        const scaledNeumann = neumann.map((bc) => ({
          ...bc,
          value: bc.value > 0 ? force : -force,
        }));
        const meshWithBC = {
          ...mesh,
          dirichlet,
          neumann: scaledNeumann,
          material,
          plane: planeType,
        };
        const configJson = JSON.stringify({ planeType, solverType: 'cg' });
        const parsed = await invoke('run_fea_solve', {
          meshJson: JSON.stringify(meshWithBC),
          configJson,
        });

        const result = parsed as Record<string, unknown>;
        if (!result?.displacements || result.max_displacement == null) {
          continue;
        }

        sweepResults.push({
          nx,
          ny,
          maxDisplacement: result.max_displacement as number,
          maxStress: (result.max_stress as number) ?? 0,
          solveTimeMs: (result.solve_time_ms as number) ?? 0,
          cgIterations: (result.cg_iterations as number) ?? 0,
        });
      } catch (err) {
        console.error(`Load study step ${i} failed:`, err);
      }
    }

    setResults(sweepResults);
    onSweepComplete(sweepResults);
    return sweepResults;
  }, [config, mesh, dirichlet, neumann, material, planeType, nx, ny, onSweepComplete]);

  const handleRunSweep = useCallback(async () => {
    setError(null);
    setIsRunning(true);
    setResults([]);

    try {
      if (config.sweepType === 'mesh_convergence') {
        await runMeshConvergence();
      } else if (config.sweepType === 'material_study') {
        await runMaterialStudy();
      } else {
        await runLoadStudy();
      }
    } catch (err) {
      console.error('Sweep failed:', err);
      setError('Sweep failed. Check configuration and try again.');
    } finally {
      setIsRunning(false);
    }
  }, [config, runMeshConvergence, runMaterialStudy, runLoadStudy]);

  const progressPct = progress.total > 0 ? (progress.current / progress.total) * 100 : 0;

  const getSweepLabel = (type: SweepType) => {
    if (type === 'mesh_convergence') return 'Mesh Convergence';
    if (type === 'material_study') return 'Material Study';
    return 'Load Study';
  };

  return (
    <div className="sweep-panel">
      {/* Sweep type selector */}
      <div className="form-group">
        <label>Sweep Type</label>
        <select
          value={config.sweepType}
          onChange={(e) =>
            setConfig((prev) => ({ ...prev, sweepType: e.target.value as SweepType }))
          }
          disabled={isRunning}
        >
          <option value="mesh_convergence">Mesh Convergence</option>
          <option value="material_study">Material Study</option>
          <option value="load_study">Load Study</option>
        </select>
      </div>

      {/* Mesh convergence config */}
      {config.sweepType === 'mesh_convergence' && (
        <div className="sweep-config">
          <div className="form-row">
            <div className="form-group">
              <label>Start nx</label>
              <input
                type="number"
                min="2"
                max="200"
                value={config.meshStartN}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    meshStartN: Math.max(2, parseInt(e.target.value, 10) || 2),
                  }))
                }
                disabled={isRunning}
              />
            </div>
            <div className="form-group">
              <label>End nx</label>
              <input
                type="number"
                min="2"
                max="200"
                value={config.meshEndN}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    meshEndN: Math.max(2, parseInt(e.target.value, 10) || 2),
                  }))
                }
                disabled={isRunning}
              />
            </div>
          </div>
          <div className="form-group">
            <label>Steps</label>
            <input
              type="number"
              min="2"
              max="20"
              value={config.meshSteps}
              onChange={(e) =>
                setConfig((prev) => ({
                  ...prev,
                  meshSteps: Math.max(2, parseInt(e.target.value, 10) || 2),
                }))
              }
              disabled={isRunning}
            />
          </div>
        </div>
      )}

      {/* Material study config */}
      {config.sweepType === 'material_study' && (
        <div className="sweep-config">
          <div className="form-group">
            <label>Property</label>
            <select
              value={config.materialProperty}
              onChange={(e) =>
                setConfig((prev) => ({
                  ...prev,
                  materialProperty: e.target.value as 'E' | 'nu' | 'rho',
                }))
              }
              disabled={isRunning}
            >
              <option value="E">Elastic Modulus (E, GPa)</option>
              <option value="nu">Poisson Ratio (nu)</option>
              <option value="rho">Density (rho, kg/m3)</option>
            </select>
          </div>
          <div className="form-row">
            <div className="form-group">
              <label>Start</label>
              <input
                type="number"
                step="0.1"
                value={config.materialStart}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    materialStart: parseFloat(e.target.value) || 0,
                  }))
                }
                disabled={isRunning}
              />
            </div>
            <div className="form-group">
              <label>End</label>
              <input
                type="number"
                step="0.1"
                value={config.materialEnd}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    materialEnd: parseFloat(e.target.value) || 0,
                  }))
                }
                disabled={isRunning}
              />
            </div>
          </div>
          <div className="form-group">
            <label>Steps</label>
            <input
              type="number"
              min="2"
              max="20"
              value={config.materialSteps}
              onChange={(e) =>
                setConfig((prev) => ({
                  ...prev,
                  materialSteps: Math.max(2, parseInt(e.target.value, 10) || 2),
                }))
              }
              disabled={isRunning}
            />
          </div>
        </div>
      )}

      {/* Load study config */}
      {config.sweepType === 'load_study' && (
        <div className="sweep-config">
          <div className="form-row">
            <div className="form-group">
              <label>Start Force (N)</label>
              <input
                type="number"
                step="100"
                value={config.loadStart}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    loadStart: parseFloat(e.target.value) || 0,
                  }))
                }
                disabled={isRunning}
              />
            </div>
            <div className="form-group">
              <label>End Force (N)</label>
              <input
                type="number"
                step="100"
                value={config.loadEnd}
                onChange={(e) =>
                  setConfig((prev) => ({
                    ...prev,
                    loadEnd: parseFloat(e.target.value) || 0,
                  }))
                }
                disabled={isRunning}
              />
            </div>
          </div>
          <div className="form-group">
            <label>Steps</label>
            <input
              type="number"
              min="2"
              max="20"
              value={config.loadSteps}
              onChange={(e) =>
                setConfig((prev) => ({
                  ...prev,
                  loadSteps: Math.max(2, parseInt(e.target.value, 10) || 2),
                }))
              }
              disabled={isRunning}
            />
          </div>
        </div>
      )}

      {/* Run button */}
      <button
        className="btn-primary"
        onClick={handleRunSweep}
        disabled={isRunning}
      >
        {isRunning ? (
          <span className="solve-loading">
            <span className="spinner" />
            Running {getSweepLabel(config.sweepType)}...
          </span>
        ) : (
          `Run ${getSweepLabel(config.sweepType)}`
        )}
      </button>

      {/* Progress bar */}
      {isRunning && progress.total > 0 && (
        <div className="sweep-progress">
          <div className="sweep-progress-bar">
            <div
              className="sweep-progress-fill"
              style={{ width: `${progressPct}%` }}
            />
          </div>
          <span className="sweep-progress-label">
            {progress.current} / {progress.total}
          </span>
        </div>
      )}

      {/* Error */}
      {error && (
        <div className="solve-error">
          <span className="solve-error-icon">&#10007;</span>
          <span>{error}</span>
        </div>
      )}

      {/* Results table */}
      {results.length > 0 && !isRunning && (
        <div className="sweep-results">
          <h3>Results</h3>
          <table>
            <thead>
              <tr>
                <th>nx</th>
                <th>Max Disp. (m)</th>
                <th>Max Stress (MPa)</th>
                <th>Time (ms)</th>
                <th>CG Iters</th>
              </tr>
            </thead>
            <tbody>
              {results.map((r, i) => (
                <tr key={i}>
                  <td>{r.nx}</td>
                  <td>{r.maxDisplacement.toExponential(3)}</td>
                  <td>{(r.maxStress / 1e6).toFixed(1)}</td>
                  <td>{r.solveTimeMs.toFixed(0)}</td>
                  <td>{r.cgIterations}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Convergence chart for mesh convergence */}
      {results.length > 0 && !isRunning && config.sweepType === 'mesh_convergence' && (
        <div className="sweep-chart-area">
          <ConvergenceChart
            results={results}
            title="Mesh Convergence: Displacement vs Density"
          />
        </div>
      )}
    </div>
  );
}

export default ParameterSweep;
