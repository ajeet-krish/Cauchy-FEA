import type { SolveResult } from '../types';

interface SolverPanelProps {
  planeType: 'stress' | 'strain';
  solverType: 'cg' | 'cholesky';
  onPlaneTypeChange: (planeType: 'stress' | 'strain') => void;
  onSolverTypeChange: (solverType: 'cg' | 'cholesky') => void;
  onSolve: () => void;
  result: SolveResult | null;
  isSolving: boolean;
  solveError: string | null;
}

function SolverPanel({
  planeType,
  solverType,
  onPlaneTypeChange,
  onSolverTypeChange,
  onSolve,
  result,
  isSolving,
  solveError,
}: SolverPanelProps) {
  return (
    <div>
      <div className="form-group">
        <label>Assumption</label>
        <select
          value={planeType}
          onChange={(e) => onPlaneTypeChange(e.target.value as 'stress' | 'strain')}
          disabled={isSolving}
        >
          <option value="stress">Plane Stress</option>
          <option value="strain">Plane Strain</option>
        </select>
      </div>

      <div className="form-group">
        <label>Solver</label>
        <select
          value={solverType}
          onChange={(e) => onSolverTypeChange(e.target.value as 'cg' | 'cholesky')}
          disabled={isSolving}
        >
          <option value="cg">Conjugate Gradient</option>
          <option value="cholesky">Cholesky Direct</option>
        </select>
      </div>

      <button
        className="btn-primary"
        onClick={onSolve}
        disabled={isSolving}
      >
        {isSolving ? (
          <span className="solve-loading">
            <span className="spinner" />
            Solving...
          </span>
        ) : (
          'Solve'
        )}
      </button>

      {solveError && (
        <div className="solve-error">
          <span className="solve-error-icon">&#10007;</span>
          <span>{solveError}</span>
        </div>
      )}

      {result && !isSolving && (
        <div className={`solve-status ${result.cg_converged ? 'success' : 'error'}`}>
          <span>{result.cg_converged ? '\u2713' : '\u2717'}</span>
          <span>
            {result.cg_converged ? 'Converged' : 'Did not converge'} in{' '}
            {result.cg_iterations} iterations
          </span>
        </div>
      )}
    </div>
  );
}

export default SolverPanel;
