import { useState, useRef, useEffect, useCallback } from 'react';
import type { Shape, Point } from '../types';

interface GeometryEditorProps {
  shapes: Shape[];
  nx: number;
  ny: number;
  onChange: (shapes: Shape[]) => void;
}

type DrawTool = 'rectangle' | 'circle' | 'polygon' | 'ibeam' | 'lbracket' | 'move';

function nextShapeName(type: string, shapes: Shape[]): string {
  const labels: Record<string, string> = {
    rectangle: 'Rectangle',
    circle: 'Circle',
    polygon: 'Polygon',
    ibeam: 'I-Beam',
    lbracket: 'L-Bracket',
  };
  const label = labels[type] || type;
  const count = shapes.filter((s) => s.name.startsWith(label)).length + 1;
  return `${label} ${count}`;
}

function computeIBeamPoints(s: Shape): Point[] {
  const fw = s.width ?? 20;
  const th = s.height ?? 30;
  const ft = s.flange ?? 3;
  const wt = s.web ?? 2;
  const cx = s.x;
  const cy = s.y;
  const hw = fw / 2;
  const hh = th / 2;
  const hwW = wt / 2;
  return [
    { x: cx - hw, y: cy - hh },
    { x: cx + hw, y: cy - hh },
    { x: cx + hw, y: cy - hh + ft },
    { x: cx + hwW, y: cy - hh + ft },
    { x: cx + hwW, y: cy + hh - ft },
    { x: cx + hw, y: cy + hh - ft },
    { x: cx + hw, y: cy + hh },
    { x: cx - hw, y: cy + hh },
    { x: cx - hw, y: cy + hh - ft },
    { x: cx - hwW, y: cy + hh - ft },
    { x: cx - hwW, y: cy - hh + ft },
    { x: cx - hw, y: cy - hh + ft },
  ];
}

function computeLBracketPoints(s: Shape): Point[] {
  const hw = (s.width ?? 25) / 2;
  const hh = (s.height ?? 25) / 2;
  const t = s.flange ?? 3;
  const cx = s.x;
  const cy = s.y;
  return [
    { x: cx - hw, y: cy + hh },
    { x: cx + hw, y: cy + hh },
    { x: cx + hw, y: cy + hh - t },
    { x: cx - hw + t, y: cy + hh - t },
    { x: cx - hw + t, y: cy - hh },
    { x: cx - hw, y: cy - hh },
  ];
}

function getShapePolygonPoints(s: Shape): Point[] {
  if (s.type === 'ibeam') return computeIBeamPoints(s);
  if (s.type === 'lbracket') return computeLBracketPoints(s);
  if (s.type === 'polygon' && s.points) return s.points;
  if (s.type === 'rectangle' && s.width != null && s.height != null) {
    return [
      { x: s.x, y: s.y },
      { x: s.x + s.width, y: s.y },
      { x: s.x + s.width, y: s.y + s.height },
      { x: s.x, y: s.y + s.height },
    ];
  }
  if (s.type === 'circle' && s.radius != null) {
    const pts: Point[] = [];
    const n = 24;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2;
      pts.push({ x: s.x + s.radius * Math.cos(a), y: s.y + s.radius * Math.sin(a) });
    }
    return pts;
  }
  return [];
}

function pointInPolygon(px: number, py: number, pts: Point[]): boolean {
  let inside = false;
  for (let j = 0, k = pts.length - 1; j < pts.length; k = j++) {
    const xi = pts[j].x, yi = pts[j].y;
    const xj = pts[k].x, yj = pts[k].y;
    if (((yi > py) !== (yj > py)) && (px < ((xj - xi) * (py - yi)) / (yj - yi) + xi)) {
      inside = !inside;
    }
  }
  return inside;
}

