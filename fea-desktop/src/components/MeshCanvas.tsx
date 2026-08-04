import type { MeshData } from '../types';

interface MeshCanvasProps {
  mesh: MeshData;
}

function MeshCanvas({ mesh }: MeshCanvasProps) {
  return (
    <div className="canvas-placeholder">
      <div className="canvas-placeholder-text">
        Mesh: {mesh.num_nodes} nodes, {mesh.num_elements} elements
      </div>
      <div className="canvas-placeholder-hint">
        Mesh rendering will be implemented in Phase 3
      </div>
    </div>
  );
}

export default MeshCanvas;
