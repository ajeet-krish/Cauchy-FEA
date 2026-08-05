import { useState } from 'react';
import type { MeshData, DirichletBC, NeumannBC, BCTool } from '../types';

interface BCLoadEditorProps {
  mesh: MeshData | null;
  dirichlet: DirichletBC[];
  neumann: NeumannBC[];
  activeBCTool: BCTool | null;
  onBCToolChange: (tool: BCTool | null) => void;
  onDirichletChange: (bc: DirichletBC[]) => void;
  onNeumannChange: (bc: NeumannBC[]) => void;
}

const TOOL_LABELS: Record<string, string> = {
  fixed_ux_uy: 'Fixed (UX+UY)',
  fixed_ux: 'Fixed UX',
  fixed_uy: 'Fixed UY',
  roller_x: 'Roller X',
  roller_y: 'Roller Y',
  force_x: 'Force X',
  force_y: 'Force Y',
};

const TOOL_ICONS: Record<string, string> = {
  fixed_ux_uy: '\u25B2',
  fixed_ux: '\u25B2\u2500',
  fixed_uy: '\u25B2\u2502',
  roller_x: '\u25CB',
  roller_y: '\u25CB',
  force_x: '\u2192',
  force_y: '\u2191',
};

function BCLoadEditor({
  mesh,
  dirichlet,
  neumann,
  activeBCTool,
  onBCToolChange,
  onDirichletChange,
  onNeumannChange,
}: BCLoadEditorProps) {
  const [forceMagnitude, setForceMagnitude] = useState(-1000);

  // Group Dirichlet BCs by node for display
  const groupedDirichlet = new Map<number, DirichletBC[]>();
  for (const bc of dirichlet) {
    const existing = groupedDirichlet.get(bc.node) || [];
    existing.push(bc);
    groupedDirichlet.set(bc.node, existing);
  }

  const removeDirichletNode = (nodeIndex: number) => {
    onDirichletChange(dirichlet.filter((bc) => bc.node !== nodeIndex));
  };

  const removeNeumann = (index: number) => {
    onNeumannChange(neumann.filter((_, i) => i !== index));
  };

  const updateNeumannValue = (index: number, value: number) => {
    const updated = neumann.map((bc, i) =>
      i === index ? { ...bc, value } : bc,
    );
    onNeumannChange(updated);
  };

  const formatDOF = (dof: number) => (dof === 0 ? 'UX' : 'UY');

  const formatDirichletLabel = (bcs: DirichletBC[]) => {
    const dofs = bcs.map((bc) => formatDOF(bc.dof));
    return dofs.join('+');
  };

  // Generate cantilever BC preset
  const loadCantileverPreset = () => {
    if (!mesh || mesh.nodes.length === 0) return;

    const nodes = mesh.nodes;
    let minX = Infinity;
    let maxX = -Infinity;
    for (const n of nodes) {
      if (n.x < minX) minX = n.x;
      if (n.x > maxX) maxX = n.x;
    }

    const newDirichlet: DirichletBC[] = [];
    const newNeumann: NeumannBC[] = [];

    // Fixed left edge: all nodes with x == minX get UX+UY = 0
    for (let i = 0; i < nodes.length; i++) {
      if (Math.abs(nodes[i].x - minX) < 1e-10) {
        newDirichlet.push({ node: i, dof: 0, value: 0 });
        newDirichlet.push({ node: i, dof: 1, value: 0 });
      }
    }

    // Force on right edge: rightmost node gets UY = -1000
    let rightNode = 0;
    let rightX = -Infinity;
    for (let i = 0; i < nodes.length; i++) {
      if (nodes[i].x > rightX) {
        rightX = nodes[i].x;
        rightNode = i;
      }
    }
    // Find the midpoint node on the right edge (top corner)
    let topRightNode = rightNode;
    let topY = -Infinity;
    for (let i = 0; i < nodes.length; i++) {
      if (Math.abs(nodes[i].x - maxX) < 1e-10 && nodes[i].y > topY) {
        topY = nodes[i].y;
        topRightNode = i;
      }
    }
    newNeumann.push({ node: topRightNode, dof: 1, value: forceMagnitude });

    onDirichletChange(newDirichlet);
    onNeumannChange(newNeumann);
  };

  // Generate simply supported BC preset
  const loadSimplySupportedPreset = () => {
    if (!mesh || mesh.nodes.length === 0) return;

    const nodes = mesh.nodes;
    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    for (const n of nodes) {
      if (n.x < minX) minX = n.x;
      if (n.x > maxX) maxX = n.x;
      if (n.y < minY) minY = n.y;
      if (n.y > maxY) maxY = n.y;
    }

    const newDirichlet: DirichletBC[] = [];

    // Fixed bottom-left corner: UX+UY = 0
    for (let i = 0; i < nodes.length; i++) {
      if (
        Math.abs(nodes[i].x - minX) < 1e-10 &&
        Math.abs(nodes[i].y - minY) < 1e-10
      ) {
        newDirichlet.push({ node: i, dof: 0, value: 0 });
        newDirichlet.push({ node: i, dof: 1, value: 0 });
      }
    }

    // Roller on bottom-right corner: UY = 0 only
    for (let i = 0; i < nodes.length; i++) {
      if (
        Math.abs(nodes[i].x - maxX) < 1e-10 &&
        Math.abs(nodes[i].y - minY) < 1e-10
      ) {
        newDirichlet.push({ node: i, dof: 1, value: 0 });
      }
    }

    // Force on top midpoint
    let topMidNode = 0;
    let minDist = Infinity;
    const midX = (minX + maxX) / 2;
    for (let i = 0; i < nodes.length; i++) {
      if (Math.abs(nodes[i].y - maxY) < 1e-10) {
        const dist = Math.abs(nodes[i].x - midX);
        if (dist < minDist) {
          minDist = dist;
          topMidNode = i;
        }
      }
    }
    const newNeumann: NeumannBC[] = [
      { node: topMidNode, dof: 1, value: forceMagnitude },
    ];

    onDirichletChange(newDirichlet);
    onNeumannChange(newNeumann);
  };

  const toolHint = activeBCTool
    ? `Click a node on the mesh to apply ${TOOL_LABELS[activeBCTool]}`
    : 'Select a tool, then click nodes on the mesh';

  return (
    <div>
      {/* Tool buttons */}
      <div className="bc-tool-bar">
        <button
          className={`bc-tool-btn ${activeBCTool === 'fixed_ux_uy' ? 'active' : ''}`}
          onClick={() =>
            onBCToolChange(activeBCTool === 'fixed_ux_uy' ? null : 'fixed_ux_uy')
          }
          title="Fixed (UX+UY): constrain both DOFs to 0"
        >
          {TOOL_ICONS['fixed_ux_uy']} Fixed
        </button>
        <button
          className={`bc-tool-btn ${activeBCTool === 'roller_x' ? 'active' : ''} ${activeBCTool === 'roller_y' ? 'active' : ''}`}
          onClick={() =>
            onBCToolChange(
              activeBCTool === 'roller_x'
                ? null
                : activeBCTool === 'roller_y'
                  ? null
                  : 'roller_x',
            )
          }
          title="Roller: constrain one DOF, free in the other"
        >
          {TOOL_ICONS['roller_x']} Roller
        </button>
        <button
          className={`bc-tool-btn ${activeBCTool === 'force_x' ? 'active' : ''} ${activeBCTool === 'force_y' ? 'active' : ''}`}
          onClick={() =>
            onBCToolChange(
              activeBCTool === 'force_x'
                ? null
                : activeBCTool === 'force_y'
                  ? null
                  : 'force_x',
            )
          }
          title="Apply force in X or Y direction"
        >
          {TOOL_ICONS['force_x']} Force
        </button>
        <button
          className={`bc-tool-btn ${activeBCTool === null ? 'active' : ''}`}
          onClick={() => onBCToolChange(null)}
          title="Deselect tool (view mode)"
        >
          Select
        </button>
      </div>

      {/* Force direction and magnitude controls (shown when force tool active) */}
      {(activeBCTool === 'force_x' || activeBCTool === 'force_y') && (
        <div className="bc-force-controls">
          <div className="form-row">
            <div className="form-group">
              <label>Direction</label>
              <select
                value={activeBCTool}
                onChange={(e) =>
                  onBCToolChange(e.target.value as BCTool)
                }
              >
                <option value="force_x">X direction</option>
                <option value="force_y">Y direction</option>
              </select>
            </div>
            <div className="form-group">
              <label>Magnitude (N)</label>
              <input
                type="number"
                className="force-input"
                value={forceMagnitude}
                onChange={(e) => {
                  const v = +e.target.value;
                  if (Number.isFinite(v)) setForceMagnitude(v);
                }}
              />
            </div>
          </div>
        </div>
      )}

      {/* Roller direction controls (shown when roller tool active) */}
      {(activeBCTool === 'roller_x' || activeBCTool === 'roller_y') && (
        <div className="bc-force-controls">
          <div className="form-group">
            <label>Constrained DOF</label>
            <select
              value={activeBCTool}
              onChange={(e) =>
                onBCToolChange(e.target.value as BCTool)
              }
            >
              <option value="roller_x">Free X, Constrained Y</option>
              <option value="roller_y">Free Y, Constrained X</option>
            </select>
          </div>
        </div>
      )}

      {/* Fixed direction controls (shown when fixed UX or UY tool active) */}
      {(activeBCTool === 'fixed_ux' || activeBCTool === 'fixed_uy') && (
        <div className="bc-force-controls">
          <div className="form-group">
            <label>Constrained DOF</label>
            <select
              value={activeBCTool}
              onChange={(e) =>
                onBCToolChange(e.target.value as BCTool)
              }
            >
              <option value="fixed_ux">Fixed UX only</option>
              <option value="fixed_uy">Fixed UY only</option>
            </select>
          </div>
        </div>
      )}

      {/* Active tool hint */}
      <div className="bc-tool-hint">{toolHint}</div>

      {/* Presets */}
      <div className="bc-presets">
        <span className="preset-label">Presets:</span>
        <button
          className="preset-btn"
          onClick={loadCantileverPreset}
          disabled={!mesh || mesh.nodes.length === 0}
        >
          Cantilever
        </button>
        <button
          className="preset-btn"
          onClick={loadSimplySupportedPreset}
          disabled={!mesh || mesh.nodes.length === 0}
        >
          Simply Supported
        </button>
      </div>

      {/* Fixed Constraints List */}
      <div className="form-group" style={{ marginTop: 10 }}>
        <label>
          Fixed Constraints ({groupedDirichlet.size})
        </label>
        {groupedDirichlet.size > 0 ? (
          <div className="bc-list">
            {Array.from(groupedDirichlet.entries()).map(([node, bcs]) => (
              <div key={node} className="shape-item">
                <span className="shape-info">
                  Node {node}, {formatDirichletLabel(bcs)} = 0
                </span>
                <button
                  className="shape-delete"
                  onClick={() => removeDirichletNode(node)}
                  aria-label={`Remove constraint at node ${node}`}
                >
                  &#10005;
                </button>
              </div>
            ))}
          </div>
        ) : (
          <div className="placeholder">No fixed constraints</div>
        )}
      </div>

      {/* Applied Loads List */}
      <div className="form-group" style={{ marginTop: 10 }}>
        <label>
          Applied Loads ({neumann.length})
        </label>
        {neumann.length > 0 ? (
          <div className="bc-list">
            {neumann.map((bc, i) => (
              <div key={`${bc.node}-${bc.dof}-${i}`} className="shape-item bc-load-item">
                <span className="shape-info">
                  Node {bc.node}, {formatDOF(bc.dof)} =
                </span>
                <input
                  type="number"
                  className="force-input-inline"
                  value={bc.value}
                  onChange={(e) => updateNeumannValue(i, +e.target.value)}
                  onClick={(e) => e.stopPropagation()}
                  aria-label={`Force magnitude at node ${bc.node}`}
                />
                <span className="shape-info">N</span>
                <button
                  className="shape-delete"
                  onClick={() => removeNeumann(i)}
                  aria-label={`Remove load at node ${bc.node}`}
                >
                  &#10005;
                </button>
              </div>
            ))}
          </div>
        ) : (
          <div className="placeholder">No applied loads</div>
        )}
      </div>
    </div>
  );
}

export default BCLoadEditor;
