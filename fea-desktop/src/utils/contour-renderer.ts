import type { MeshData, SolveResult, Point } from '../types';
import { turbo, viridis, rdBu, type ColormapFn } from './colormap';

export type ContourField =
  | 'von_mises'
  | 'sigma_xx'
  | 'sigma_yy'
  | 'sigma_xy'
  | 'sigma_1'
  | 'sigma_2'
  | 'ux'
  | 'uy'
  | 'disp_mag';

export const FIELD_LABELS: Record<ContourField, string> = {
  von_mises: 'Von Mises Stress',
  sigma_xx: 'Normal Stress XX',
  sigma_yy: 'Normal Stress YY',
  sigma_xy: 'Shear Stress XY',
  sigma_1: 'Principal Stress 1',
  sigma_2: 'Principal Stress 2',
  ux: 'Displacement X',
  uy: 'Displacement Y',
  disp_mag: 'Displacement Magnitude',
};

export const FIELD_UNITS: Record<ContourField, string> = {
  von_mises: 'Pa',
  sigma_xx: 'Pa',
  sigma_yy: 'Pa',
  sigma_xy: 'Pa',
  sigma_1: 'Pa',
  sigma_2: 'Pa',
  ux: 'm',
  uy: 'm',
  disp_mag: 'm',
};

export const STRESS_FIELDS: ContourField[] = [
  'von_mises',
  'sigma_xx',
  'sigma_yy',
  'sigma_xy',
  'sigma_1',
  'sigma_2',
];

export const DISPLACEMENT_FIELDS: ContourField[] = ['ux', 'uy', 'disp_mag'];

/**
 * Get the colormap function appropriate for a given field.
 */
function getColormapForField(field: ContourField): ColormapFn {
  if (field === 'sigma_xy') return rdBu;
  if (field === 'disp_mag') return viridis;
  return turbo;
}

/**
 * Compute the scalar value for each element based on the selected field.
 * Stress fields are element-indexed; displacement fields are nodal and averaged.
 */
function computeElementValues(
  mesh: MeshData,
  result: SolveResult,
  field: ContourField,
): number[] {
  const nElem = mesh.elements.length;
  const values = new Array<number>(nElem);

  // Element-indexed stress fields
  if (field === 'von_mises') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.von_mises ?? 0;
    }
    return values;
  }
  if (field === 'sigma_xx') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.sigma_xx ?? 0;
    }
    return values;
  }
  if (field === 'sigma_yy') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.sigma_yy ?? 0;
    }
    return values;
  }
  if (field === 'sigma_xy') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.sigma_xy ?? 0;
    }
    return values;
  }
  if (field === 'sigma_1') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.sigma_1 ?? 0;
    }
    return values;
  }
  if (field === 'sigma_2') {
    for (let i = 0; i < nElem; i++) {
      values[i] = result.stresses[i]?.sigma_2 ?? 0;
    }
    return values;
  }

  // Nodal fields: average across element nodes
  for (let ei = 0; ei < nElem; ei++) {
    const elemNodes = mesh.elements[ei].nodes;
    let sum = 0;
    let count = 0;
    for (const ni of elemNodes) {
      if (ni < 0 || ni >= result.displacements.length) continue;
      const d = result.displacements[ni];
      if (field === 'ux') {
        sum += d.ux;
      } else if (field === 'uy') {
        sum += d.uy;
      } else {
        // disp_mag
        sum += Math.sqrt(d.ux * d.ux + d.uy * d.uy);
      }
      count++;
    }
    values[ei] = count > 0 ? sum / count : 0;
  }
  return values;
}

/**
 * Compute the min and max of an array of numbers.
 */
function arrayRange(arr: number[]): { min: number; max: number } {
  if (arr.length === 0) return { min: 0, max: 1 };
  let min = Infinity;
  let max = -Infinity;
  for (const v of arr) {
    if (v < min) min = v;
    if (v > max) max = v;
  }
  if (min === max) {
    // Constant field: spread the range so colorbar is still visible
    const mag = Math.abs(min) || 1;
    return { min: min - mag * 0.5, max: max + mag * 0.5 };
  }
  return { min, max };
}

/**
 * Render element-based contour coloring onto a 2D canvas context.
 *
 * @param ctx Canvas 2D rendering context
 * @param mesh Mesh data with nodes and element connectivity
 * @param result Solve result with displacements and stresses
 * @param field Which scalar field to visualize
 * @param toScreen Function mapping world coordinates to screen coordinates
 * @param deformedNodes Optional deformed node positions (null = undeformed)
 */
