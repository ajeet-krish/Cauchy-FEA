import { useRef, useEffect, useState, useCallback } from 'react';
import type { MeshData, Point } from '../types';

interface MeshCanvasProps {
  mesh: MeshData | null;
  showGrid?: boolean;
  showNodes?: boolean;
}

// Element color palette (cycling)
const ELEM_COLORS = [
  'rgba(88, 166, 255, 0.35)',
  'rgba(255, 179, 71, 0.35)',
  'rgba(63, 185, 80, 0.35)',
  'rgba(255, 0, 102, 0.35)',
  'rgba(0, 212, 255, 0.35)',
  'rgba(210, 153, 34, 0.35)',
  'rgba(139, 148, 158, 0.35)',
  'rgba(240, 136, 62, 0.35)',
];

const EDGE_COLOR = '#58a6ff';
const NODE_COLOR = '#ffb347';
const BG_COLOR = '#0a0e14';
const GRID_COLOR = '#1c2128';
const AXIS_COLOR_X = '#ff6b6b';
const AXIS_COLOR_Y = '#51cf66';

function getBBox(nodes: Point[]): { x0: number; y0: number; x1: number; y1: number } {
  if (nodes.length === 0) return { x0: 0, y0: 0, x1: 1, y1: 1 };
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const n of nodes) {
    if (n.x < x0) x0 = n.x;
    if (n.y < y0) y0 = n.y;
    if (n.x > x1) x1 = n.x;
    if (n.y > y1) y1 = n.y;
  }
  // Add small padding
  const pad = Math.max((x1 - x0), (y1 - y0)) * 0.05 || 1;
  return { x0: x0 - pad, y0: y0 - pad, x1: x1 + pad, y1: y1 + pad };
}

