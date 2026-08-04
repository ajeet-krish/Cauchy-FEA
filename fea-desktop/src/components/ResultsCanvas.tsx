import type { MeshData, SolveResult } from '../types';

interface ResultsCanvasProps {
  mesh: MeshData;
  result: SolveResult;
}

function ResultsCanvas({ mesh: _mesh, result }: ResultsCanvasProps) {
  return (
    <div className="canvas-placeholder">
      <div className="canvas-placeholder-text">
        Results: {result.num_elements} elements, max stress {(result.max_stress / 1e6).toFixed(1)} MPa
      </div>
      <div className="canvas-placeholder-hint">
        Contour visualization will be implemented in Phase 5
      </div>
    </div>
  );
}

export default ResultsCanvas;