export default function GeometryEditor({ shapes, nx, ny, onChange }: GeometryEditorProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [activeTool, setActiveTool] = useState<DrawTool>('rectangle');
  const [isDrawing, setIsDrawing] = useState(false);
  const [drawStart, setDrawStart] = useState<Point | null>(null);
  const [drawCurrent, setDrawCurrent] = useState<Point | null>(null);
  const [polygonPoints, setPolygonPoints] = useState<Point[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);

  const [draggingId, setDraggingId] = useState<string | null>(null);
  const [dragStartGrid, setDragStartGrid] = useState<Point | null>(null);

  const [resizingId, setResizingId] = useState<string | null>(null);
  const [resizeHandle, setResizeHandle] = useState<string | null>(null);
  const [resizeSnapshot, setResizeSnapshot] = useState<Shape | null>(null);

  const [ibeamParams, setIbeamParams] = useState({ flangeWidth: 20, flangeThickness: 3, webThickness: 2, totalHeight: 30 });
  const [lbracketParams, setLbracketParams] = useState({ horizontalWidth: 25, verticalHeight: 25, thickness: 3 });

  const [collisionWarning, setCollisionWarning] = useState<string | null>(null);

  // Grid <-> Canvas coordinate conversion
  const gridToCanvas = useCallback((gx: number, gy: number, canvasW: number, canvasH: number) => {
    const scaleX = canvasW / nx;
    const scaleY = canvasH / ny;
    const scale = Math.min(scaleX, scaleY);
    const offsetX = (canvasW - nx * scale) / 2;
    const offsetY = (canvasH - ny * scale) / 2;
    return {
      px: offsetX + gx * scale,
      py: offsetY + gy * scale,
      scale,
      offsetX,
      offsetY,
    };
  }, [nx, ny]);

  const canvasToGrid = useCallback((px: number, py: number, canvasW: number, canvasH: number) => {
    const scaleX = canvasW / nx;
    const scaleY = canvasH / ny;
    const scale = Math.min(scaleX, scaleY);
    const offsetX = (canvasW - nx * scale) / 2;
    const offsetY = (canvasH - ny * scale) / 2;
    return {
      gx: (px - offsetX) / scale,
      gy: (py - offsetY) / scale,
    };
  }, [nx, ny]);

  // Hit test: find shape at grid point
  const findShapeAtPoint = useCallback(
    (gx: number, gy: number): Shape | null => {
      for (let i = shapes.length - 1; i >= 0; i--) {
        const s = shapes[i];
        if (s.type === 'circle' && s.radius != null) {
          const dx = gx - s.x;
          const dy = gy - s.y;
          if (dx * dx + dy * dy <= s.radius * s.radius) return s;
        } else if (s.type === 'rectangle' && s.width != null && s.height != null) {
          if (gx >= s.x && gx <= s.x + s.width && gy >= s.y && gy <= s.y + s.height) return s;
        } else {
          const pts = getShapePolygonPoints(s);
          if (pts.length >= 3 && pointInPolygon(gx, gy, pts)) return s;
        }
      }
      return null;
    },
    [shapes]
  );

  // Get resize handles for a shape
  const getResizeHandles = useCallback(
    (shape: Shape): Array<{ id: string; gx: number; gy: number }> => {
      const handles: Array<{ id: string; gx: number; gy: number }> = [];
      if (shape.type === 'circle' && shape.radius != null) {
        handles.push(
          { id: 'n', gx: shape.x, gy: shape.y - shape.radius },
          { id: 's', gx: shape.x, gy: shape.y + shape.radius },
          { id: 'e', gx: shape.x + shape.radius, gy: shape.y },
          { id: 'w', gx: shape.x - shape.radius, gy: shape.y },
        );
      } else if (shape.type === 'rectangle' && shape.width != null && shape.height != null) {
        const x0 = shape.x, y0 = shape.y;
        const x1 = shape.x + shape.width, y1 = shape.y + shape.height;
        handles.push(
          { id: 'nw', gx: x0, gy: y0 },
          { id: 'ne', gx: x1, gy: y0 },
          { id: 'sw', gx: x0, gy: y1 },
          { id: 'se', gx: x1, gy: y1 },
          { id: 'n', gx: (x0 + x1) / 2, gy: y0 },
          { id: 's', gx: (x0 + x1) / 2, gy: y1 },
          { id: 'w', gx: x0, gy: (y0 + y1) / 2 },
          { id: 'e', gx: x1, gy: (y0 + y1) / 2 },
        );
      } else {
        const pts = getShapePolygonPoints(shape);
        for (let i = 0; i < pts.length; i++) {
          handles.push({ id: `v${i}`, gx: pts[i].x, gy: pts[i].y });
        }
      }
      return handles;
    },
    []
  );

  // Hit test: find resize handle near point
  const findHandleAtPoint = useCallback(
    (gx: number, gy: number, shapeId: string): string | null => {
      const shape = shapes.find((s) => s.id === shapeId);
      if (!shape) return null;
      const handles = getResizeHandles(shape);
      // Threshold proportional to grid density (0.5 grid units)
      const THRESH = Math.max(nx, ny) * 0.025;
      for (const h of handles) {
        const dx = gx - h.gx;
        const dy = gy - h.gy;
        if (dx * dx + dy * dy <= THRESH * THRESH) return h.id;
      }
      return null;
    },
    [shapes, getResizeHandles, nx, ny]
  );

  // Move shape by delta
  const moveShape = useCallback(
    (id: string, dx: number, dy: number) => {
      const updated = shapes.map((s) => {
        if (s.id !== id) return s;
        if (s.type === 'circle') return { ...s, x: s.x + dx, y: s.y + dy };
        if (s.type === 'rectangle') return { ...s, x: s.x + dx, y: s.y + dy };
        if (s.type === 'ibeam') return { ...s, x: s.x + dx, y: s.y + dy };
        if (s.type === 'lbracket') return { ...s, x: s.x + dx, y: s.y + dy };
        if (s.type === 'polygon' && s.points) {
          return {
            ...s,
            x: s.x + dx,
            y: s.y + dy,
            points: s.points.map((p) => ({ x: p.x + dx, y: p.y + dy })),
          };
        }
        return s;
      });
      onChange(updated);
    },
    [shapes, onChange]
  );

  // Apply resize to shape
  const applyResize = useCallback(
    (id: string, handle: string, gx: number, gy: number) => {
      if (!resizeSnapshot) return;
      const snap = resizeSnapshot;
      const updated = shapes.map((s) => {
        if (s.id !== id) return s;

        if (s.type === 'circle' && snap.type === 'circle' && snap.radius != null) {
          const dx = gx - snap.x;
          const dy = gy - snap.y;
          const newR = Math.max(3, Math.sqrt(dx * dx + dy * dy));
          return { ...s, radius: newR };
        }

        if (s.type === 'rectangle' && snap.type === 'rectangle'
          && snap.width != null && snap.height != null) {
          const x0 = snap.x, y0 = snap.y;
          const x1 = snap.x + snap.width;
          const y1 = snap.y + snap.height;
          let nx0 = x0, ny0 = y0, nx1 = x1, ny1 = y1;

          if (handle === 'nw') { nx0 = gx; ny0 = gy; }
          else if (handle === 'ne') { nx1 = gx; ny0 = gy; }
          else if (handle === 'sw') { nx0 = gx; ny1 = gy; }
          else if (handle === 'se') { nx1 = gx; ny1 = gy; }
          else if (handle === 'n') { ny0 = gy; }
          else if (handle === 's') { ny1 = gy; }
          else if (handle === 'w') { nx0 = gx; }
          else if (handle === 'e') { nx1 = gx; }

          const finalX = Math.min(nx0, nx1);
          const finalY = Math.min(ny0, ny1);
          const finalW = Math.max(3, Math.abs(nx1 - nx0));
          const finalH = Math.max(3, Math.abs(ny1 - ny0));
          return { ...s, x: finalX, y: finalY, width: finalW, height: finalH };
        }

        // Polygon, I-beam, L-bracket: scale relative to centroid
        if (handle.startsWith('v')) {
          const snapPts = getShapePolygonPoints(snap);
          const vi = parseInt(handle.slice(1));
          if (vi >= 0 && vi < snapPts.length) {
            let cx = 0, cy = 0;
            for (const pt of snapPts) { cx += pt.x; cy += pt.y; }
            cx /= snapPts.length;
            cy /= snapPts.length;
            const oldPt = snapPts[vi];
            const oldDist = Math.hypot(oldPt.x - cx, oldPt.y - cy);
            const newDist = Math.hypot(gx - cx, gy - cy);
            const scaleFactor = oldDist > 0.1 ? newDist / oldDist : 1;

            if (s.type === 'ibeam') {
              return {
                ...s,
                width: Math.max(4, (snap.width ?? 20) * scaleFactor),
                height: Math.max(4, (snap.height ?? 30) * scaleFactor),
                flange: Math.max(1, (snap.flange ?? 3) * scaleFactor),
                web: Math.max(1, (snap.web ?? 2) * scaleFactor),
              };
            }
            if (s.type === 'lbracket') {
              return {
                ...s,
                width: Math.max(4, (snap.width ?? 25) * scaleFactor),
                height: Math.max(4, (snap.height ?? 25) * scaleFactor),
                flange: Math.max(1, (snap.flange ?? 3) * scaleFactor),
              };
            }
            // Generic polygon resize
            if (snap.type === 'polygon' && snap.points && s.points) {
              const newPoints = snap.points.map((pt) => ({
                x: cx + (pt.x - cx) * scaleFactor,
                y: cy + (pt.y - cy) * scaleFactor,
              }));
              return { ...s, points: newPoints };
            }
          }
        }
        return s;
      });
      onChange(updated);
    },
    [shapes, resizeSnapshot, onChange]
  );

  // Collision check
  const checkCollision = useCallback(
    (newShape: Shape): string | null => {
      for (const existing of shapes) {
        if (existing.id === newShape.id) continue;
        const newPts = getShapePolygonPoints(newShape);
        const existPts = getShapePolygonPoints(existing);
        if (newPts.length >= 3 && existPts.length >= 3) {
          // Simple bounding box overlap check
          const nb = getBBox(newPts);
          const eb = getBBox(existPts);
          if (nb.x0 < eb.x1 && nb.x1 > eb.x0 && nb.y0 < eb.y1 && nb.y1 > eb.y0) {
            return `Warning: "${newShape.name}" overlaps "${existing.name}"`;
          }
        }
      }
      return null;
    },
    [shapes]
  );

  // Draw the canvas
  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;

    // Clear
    ctx.fillStyle = '#0a0e14';
    ctx.fillRect(0, 0, w, h);

    const { scale, offsetX, offsetY } = gridToCanvas(0, 0, w, h);

    // Draw grid
    ctx.strokeStyle = '#1c2128';
    ctx.lineWidth = 1;
    const gridStep = Math.max(1, Math.floor(nx / 20));
    for (let x = 0; x <= nx; x += gridStep) {
      const px = offsetX + x * scale;
      ctx.beginPath();
      ctx.moveTo(px, offsetY);
      ctx.lineTo(px, offsetY + ny * scale);
      ctx.stroke();
    }
    const gridStepY = Math.max(1, Math.floor(ny / 20));
    for (let y = 0; y <= ny; y += gridStepY) {
      const py = offsetY + y * scale;
      ctx.beginPath();
      ctx.moveTo(offsetX, py);
      ctx.lineTo(offsetX + nx * scale, py);
      ctx.stroke();
    }

    // Grid coordinate labels
    ctx.fillStyle = '#484f58';
    ctx.font = '9px JetBrains Mono, monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    for (let x = 0; x <= nx; x += Math.max(gridStep * 2, 1)) {
      const px = offsetX + x * scale;
      ctx.fillText(String(x), px, offsetY + ny * scale + 2);
    }
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (let y = 0; y <= ny; y += Math.max(gridStepY * 2, 1)) {
      const py = offsetY + y * scale;
      ctx.fillText(String(y), offsetX - 4, py);
    }

    // Domain bbox (union of all shapes)
    if (shapes.length > 0) {
      let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
      for (const s of shapes) {
        const pts = getShapePolygonPoints(s);
        for (const pt of pts) {
          if (pt.x < minX) minX = pt.x;
          if (pt.y < minY) minY = pt.y;
          if (pt.x > maxX) maxX = pt.x;
          if (pt.y > maxY) maxY = pt.y;
        }
      }
      const bx0 = offsetX + minX * scale;
      const by0 = offsetY + minY * scale;
      const bw = (maxX - minX) * scale;
      const bh = (maxY - minY) * scale;
      ctx.strokeStyle = 'rgba(255, 179, 71, 0.4)';
      ctx.lineWidth = 1.5;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(bx0, by0, bw, bh);
      ctx.setLineDash([]);

      // Domain info label
      ctx.fillStyle = 'rgba(255, 179, 71, 0.7)';
      ctx.font = '9px JetBrains Mono, monospace';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'bottom';
      ctx.fillText(
        `Domain: ${minX.toFixed(0)}..${maxX.toFixed(0)} x ${minY.toFixed(0)}..${maxY.toFixed(0)}`,
        bx0, by0 - 3
      );
    }

    // Draw domain boundary
    ctx.strokeStyle = '#58a6ff';
    ctx.lineWidth = 2;
    ctx.strokeRect(offsetX, offsetY, nx * scale, ny * scale);

    // Draw placed shapes
    for (const shape of shapes) {
      const isSelected = shape.id === selectedId;
      ctx.fillStyle = isSelected ? 'rgba(88, 166, 255, 0.3)' : 'rgba(139, 148, 158, 0.3)';
      ctx.strokeStyle = isSelected ? '#58a6ff' : '#8b949e';
      ctx.lineWidth = isSelected ? 2 : 1;

      if (shape.type === 'circle' && shape.radius != null) {
        const { px, py } = gridToCanvas(shape.x, shape.y, w, h);
        ctx.beginPath();
        ctx.arc(px, py, shape.radius * scale, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      } else if (shape.type === 'rectangle' && shape.width != null && shape.height != null) {
        const { px: x0, py: y0 } = gridToCanvas(shape.x, shape.y, w, h);
        ctx.fillRect(x0, y0, shape.width * scale, shape.height * scale);
        ctx.strokeRect(x0, y0, shape.width * scale, shape.height * scale);
      } else {
        const pts = getShapePolygonPoints(shape);
        if (pts.length >= 3) {
          ctx.beginPath();
          const first = gridToCanvas(pts[0].x, pts[0].y, w, h);
          ctx.moveTo(first.px, first.py);
          for (let i = 1; i < pts.length; i++) {
            const pt = gridToCanvas(pts[i].x, pts[i].y, w, h);
            ctx.lineTo(pt.px, pt.py);
          }
          ctx.closePath();
          ctx.fill();
          ctx.stroke();
        }
      }

      // Shape name label
      const pts = getShapePolygonPoints(shape);
      let labelX: number, labelY: number;
      if (pts.length > 0) {
        let cx = 0, cy = 0;
        for (const pt of pts) { cx += pt.x; cy += pt.y; }
        cx /= pts.length;
        cy /= pts.length;
        const lbl = gridToCanvas(cx, cy, w, h);
        labelX = lbl.px;
        labelY = lbl.py;
      } else {
        const lbl = gridToCanvas(shape.x, shape.y, w, h);
        labelX = lbl.px;
        labelY = lbl.py;
      }
      ctx.fillStyle = '#58a6ff';
      ctx.font = '10px JetBrains Mono, monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(shape.name, labelX, labelY);

      // Resize handles for selected shape
      if (isSelected) {
        const handles = getResizeHandles(shape);
        for (const handle of handles) {
          const { px: hx, py: hy } = gridToCanvas(handle.gx, handle.gy, w, h);
          ctx.fillStyle = '#f0883e';
          ctx.strokeStyle = '#ffffff';
          ctx.lineWidth = 1.5;
          ctx.beginPath();
          ctx.arc(hx, hy, 5, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
        }
      }
    }

    // Draw in-progress shape
    if (isDrawing && drawStart && drawCurrent) {
      ctx.strokeStyle = '#2ea043';
      ctx.fillStyle = 'rgba(46, 160, 67, 0.2)';
      ctx.lineWidth = 2;
      ctx.setLineDash([5, 5]);

      if (activeTool === 'circle') {
        const dx = drawCurrent.x - drawStart.x;
        const dy = drawCurrent.y - drawStart.y;
        const r = Math.sqrt(dx * dx + dy * dy);
        const { px, py } = gridToCanvas(drawStart.x, drawStart.y, w, h);
        ctx.beginPath();
        ctx.arc(px, py, r * scale, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      } else if (activeTool === 'rectangle') {
        const x0 = Math.min(drawStart.x, drawCurrent.x);
        const y0 = Math.min(drawStart.y, drawCurrent.y);
        const x1 = Math.max(drawStart.x, drawCurrent.x);
        const y1 = Math.max(drawStart.y, drawCurrent.y);
        const { px, py } = gridToCanvas(x0, y0, w, h);
        ctx.fillRect(px, py, (x1 - x0) * scale, (y1 - y0) * scale);
        ctx.strokeRect(px, py, (x1 - x0) * scale, (y1 - y0) * scale);
      }
      ctx.setLineDash([]);
    }

    // Draw polygon preview
    if (activeTool === 'polygon' && polygonPoints.length > 0) {
      ctx.strokeStyle = '#2ea043';
      ctx.fillStyle = 'rgba(46, 160, 67, 0.2)';
      ctx.lineWidth = 2;
      ctx.setLineDash([5, 5]);

      ctx.beginPath();
      const first = gridToCanvas(polygonPoints[0].x, polygonPoints[0].y, w, h);
      ctx.moveTo(first.px, first.py);
      for (let i = 1; i < polygonPoints.length; i++) {
        const pt = gridToCanvas(polygonPoints[i].x, polygonPoints[i].y, w, h);
        ctx.lineTo(pt.px, pt.py);
      }
      if (drawCurrent) {
        const curr = gridToCanvas(drawCurrent.x, drawCurrent.y, w, h);
        ctx.lineTo(curr.px, curr.py);
      }
      ctx.stroke();
      ctx.setLineDash([]);

      for (const pt of polygonPoints) {
        const { px, py } = gridToCanvas(pt.x, pt.y, w, h);
        ctx.fillStyle = '#2ea043';
        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }, [
    shapes, selectedId, isDrawing, drawStart, drawCurrent,
    activeTool, polygonPoints, nx, ny, gridToCanvas, getResizeHandles,
  ]);

  useEffect(() => {
    draw();
  }, [draw]);

  // Resize canvas
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

  const getMouseGrid = (e: React.MouseEvent<HTMLCanvasElement>): Point => {
    const canvas = canvasRef.current!;
    const rect = canvas.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    const { gx, gy } = canvasToGrid(px, py, canvas.width, canvas.height);
    return { x: Math.round(gx), y: Math.round(gy) };
  };

  const handleMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const pt = getMouseGrid(e);

    // Check resize handle first
    if (selectedId) {
      const handle = findHandleAtPoint(pt.x, pt.y, selectedId);
      if (handle) {
        const shape = shapes.find((s) => s.id === selectedId);
        if (shape) {
          setResizingId(selectedId);
          setResizeHandle(handle);
          setResizeSnapshot({
            ...shape,
            points: shape.points ? shape.points.map((p) => ({ ...p })) : undefined,
          });
          return;
        }
      }
    }

    // Check if clicking existing shape (drag-to-move)
    const hitShape = findShapeAtPoint(pt.x, pt.y);
    if (hitShape) {
      setSelectedId(hitShape.id);
      setDraggingId(hitShape.id);
      setDragStartGrid(pt);
      return;
    }

    // Move tool on empty space: deselect
    if (activeTool === 'move') {
      setSelectedId(null);
      return;
    }

    // I-beam placement
    if (activeTool === 'ibeam') {
      const name = nextShapeName('ibeam', shapes);
      const newShape: Shape = {
        id: Date.now().toString(),
        type: 'ibeam',
        name,
        x: pt.x,
        y: pt.y,
        width: ibeamParams.flangeWidth,
        height: ibeamParams.totalHeight,
        flange: ibeamParams.flangeThickness,
        web: ibeamParams.webThickness,
      };
      const warning = checkCollision(newShape);
      setCollisionWarning(warning);
      const updated = [...shapes, newShape];
      onChange(updated);
      setSelectedId(newShape.id);
      return;
    }

    // L-bracket placement
    if (activeTool === 'lbracket') {
      const name = nextShapeName('lbracket', shapes);
      const newShape: Shape = {
        id: Date.now().toString(),
        type: 'lbracket',
        name,
        x: pt.x,
        y: pt.y,
        width: lbracketParams.horizontalWidth,
        height: lbracketParams.verticalHeight,
        flange: lbracketParams.thickness,
      };
      const warning = checkCollision(newShape);
      setCollisionWarning(warning);
      const updated = [...shapes, newShape];
      onChange(updated);
      setSelectedId(newShape.id);
      return;
    }

    // Polygon vertex-by-vertex
    if (activeTool === 'polygon') {
      if (polygonPoints.length >= 3) {
        const first = polygonPoints[0];
        const dx = pt.x - first.x;
        const dy = pt.y - first.y;
        if (dx * dx + dy * dy < 100) {
          const name = nextShapeName('polygon', shapes);
          const newShape: Shape = {
            id: Date.now().toString(),
            type: 'polygon',
            name,
            x: 0,
            y: 0,
            points: [...polygonPoints],
          };
          const warning = checkCollision(newShape);
          setCollisionWarning(warning);
          const updated = [...shapes, newShape];
          onChange(updated);
          setPolygonPoints([]);
          return;
        }
      }
      setPolygonPoints([...polygonPoints, pt]);
      return;
    }

    // Rectangle / circle: start drag
    setIsDrawing(true);
    setDrawStart(pt);
    setDrawCurrent(pt);
  };

  const handleMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const pt = getMouseGrid(e);

    if (resizingId && resizeHandle) {
      applyResize(resizingId, resizeHandle, pt.x, pt.y);
      return;
    }

    if (draggingId && dragStartGrid) {
      const dx = pt.x - dragStartGrid.x;
      const dy = pt.y - dragStartGrid.y;
      moveShape(draggingId, dx, dy);
      setDragStartGrid(pt);
      return;
    }

    if (!isDrawing && activeTool !== 'polygon') return;
    setDrawCurrent(pt);
  };

  const handleMouseUp = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (resizingId) {
      setResizingId(null);
      setResizeHandle(null);
      setResizeSnapshot(null);
      return;
    }

    if (draggingId) {
      setDraggingId(null);
      setDragStartGrid(null);
      return;
    }

    if (!isDrawing || !drawStart) return;
    const pt = getMouseGrid(e);

    if (activeTool === 'circle') {
      const dx = pt.x - drawStart.x;
      const dy = pt.y - drawStart.y;
      const radius = Math.sqrt(dx * dx + dy * dy);
      if (radius > 2) {
        const name = nextShapeName('circle', shapes);
        const newShape: Shape = {
          id: Date.now().toString(),
          type: 'circle',
          name,
          x: drawStart.x,
          y: drawStart.y,
          radius,
        };
        const warning = checkCollision(newShape);
        setCollisionWarning(warning);
        onChange([...shapes, newShape]);
      }
    } else if (activeTool === 'rectangle') {
      const x0 = Math.min(drawStart.x, pt.x);
      const y0 = Math.min(drawStart.y, pt.y);
      const rw = Math.abs(pt.x - drawStart.x);
      const rh = Math.abs(pt.y - drawStart.y);
      if (rw > 2 && rh > 2) {
        const name = nextShapeName('rectangle', shapes);
        const newShape: Shape = {
          id: Date.now().toString(),
          type: 'rectangle',
          name,
          x: x0,
          y: y0,
          width: rw,
          height: rh,
        };
        const warning = checkCollision(newShape);
        setCollisionWarning(warning);
        onChange([...shapes, newShape]);
      }
    }

    setIsDrawing(false);
    setDrawStart(null);
    setDrawCurrent(null);
  };

  const deleteShape = (id: string) => {
    onChange(shapes.filter((s) => s.id !== id));
    if (selectedId === id) setSelectedId(null);
    setCollisionWarning(null);
  };

  const clearAll = () => {
    onChange([]);
    setPolygonPoints([]);
    setSelectedId(null);
    setCollisionWarning(null);
  };

  const updateShapeParams = (id: string, params: Partial<Shape>) => {
    const updated = shapes.map((s) => (s.id === id ? { ...s, ...params } : s));
    onChange(updated);
  };

  const loadPreset = (preset: string) => {
    let newShapes: Shape[] = [];
    if (preset === 'cantilever') {
      newShapes = [{
        id: '1',
        type: 'rectangle',
        name: 'Cantilever',
        x: Math.round(nx * 0.1),
        y: Math.round(ny * 0.25),
        width: Math.round(nx * 0.8),
        height: Math.round(ny * 0.5),
      }];
    } else if (preset === 'lbracket') {
      newShapes = [{
        id: '1',
        type: 'lbracket',
        name: 'L-Bracket 1',
        x: Math.round(nx / 2),
        y: Math.round(ny / 2),
        width: Math.round(nx * 0.6),
        height: Math.round(ny * 0.8),
        flange: Math.round(Math.min(nx, ny) * 0.1),
      }];
    } else if (preset === 'ibeam') {
      newShapes = [{
        id: '1',
        type: 'ibeam',
        name: 'I-Beam 1',
        x: Math.round(nx / 2),
        y: Math.round(ny / 2),
        width: Math.round(nx * 0.4),
        height: Math.round(ny * 0.8),
        flange: Math.round(ny * 0.1),
        web: Math.round(ny * 0.06),
      }];
    } else if (preset === 'platehole') {
      newShapes = [{
        id: '1',
        type: 'rectangle',
        name: 'Plate',
        x: 0,
        y: 0,
        width: nx,
        height: ny,
      }, {
        id: '2',
        type: 'circle',
        name: 'Hole',
        x: Math.round(nx / 2),
        y: Math.round(ny / 2),
        radius: Math.round(Math.min(nx, ny) * 0.2),
      }];
    }
    setCollisionWarning(null);
    onChange(newShapes);
  };

  const selectedShape = shapes.find((s) => s.id === selectedId);

  const cursorStyle = (() => {
    if (resizingId) return 'nwse-resize';
    if (draggingId) return 'grabbing';
    if (activeTool === 'move') return 'grab';
    return 'crosshair';
  })();

  return (
    <div className="geometry-editor">
      {/* Canvas fills the entire container */}
      <div className="editor-canvas-container">
        <canvas
          ref={canvasRef}
          onMouseDown={handleMouseDown}
          onMouseMove={handleMouseMove}
          onMouseUp={handleMouseUp}
          style={{ cursor: cursorStyle }}
        />
      </div>

      {/* Floating toolbar -- top-left */}
      <div className="geometry-overlay geometry-toolbar-overlay">
        <div className="editor-toolbar">
          <div className="tool-group">
            <button
              className={`tool-btn ${activeTool === 'rectangle' ? 'active' : ''}`}
              onClick={() => { setActiveTool('rectangle'); setPolygonPoints([]); }}
              title="Draw Rectangle (click-drag)"
            >
              []
            </button>
            <button
              className={`tool-btn ${activeTool === 'circle' ? 'active' : ''}`}
              onClick={() => { setActiveTool('circle'); setPolygonPoints([]); }}
              title="Draw Circle (click-drag)"
            >
              O
            </button>
            <button
              className={`tool-btn ${activeTool === 'polygon' ? 'active' : ''}`}
              onClick={() => setActiveTool('polygon')}
              title="Draw Polygon (click vertices, click first to close)"
            >
              /_/
            </button>
            <button
              className={`tool-btn ${activeTool === 'ibeam' ? 'active' : ''}`}
              onClick={() => { setActiveTool('ibeam'); setPolygonPoints([]); }}
              title="Place I-Beam (click to place)"
            >
              I
            </button>
            <button
              className={`tool-btn ${activeTool === 'lbracket' ? 'active' : ''}`}
              onClick={() => { setActiveTool('lbracket'); setPolygonPoints([]); }}
              title="Place L-Bracket (click to place)"
            >
              L
            </button>
            <button
              className={`tool-btn ${activeTool === 'move' ? 'active' : ''}`}
              onClick={() => { setActiveTool('move'); setPolygonPoints([]); }}
              title="Move shapes (click-drag)"
            >
              Move
            </button>
          </div>
          <div className="tool-group">
            <button className="tool-btn danger" onClick={clearAll} title="Clear All">
              Clear
            </button>
          </div>
        </div>

        {/* I-beam parameter inputs when tool is active */}
        {activeTool === 'ibeam' && (
          <div className="shape-params">
            <span className="params-label">I-Beam Defaults</span>
            <div className="form-row">
              <div className="form-group">
                <label>Flange W</label>
                <input
                  type="number"
                  min="4"
                  value={ibeamParams.flangeWidth}
                  onChange={(e) => setIbeamParams({ ...ibeamParams, flangeWidth: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Height</label>
                <input
                  type="number"
                  min="4"
                  value={ibeamParams.totalHeight}
                  onChange={(e) => setIbeamParams({ ...ibeamParams, totalHeight: +e.target.value })}
                />
              </div>
            </div>
            <div className="form-row">
              <div className="form-group">
                <label>Flange Thk</label>
                <input
                  type="number"
                  min="1"
                  value={ibeamParams.flangeThickness}
                  onChange={(e) => setIbeamParams({ ...ibeamParams, flangeThickness: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Web Thk</label>
                <input
                  type="number"
                  min="1"
                  value={ibeamParams.webThickness}
                  onChange={(e) => setIbeamParams({ ...ibeamParams, webThickness: +e.target.value })}
                />
              </div>
            </div>
          </div>
        )}

        {/* L-bracket parameter inputs when tool is active */}
        {activeTool === 'lbracket' && (
          <div className="shape-params">
            <span className="params-label">L-Bracket Defaults</span>
            <div className="form-row">
              <div className="form-group">
                <label>Width</label>
                <input
                  type="number"
                  min="4"
                  value={lbracketParams.horizontalWidth}
                  onChange={(e) => setLbracketParams({ ...lbracketParams, horizontalWidth: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Height</label>
                <input
                  type="number"
                  min="4"
                  value={lbracketParams.verticalHeight}
                  onChange={(e) => setLbracketParams({ ...lbracketParams, verticalHeight: +e.target.value })}
                />
              </div>
            </div>
            <div className="form-group">
              <label>Leg Thk</label>
              <input
                type="number"
                min="1"
                value={lbracketParams.thickness}
                onChange={(e) => setLbracketParams({ ...lbracketParams, thickness: +e.target.value })}
              />
            </div>
          </div>
        )}

        {/* Selected I-beam/L-bracket parameter editing */}
        {selectedShape && selectedShape.type === 'ibeam' && (
          <div className="shape-params">
            <span className="params-label">{selectedShape.name}</span>
            <div className="form-row">
              <div className="form-group">
                <label>Flange W</label>
                <input
                  type="number"
                  min="4"
                  value={selectedShape.width ?? 20}
                  onChange={(e) => updateShapeParams(selectedShape.id, { width: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Height</label>
                <input
                  type="number"
                  min="4"
                  value={selectedShape.height ?? 30}
                  onChange={(e) => updateShapeParams(selectedShape.id, { height: +e.target.value })}
                />
              </div>
            </div>
            <div className="form-row">
              <div className="form-group">
                <label>Flange Thk</label>
                <input
                  type="number"
                  min="1"
                  value={selectedShape.flange ?? 3}
                  onChange={(e) => updateShapeParams(selectedShape.id, { flange: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Web Thk</label>
                <input
                  type="number"
                  min="1"
                  value={selectedShape.web ?? 2}
                  onChange={(e) => updateShapeParams(selectedShape.id, { web: +e.target.value })}
                />
              </div>
            </div>
          </div>
        )}
        {selectedShape && selectedShape.type === 'lbracket' && (
          <div className="shape-params">
            <span className="params-label">{selectedShape.name}</span>
            <div className="form-row">
              <div className="form-group">
                <label>Width</label>
                <input
                  type="number"
                  min="4"
                  value={selectedShape.width ?? 25}
                  onChange={(e) => updateShapeParams(selectedShape.id, { width: +e.target.value })}
                />
              </div>
              <div className="form-group">
                <label>Height</label>
                <input
                  type="number"
                  min="4"
                  value={selectedShape.height ?? 25}
                  onChange={(e) => updateShapeParams(selectedShape.id, { height: +e.target.value })}
                />
              </div>
            </div>
            <div className="form-group">
              <label>Leg Thk</label>
              <input
                type="number"
                min="1"
                value={selectedShape.flange ?? 3}
                onChange={(e) => updateShapeParams(selectedShape.id, { flange: +e.target.value })}
              />
            </div>
          </div>
        )}

        {/* Presets */}
        <div className="editor-presets">
          <span className="preset-label">Presets:</span>
          <button className="preset-btn" onClick={() => loadPreset('cantilever')}>Cantilever</button>
          <button className="preset-btn" onClick={() => loadPreset('lbracket')}>L-Bracket</button>
          <button className="preset-btn" onClick={() => loadPreset('ibeam')}>I-Beam</button>
          <button className="preset-btn" onClick={() => loadPreset('platehole')}>Plate+Hole</button>
        </div>
      </div>

      {/* Collision warning -- top-center */}
      {collisionWarning && (
        <div className="geometry-overlay geometry-warning-overlay">
          <div className="collision-warning">
            {collisionWarning}
          </div>
        </div>
      )}

      {/* Hints -- bottom-center */}
      {(activeTool === 'polygon' && polygonPoints.length > 0) ||
       (activeTool === 'ibeam') ||
       (activeTool === 'lbracket') ||
       (activeTool === 'move' && !draggingId) ? (
        <div className="geometry-overlay geometry-hint-overlay">
          {activeTool === 'polygon' && polygonPoints.length > 0 && (
            <div className="editor-hint">
              Click to add points. Click near first point to close polygon.
              ({polygonPoints.length} points)
            </div>
          )}
          {activeTool === 'ibeam' && (
            <div className="editor-hint">
              Click on canvas to place I-Beam. Adjust parameters above.
            </div>
          )}
          {activeTool === 'lbracket' && (
            <div className="editor-hint">
              Click on canvas to place L-Bracket. Adjust parameters above.
            </div>
          )}
          {activeTool === 'move' && !draggingId && (
            <div className="editor-hint">
              Click and drag shapes to reposition them.
            </div>
          )}
        </div>
      ) : null}

      {/* Shape list -- bottom-left */}
      {shapes.length > 0 && (
        <div className="geometry-overlay geometry-shape-list-overlay">
          <div className="shape-list">
            <h3>Shapes ({shapes.length})</h3>
            {shapes.map((shape) => (
              <div
                key={shape.id}
                className={`shape-item ${selectedId === shape.id ? 'selected' : ''}`}
                onClick={() => setSelectedId(shape.id)}
              >
                <span className="shape-info">
                  {shape.name}
                  {shape.type === 'circle' && shape.radius != null && ` (r=${shape.radius.toFixed(0)})`}
                  {shape.type === 'rectangle' && shape.width != null && shape.height != null
                    && ` (${shape.width.toFixed(0)}x${shape.height.toFixed(0)})`}
                  {shape.type === 'polygon' && shape.points && ` (${shape.points.length} pts)`}
                  {shape.type === 'ibeam' && ` (fw=${(shape.width ?? 0).toFixed(0)}, h=${(shape.height ?? 0).toFixed(0)})`}
                  {shape.type === 'lbracket' && ` (w=${(shape.width ?? 0).toFixed(0)}, h=${(shape.height ?? 0).toFixed(0)})`}
                </span>
                <button
                  className="shape-delete"
                  onClick={(e) => { e.stopPropagation(); deleteShape(shape.id); }}
                  aria-label={`Delete ${shape.name}`}
                >
                  &#10005;
                </button>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

// Bounding box helper
function getBBox(pts: Point[]): { x0: number; y0: number; x1: number; y1: number } {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const pt of pts) {
    if (pt.x < x0) x0 = pt.x;
    if (pt.y < y0) y0 = pt.y;
    if (pt.x > x1) x1 = pt.x;
    if (pt.y > y1) y1 = pt.y;
  }
  return { x0, y0, x1, y1 };
}