export function renderContours(
  ctx: CanvasRenderingContext2D,
  mesh: MeshData,
  result: SolveResult,
  field: ContourField,
  toScreen: (pt: Point) => { sx: number; sy: number },
  deformedNodes?: Point[] | null,
): void {
  const values = computeElementValues(mesh, result, field);
  const { min, max } = arrayRange(values);
  const range = max - min;
  const colormap = getColormapForField(field);

  const EDGE_COLOR = '#1c2128';
  const EDGE_WIDTH = 0.6;

  // Draw filled elements
  for (let ei = 0; ei < mesh.elements.length; ei++) {
    const elem = mesh.elements[ei];
    const nodeIndices = elem.nodes;
    if (nodeIndices.length < 3) continue;

    // Compute normalized value
    const t = range !== 0 ? (values[ei] - min) / range : 0.5;
    const [r, g, b] = colormap(t);
    ctx.fillStyle = `rgb(${r},${g},${b})`;

    ctx.beginPath();
    for (let ni = 0; ni < nodeIndices.length; ni++) {
      const nodeIdx = nodeIndices[ni];
      const pt =
        deformedNodes && nodeIdx < deformedNodes.length
          ? deformedNodes[nodeIdx]
          : mesh.nodes[nodeIdx];
      const { sx, sy } = toScreen(pt);
      if (ni === 0) {
        ctx.moveTo(sx, sy);
      } else {
        ctx.lineTo(sx, sy);
      }
    }
    ctx.closePath();
    ctx.fill();
  }

  // Draw element edges on top
  ctx.strokeStyle = EDGE_COLOR;
  ctx.lineWidth = EDGE_WIDTH;
  for (const elem of mesh.elements) {
    const nodeIndices = elem.nodes;
    if (nodeIndices.length < 3) continue;
    ctx.beginPath();
    for (let ni = 0; ni < nodeIndices.length; ni++) {
      const nodeIdx = nodeIndices[ni];
      const pt =
        deformedNodes && nodeIdx < deformedNodes.length
          ? deformedNodes[nodeIdx]
          : mesh.nodes[nodeIdx];
      const { sx, sy } = toScreen(pt);
      if (ni === 0) {
        ctx.moveTo(sx, sy);
      } else {
        ctx.lineTo(sx, sy);
      }
    }
    ctx.closePath();
    ctx.stroke();
  }
}

/**
 * Format a numeric value for axis/colorbar labels.
 * Uses SI prefixes for large/small values.
 */
function formatValue(value: number): string {
  if (value === 0) return '0';
  const abs = Math.abs(value);
  if (abs >= 1e9) return (value / 1e9).toFixed(1) + 'G';
  if (abs >= 1e6) return (value / 1e6).toFixed(2) + 'M';
  if (abs >= 1e3) return (value / 1e3).toFixed(1) + 'k';
  if (abs >= 1) return value.toFixed(2);
  if (abs >= 1e-3) return (value * 1e3).toFixed(1) + 'm';
  if (abs >= 1e-6) return (value * 1e6).toFixed(1) + 'u';
  return value.toExponential(1);
}

/**
 * Draw a vertical colorbar on the right side of the canvas.
 *
 * @param ctx Canvas 2D rendering context
 * @param colormap Color mapping function
 * @param min Minimum value of the field
 * @param max Maximum value of the field
 * @param unit Unit string for the field
 * @param canvasWidth Width of the canvas
 * @param canvasHeight Height of the canvas
 */
export function drawColorbar(
  ctx: CanvasRenderingContext2D,
  colormap: ColormapFn,
  min: number,
  max: number,
  unit: string,
  canvasWidth: number,
  canvasHeight: number,
): void {
  const barWidth = 16;
  const barHeight = Math.min(320, canvasHeight - 80);
  const barX = canvasWidth - 60;
  const barY = (canvasHeight - barHeight) / 2;
  const numSamples = 128;

  // Background panel
  ctx.fillStyle = 'rgba(13, 17, 23, 0.88)';
  ctx.fillRect(barX - 36, barY - 30, barWidth + 80, barHeight + 60);
  ctx.strokeStyle = '#21262d';
  ctx.lineWidth = 1;
  ctx.strokeRect(barX - 36, barY - 30, barWidth + 80, barHeight + 60);

  // Draw gradient bar (top = max, bottom = min)
  for (let i = 0; i < numSamples; i++) {
    const t = 1 - i / (numSamples - 1); // top = max (t=1), bottom = min (t=0)
    const [r, g, b] = colormap(t);
    const y = barY + (i / (numSamples - 1)) * barHeight;
    const h = Math.ceil(barHeight / numSamples) + 1;
    ctx.fillStyle = `rgb(${r},${g},${b})`;
    ctx.fillRect(barX, y, barWidth, h);
  }

  // Border around gradient bar
  ctx.strokeStyle = '#30363d';
  ctx.lineWidth = 1;
  ctx.strokeRect(barX, barY, barWidth, barHeight);

  // Tick marks and labels (5 ticks: min, 25%, 50%, 75%, max)
  const numTicks = 5;
  ctx.fillStyle = '#c9d1d9';
  ctx.font = '10px JetBrains Mono, monospace';
  ctx.textBaseline = 'middle';

  for (let i = 0; i < numTicks; i++) {
    // Tick 0 = top = max, tick 4 = bottom = min
    const tickT = 1 - i / (numTicks - 1);
    const value = min + tickT * (max - min);
    const y = barY + (i / (numTicks - 1)) * barHeight;

    // Tick line
    ctx.strokeStyle = '#484f58';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(barX - 4, y);
    ctx.lineTo(barX, y);
    ctx.stroke();

    // Label (right of the bar)
    ctx.textAlign = 'left';
    ctx.fillText(formatValue(value), barX + barWidth + 6, y);
  }

  // Unit label at bottom
  ctx.fillStyle = '#8b949e';
  ctx.font = '9px JetBrains Mono, monospace';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  ctx.fillText(unit, barX + barWidth / 2, barY + barHeight + 6);
}

/**
 * Get the colormap and range for a field (used by ResultsCanvas for colorbar).
 */
export function getFieldColormap(field: ContourField): ColormapFn {
  return getColormapForField(field);
}

/**
 * Compute the range of a field across the mesh (used by ResultsCanvas for colorbar).
 */
export function computeFieldRange(
  mesh: MeshData,
  result: SolveResult,
  field: ContourField,
): { min: number; max: number } {
  const values = computeElementValues(mesh, result, field);
  return arrayRange(values);
}
