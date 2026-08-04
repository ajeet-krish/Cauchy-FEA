/**
 * Scientific colormap functions for FEA contour visualization.
 * All functions accept t in [0, 1] and return [r, g, b] in [0, 255].
 */

export function turbo(t: number): [number, number, number] {
  // Placeholder: will implement full turbo colormap in Phase 5
  const r = Math.round(255 * t);
  const g = Math.round(255 * (1 - Math.abs(t - 0.5) * 2));
  const b = Math.round(255 * (1 - t));
  return [r, g, b];
}

export function viridis(t: number): [number, number, number] {
  // Placeholder: will implement full viridis colormap in Phase 5
  const r = Math.round(68 + 187 * t);
  const g = Math.round(1 + 220 * t);
  const b = Math.round(84 + 100 * (1 - t));
  return [r, g, b];
}

export function rdBu(t: number): [number, number, number] {
  // Placeholder: will implement full RdBu_r colormap in Phase 5
  // Blue (t=0) to White (t=0.5) to Red (t=1)
  if (t < 0.5) {
    const s = t * 2;
    const r = Math.round(5 + 250 * s);
    const g = Math.round(100 + 155 * s);
    const b = Math.round(200 + 55 * s);
    return [r, g, b];
  }
  const s = (t - 0.5) * 2;
  const r = Math.round(255 - 5 * s);
  const g = Math.round(255 - 155 * s);
  const b = Math.round(255 - 200 * s);
  return [r, g, b];
}