export default function MeshCanvas({ mesh, showGrid = true, showNodes = true }: MeshCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [viewOffset, setViewOffset] = useState({ x: 0, y: 0 });
  const [viewScale, setViewScale] = useState(1);
  const [isPanning, setIsPanning] = useState(false);
  const [panStart, setPanStart] = useState<{ px: number; py: number; ox: number; oy: number } | null>(null);

  // Compute transform to fit mesh in canvas
  const computeTransform = useCallback((canvasW: number, canvasH: number) => {
    if (!mesh || mesh.nodes.length === 0) {
      return { offsetX: canvasW / 2, offsetY: canvasH / 2, scale: 1 };
    }
    const bbox = getBBox(mesh.nodes);
    const meshW = bbox.x1 - bbox.x0;
    const meshH = bbox.y1 - bbox.y0;
    if (meshW <= 0 || meshH <= 0) {
      return { offsetX: canvasW / 2, offsetY: canvasH / 2, scale: 1 };
    }
    const padding = 40;
    const scaleX = (canvasW - padding * 2) / meshW;
    const scaleY = (canvasH - padding * 2) / meshH;
    const baseScale = Math.min(scaleX, scaleY);
    const scale = baseScale * viewScale;
    const centerX = (bbox.x0 + bbox.x1) / 2;
    const centerY = (bbox.y0 + bbox.y1) / 2;
    const offsetX = canvasW / 2 - centerX * scale + viewOffset.x;
    const offsetY = canvasH / 2 - centerY * scale + viewOffset.y;
    return { offsetX, offsetY, scale };
  }, [mesh, viewScale, viewOffset]);

  // Draw
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

    if (!mesh || mesh.nodes.length === 0) {
      ctx.fillStyle = '#484f58';
      ctx.font = '13px JetBrains Mono, monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('No mesh data', w / 2, h / 2);
      return;
    }

    const { offsetX, offsetY, scale } = computeTransform(w, h);

    const toScreen = (pt: Point) => ({
      sx: offsetX + pt.x * scale,
      sy: offsetY + pt.y * scale,
    });

    // Background grid
    if (showGrid) {
      const bbox = getBBox(mesh.nodes);
      const gridStep = Math.max(1, Math.ceil((bbox.x1 - bbox.x0) / 20));
      ctx.strokeStyle = GRID_COLOR;
      ctx.lineWidth = 1;
      const startGx = Math.floor(bbox.x0 / gridStep) * gridStep;
      const endGx = Math.ceil(bbox.x1 / gridStep) * gridStep;
      for (let gx = startGx; gx <= endGx; gx += gridStep) {
        const { sx } = toScreen({ x: gx, y: 0 });
        ctx.beginPath();
        ctx.moveTo(sx, 0);
        ctx.lineTo(sx, h);
        ctx.stroke();
      }
      const startGy = Math.floor(bbox.y0 / gridStep) * gridStep;
      const endGy = Math.ceil(bbox.y1 / gridStep) * gridStep;
      for (let gy = startGy; gy <= endGy; gy += gridStep) {
        const { sy } = toScreen({ x: 0, y: gy });
        ctx.beginPath();
        ctx.moveTo(0, sy);
        ctx.lineTo(w, sy);
        ctx.stroke();
      }
    }

    // Draw elements (filled with cycling colors)
    for (let ei = 0; ei < mesh.elements.length; ei++) {
      const elem = mesh.elements[ei];
      const nodeIndices = elem.nodes;
      if (nodeIndices.length < 3) continue;

      ctx.fillStyle = ELEM_COLORS[ei % ELEM_COLORS.length];
      ctx.beginPath();
      const first = toScreen(mesh.nodes[nodeIndices[0]]);
      ctx.moveTo(first.sx, first.sy);
      for (let ni = 1; ni < nodeIndices.length; ni++) {
        const pt = toScreen(mesh.nodes[nodeIndices[ni]]);
        ctx.lineTo(pt.sx, pt.sy);
      }
      ctx.closePath();
      ctx.fill();
    }

    // Draw element edges
    ctx.strokeStyle = EDGE_COLOR;
    ctx.lineWidth = 0.8;
    for (const elem of mesh.elements) {
      const nodeIndices = elem.nodes;
      if (nodeIndices.length < 2) continue;
      ctx.beginPath();
      const first = toScreen(mesh.nodes[nodeIndices[0]]);
      ctx.moveTo(first.sx, first.sy);
      for (let ni = 1; ni < nodeIndices.length; ni++) {
        const pt = toScreen(mesh.nodes[nodeIndices[ni]]);
        ctx.lineTo(pt.sx, pt.sy);
      }
      if (nodeIndices.length > 2) {
        ctx.closePath();
      }
      ctx.stroke();
    }

    // Draw nodes
    if (showNodes) {
      const nodeRadius = Math.max(1.5, Math.min(3, 2000 / mesh.nodes.length));
      for (const node of mesh.nodes) {
        const { sx, sy } = toScreen(node);
        ctx.fillStyle = NODE_COLOR;
        ctx.beginPath();
        ctx.arc(sx, sy, nodeRadius, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // Coordinate axes at bottom-left
    const axisLen = 40;
    const axisX = 30;
    const axisY = h - 30;
    // X axis
    ctx.strokeStyle = AXIS_COLOR_X;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(axisX, axisY);
    ctx.lineTo(axisX + axisLen, axisY);
    ctx.stroke();
    ctx.fillStyle = AXIS_COLOR_X;
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
    ctx.strokeStyle = AXIS_COLOR_Y;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(axisX, axisY);
    ctx.lineTo(axisX, axisY - axisLen);
    ctx.stroke();
    ctx.fillStyle = AXIS_COLOR_Y;
    ctx.beginPath();
    ctx.moveTo(axisX, axisY - axisLen - 6);
    ctx.lineTo(axisX - 4, axisY - axisLen + 2);
    ctx.lineTo(axisX + 4, axisY - axisLen + 2);
    ctx.closePath();
    ctx.fill();
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    ctx.fillText('Y', axisX - 8, axisY - axisLen / 2);

    // Mesh info overlay
    ctx.fillStyle = 'rgba(13, 17, 23, 0.8)';
    ctx.fillRect(w - 160, 8, 152, 44);
    ctx.strokeStyle = '#21262d';
    ctx.lineWidth = 1;
    ctx.strokeRect(w - 160, 8, 152, 44);
    ctx.fillStyle = '#8b949e';
    ctx.font = '10px JetBrains Mono, monospace';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText(`Nodes: ${mesh.num_nodes}`, w - 152, 14);
    ctx.fillText(`Elements: ${mesh.num_elements}`, w - 152, 28);
    ctx.fillText(`DOFs: ${mesh.num_dofs}`, w - 152, 42);
  }, [mesh, showGrid, showNodes, computeTransform]);

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
    <div className="mesh-canvas-container">
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
