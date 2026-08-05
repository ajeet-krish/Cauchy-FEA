import { useRef, useEffect, useState, useCallback } from 'react';
import type { MeshData, SolveResult, Point } from '../types';
import {
  renderContours,
  drawColorbar,
  getFieldColormap,
  computeFieldRange,
  FIELD_LABELS,
  FIELD_UNITS,
  STRESS_FIELDS,
  DISPLACEMENT_FIELDS,
  type ContourField,
} from '../utils/contour-renderer';

const BG_COLOR = '#0a0e14';
const OVERLAY_BG = 'rgba(13, 17, 23, 0.88)';
const OVERLAY_BORDER = '#21262d';
const TEXT_MUTED = '#8b949e';

interface ResultsCanvasProps {
  mesh: MeshData;
  result: SolveResult;
  showDeformed?: boolean;
  deformationScale?: number;
}

function getBBox(nodes: Point[]): { x0: number; y0: number; x1: number; y1: number } {
  if (nodes.length === 0) return { x0: 0, y0: 0, x1: 1, y1: 1 };
  let x0 = Infinity;
  let y0 = Infinity;
  let x1 = -Infinity;
  let y1 = -Infinity;
  for (const n of nodes) {
    if (n.x < x0) x0 = n.x;
    if (n.y < y0) y0 = n.y;
    if (n.x > x1) x1 = n.x;
    if (n.y > y1) y1 = n.y;
  }
  const pad = Math.max(x1 - x0, y1 - y0) * 0.05 || 1;
  return { x0: x0 - pad, y0: y0 - pad, x1: x1 + pad, y1: y1 + pad };
}

