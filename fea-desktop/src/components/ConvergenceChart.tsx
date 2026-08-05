import { useRef, useEffect, useCallback } from 'react';
import type { SweepResult } from '../types';

interface ConvergenceChartProps {
  results: SweepResult[];
  title?: string;
}

const PADDING = { top: 32, right: 24, bottom: 48, left: 72 };
const DOT_RADIUS = 4;
const LINE_WIDTH = 1.5;

const COLORS = {
  bg: '#0a0e14',
  grid: '#1c2128',
  gridMajor: '#21262d',
  text: '#8b949e',
  textBright: '#c9d1d9',
  accent: '#ffb347',
  dot: '#00d4ff',
  dotStroke: '#0d1117',
  line: 'rgba(0, 212, 255, 0.5)',
};

function log10(v: number): number {
  return Math.log(v) / Math.LN10;
}

function ConvergenceChart({ results, title }: ConvergenceChartProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas || results.length === 0) return;

    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.scale(dpr, dpr);

    const w = rect.width;
    const h = rect.height;
    const plotW = w - PADDING.left - PADDING.right;
    const plotH = h - PADDING.top - PADDING.bottom;

    // Clear
    ctx.fillStyle = COLORS.bg;
    ctx.fillRect(0, 0, w, h);

    // Extract data: mesh density (nx * ny) vs maxDisplacement
    const points = results.map((r) => ({
      x: r.nx * r.ny,
      y: r.maxDisplacement,
    }));

    const xs = points.map((p) => p.x);
    const ys = points.filter((p) => p.y > 0).map((p) => p.y);

    if (xs.length === 0 || ys.length === 0) return;

    const xMin = Math.min(...xs);
    const xMax = Math.max(...xs);
    const yMin = Math.min(...ys);
    const yMax = Math.max(...ys);

    // Log ranges
    const xLogMin = Math.max(0, Math.floor(log10(xMin)) - 1);
    const xLogMax = Math.ceil(log10(xMax)) + 1;
    const yLogMin = Math.floor(log10(yMin)) - 1;
    const yLogMax = Math.ceil(log10(yMax)) + 1;

    const toCanvasX = (v: number) =>
      PADDING.left + ((log10(v) - xLogMin) / (xLogMax - xLogMin)) * plotW;
    const toCanvasY = (v: number) =>
      PADDING.top + plotH - ((log10(v) - yLogMin) / (yLogMax - yLogMin)) * plotH;

    // Grid lines
    ctx.strokeStyle = COLORS.grid;
    ctx.lineWidth = 0.5;

    for (let exp = xLogMin; exp <= xLogMax; exp++) {
      const x = toCanvasX(Math.pow(10, exp));
      ctx.beginPath();
      ctx.moveTo(x, PADDING.top);
      ctx.lineTo(x, PADDING.top + plotH);
      ctx.stroke();
    }

    for (let exp = yLogMin; exp <= yLogMax; exp++) {
      const y = toCanvasY(Math.pow(10, exp));
      ctx.beginPath();
      ctx.moveTo(PADDING.left, y);
      ctx.lineTo(PADDING.left + plotW, y);
      ctx.stroke();
    }

    // Axis labels
    ctx.fillStyle = COLORS.text;
    ctx.font = '10px JetBrains Mono, monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    for (let exp = xLogMin; exp <= xLogMax; exp++) {
      const x = toCanvasX(Math.pow(10, exp));
      ctx.fillText(`10^${exp}`, x, PADDING.top + plotH + 8);
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (let exp = yLogMin; exp <= yLogMax; exp++) {
      const y = toCanvasY(Math.pow(10, exp));
      ctx.fillText(`10^${exp}`, PADDING.left - 8, y);
    }

    // Axis titles
    ctx.fillStyle = COLORS.textBright;
    ctx.font = '11px JetBrains Mono, monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillText('Mesh Density (elements)', PADDING.left + plotW / 2, h - 14);

    ctx.save();
    ctx.translate(14, PADDING.top + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillText('Max Displacement (m)', 0, 0);
    ctx.restore();

    // Plot border
    ctx.strokeStyle = COLORS.gridMajor;
    ctx.lineWidth = 1;
    ctx.strokeRect(PADDING.left, PADDING.top, plotW, plotH);

    // Lines connecting dots
    if (points.length > 1) {
      ctx.strokeStyle = COLORS.line;
      ctx.lineWidth = LINE_WIDTH;
      ctx.beginPath();
      const first = points[0];
      ctx.moveTo(toCanvasX(first.x), toCanvasY(first.y));
      for (let i = 1; i < points.length; i++) {
        ctx.lineTo(toCanvasX(points[i].x), toCanvasY(points[i].y));
      }
      ctx.stroke();
    }

    // Data points
    points.forEach((p) => {
      const cx = toCanvasX(p.x);
      const cy = toCanvasY(p.y);

      ctx.fillStyle = COLORS.dot;
      ctx.strokeStyle = COLORS.dotStroke;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.arc(cx, cy, DOT_RADIUS, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
    });

    // Title
    if (title) {
      ctx.fillStyle = COLORS.textBright;
      ctx.font = '12px JetBrains Mono, monospace';
      ctx.textAlign = 'left';
      ctx.textBaseline = 'top';
      ctx.fillText(title, PADDING.left, 8);
    }
  }, [results, title]);

  useEffect(() => {
    draw();
    const handleResize = () => draw();
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, [draw]);

  return (
    <div className="convergence-chart">
      <canvas ref={canvasRef} />
    </div>
  );
}

export default ConvergenceChart;
