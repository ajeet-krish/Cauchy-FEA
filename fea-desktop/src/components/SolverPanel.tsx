import type { SolveResult } from '../types';

interface SolverPanelProps {
  planeType: 'stress' | 'strain';
  onPlaneTypeChange: (planeType: 'stress' | 'strain') => void;
  onSolve: () => void;
  result: SolveResult | null;
}

function SolverPanel({
  planeType,
  onPlaneTypeChange,
  onSolve,
  result,
}: SolverPanelProps) {
  return (
    <div>
      <div className="form-group">
        <label>Assumption</label>
        <select
          value={planeType}
          onChange={(e) => onPlaneTypeChange(e.target.value as 'stress' | 'strain')}
        >
          <option value="stress">Plane Stress</option>
          <option value="strain">Plane Strain</option>
        </select>
      </div>

      <div className="form-group">
        <label>Solver</label>
        <select defaultValue="CG">
          <option value="CG">Conjugate Gradient</option>
          <option value="Cholesky">Cholesky Direct</option>
        </select>
      </div>

      <button className="btn-primary" onClick={onSolve}>
        Solve
      </button>

      {result && (
        <div className={`solve-status ${result.cg_converged ? 'success' : 'error'}`}>
          <span>{result.cg_converged ? '&#10003;' : '&#10007;'}</span>
          <span>
            {result.cg_converged ? 'Converged' : 'Did not converge'} in {result.cg_iterations} iterations
          </span>
        </div>
      )}
    </div>
  );
}

export default SolverPanel;