export default function ResultsCanvas({
  mesh,
  result,
  showDeformed: initialDeformed = false,
  deformationScale: initialScale = 1.0,
}: ResultsCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // View state
  const [viewOffset, setViewOffset] = useState({ x: 0, y: 0 });
  const [viewScale, setViewScale] = useState(1);
  const [isPanning, setIsPanning] = useState(false);
  const [panStart, setPanStart] = useState<{
    px: number;
    py: number;
    ox: number;
    oy: number;
  } | null>(null);

  // Visualization state
  const [selectedField, setSelectedField] = useState<ContourField>('von_mises');
  const [showDeformed, setShowDeformed] = useState(initialDeformed);
  const [deformationScale, setDeformationScale] = useState(initialScale);
  const [showNodes, setShowNodes] = useState(false);

  // Compute deformed node positions
  const getDeformedNodes = useCallback((): Point[] | null => {
    if (!showDeformed) return null;
    return mesh.nodes.map((node, i) => {
      const d = result.displacements[i];
      if (!d) return node;
      return {
        x: node.x + d.ux * deformationScale,
        y: node.y + d.uy * deformationScale,
      };
    });
  }, [mesh, result, showDeformed, deformationScale]);

  // Compute transform to fit mesh in canvas
  const computeTransform = useCallback(
    (canvasW: number, canvasH: number) => {
      if (!mesh || mesh.nodes.length === 0) {
        return { offsetX: canvasW / 2, offsetY: canvasH / 2, scale: 1 };
      }
      const bbox = getBBox(mesh.nodes);
      const meshW = bbox.x1 - bbox.x0;
      const meshH = bbox.y1 - bbox.y0;
      if (meshW <= 0 || meshH <= 0) {
        return { offsetX: canvasW / 2, offsetY: canvasH / 2, scale: 1 };
      }
      const padding = 60;
      const scaleX = (canvasW - padding * 2) / meshW;
      const scaleY = (canvasH - padding * 2) / meshH;
      const baseScale = Math.min(scaleX, scaleY);
      const scale = baseScale * viewScale;
      const centerX = (bbox.x0 + bbox.x1) / 2;
      const centerY = (bbox.y0 + bbox.y1) / 2;
      const offsetX = canvasW / 2 - centerX * scale + viewOffset.x;
      const offsetY = canvasH / 2 - centerY * scale + viewOffset.y;
      return { offsetX, offsetY, scale };
    },
    [mesh, viewScale, viewOffset],
  );

  // Main draw function
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;

    // Clear
    ctx.fillStyle = BG_COLOR;
    ctx.fillRect(0, 0, w, h);

    if (!mesh || mesh.nodes.length === 0 || !result) {
      ctx.fillStyle = '#484f58';
      ctx.font = '13px JetBrains Mono, monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('No result data', w / 2, h / 2);
      return;
    }

    // Compute transform ONCE per frame, then create a closure for toScreen
    const { offsetX, offsetY, scale } = computeTransform(w, h);
    const toScreen = (pt: Point) => ({
      sx: offsetX + pt.x * scale,
      sy: offsetY + pt.y * scale,
    });

    const deformedNodes = getDeformedNodes();

    // Draw contour-filled elements -- pass pre-baked toScreen to avoid per-point recomputation
    renderContours(ctx, mesh, result, selectedField, toScreen, deformedNodes);

    // Draw colorbar
    const { min, max } = computeFieldRange(mesh, result, selectedField);
    const colormap = getFieldColormap(selectedField);
    drawColorbar(ctx, colormap, min, max, FIELD_UNITS[selectedField], w, h);

    // Draw node dots if enabled
    if (showNodes) {
      const nodeRadius = Math.max(1.5, Math.min(3, 2000 / mesh.nodes.length));
      const nodesToUse = deformedNodes ?? mesh.nodes;
      ctx.fillStyle = 'rgba(255, 179, 71, 0.7)';
      for (let i = 0; i < nodesToUse.length; i++) {
        const { sx, sy } = toScreen(nodesToUse[i]);
        ctx.beginPath();
        ctx.arc(sx, sy, nodeRadius, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // Info overlay (top-left)
    const fieldLabel = FIELD_LABELS[selectedField];
    const deformedLabel = showDeformed ? ` (scale: ${deformationScale.toFixed(1)}x)` : '';
    const infoLines = [
      `Field: ${fieldLabel}${deformedLabel}`,
      `Nodes: ${mesh.num_nodes}  Elements: ${mesh.num_elements}`,
    ];
    const infoWidth = 260;
    const infoHeight = 34;
    ctx.fillStyle = OVERLAY_BG;
    ctx.fillRect(8, 8, infoWidth, infoHeight);
    ctx.strokeStyle = OVERLAY_BORDER;
    ctx.lineWidth = 1;
    ctx.strokeRect(8, 8, infoWidth, infoHeight);
    ctx.fillStyle = TEXT_MUTED;
    ctx.font = '10px JetBrains Mono, monospace';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText(infoLines[0], 14, 14);
    ctx.fillText(infoLines[1], 14, 26);

    // Coordinate axes at bottom-left
    const axisLen = 40;
    const axisX = 30;
    const axisY = h - 30;
    // X axis
    ctx.strokeStyle = '#ff6b6b';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(axisX, axisY);
    ctx.lineTo(axisX + axisLen, axisY);
    ctx.stroke();
    ctx.fillStyle = '#ff6b6b';
    ctx.beginPath();
    ctx.moveTo(axisX + axisLen + 6, axisY);
    ctx.lineTo(axisX + axisLen - 2, axisY - 4);
    ctx.lineTo(axisX + axisLen - 2, axisY + 4);
    ctx.closePath();
    ctx.fill();
    ctx.font = '11px JetBrains Mono, monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillText('X', axisX + axisLen / 2, axisY + 6);

    // Y axis
    ctx.strokeStyle = '#51cf66';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(axisX, axisY);
    ctx.lineTo(axisX, axisY - axisLen);
    ctx.stroke();
    ctx.fillStyle = '#51cf66';
    ctx.beginPath();
    ctx.moveTo(axisX, axisY - axisLen - 6);
    ctx.lineTo(axisX - 4, axisY - axisLen + 2);
    ctx.lineTo(axisX + 4, axisY - axisLen + 2);
    ctx.closePath();
    ctx.fill();
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    ctx.fillText('Y', axisX - 8, axisY - axisLen / 2);
  }, [
    mesh,
    result,
    selectedField,
    showDeformed,
    deformationScale,
    showNodes,
    computeTransform,
    getDeformedNodes,
  ]);

  // Redraw on state change
  useEffect(() => {
    draw();
  }, [draw]);

  // Resize observer
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const parent = canvas.parentElement;
    if (!parent) return;

    const observer = new ResizeObserver(() => {
      const rect = parent.getBoundingClientRect();
      canvas.width = Math.floor(rect.width);
      canvas.height = Math.floor(rect.height);
      draw();
    });
    observer.observe(parent);
    return () => observer.disconnect();
  }, [draw]);

  // Reset view when mesh changes
  useEffect(() => {
    setViewOffset({ x: 0, y: 0 });
    setViewScale(1);
  }, [mesh]);

  // Mouse wheel zoom
  const handleWheel = (e: React.WheelEvent<HTMLCanvasElement>) => {
    e.preventDefault();
    const delta = e.deltaY > 0 ? 0.9 : 1.1;
    setViewScale((prev) => Math.max(0.1, Math.min(10, prev * delta)));
  };

  // Pan start
  const handleMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (e.button !== 0) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    setIsPanning(true);
    setPanStart({
      px: e.clientX - rect.left,
      py: e.clientY - rect.top,
      ox: viewOffset.x,
      oy: viewOffset.y,
    });
  };

  // Pan move
  const handleMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!isPanning || !panStart) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    setViewOffset({
      x: panStart.ox + (px - panStart.px),
      y: panStart.oy + (py - panStart.py),
    });
  };

  // Pan end
  const handleMouseUp = () => {
    setIsPanning(false);
    setPanStart(null);
  };

  const cursorStyle = isPanning ? 'grabbing' : 'grab';

  return (
    <div className="results-canvas-container">
      {/* Floating toolbar */}
      <div className="contour-toolbar">
        <div className="contour-toolbar-row">
          <label className="contour-toolbar-label">Field</label>
          <select
            className="contour-field-selector"
            value={selectedField}
            onChange={(e) => setSelectedField(e.target.value as ContourField)}
          >
            <optgroup label="Stress">
              {STRESS_FIELDS.map((f) => (
                <option key={f} value={f}>
                  {FIELD_LABELS[f]}
                </option>
              ))}
            </optgroup>
            <optgroup label="Displacement">
              {DISPLACEMENT_FIELDS.map((f) => (
                <option key={f} value={f}>
                  {FIELD_LABELS[f]}
                </option>
              ))}
            </optgroup>
          </select>
        </div>

        <div className="contour-toolbar-row">
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={showDeformed}
              onChange={(e) => setShowDeformed(e.target.checked)}
            />
            Deformed
          </label>
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={showNodes}
              onChange={(e) => setShowNodes(e.target.checked)}
            />
            Nodes
          </label>
        </div>

        {showDeformed && (
          <div className="contour-toolbar-row">
            <label className="contour-toolbar-label">Scale</label>
            <input
              type="range"
              min="0"
              max="1000"
              step="1"
              value={Math.log10(deformationScale + 1) * 200}
              onChange={(e) => {
                const raw = +e.target.value / 200;
                setDeformationScale(Math.pow(10, raw) - 1);
              }}
            />
            <span className="contour-toolbar-value">
              {deformationScale.toFixed(1)}x
            </span>
          </div>
        )}
      </div>

      {/* Canvas */}
      <canvas
        ref={canvasRef}
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        style={{ cursor: cursorStyle }}
      />
    </div>
  );
}
