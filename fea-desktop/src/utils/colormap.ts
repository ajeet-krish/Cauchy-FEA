/**
 * Scientific colormap functions for FEA contour visualization.
 * All functions accept t in [0, 1] and return [r, g, b] in [0, 255].
 * Uses control-point interpolation for smooth gradients.
 */

export type ColormapFn = (t: number) => [number, number, number];

interface ColorStop {
  t: number;
  r: number;
  g: number;
  b: number;
}

/**
 * Interpolate between color stops at a given parameter t in [0, 1].
 */
function interpolateStops(stops: ColorStop[], t: number): [number, number, number] {
  const clamped = Math.max(0, Math.min(1, t));
  for (let i = 0; i < stops.length - 1; i++) {
    if (clamped >= stops[i].t && clamped <= stops[i + 1].t) {
      const localT = (clamped - stops[i].t) / (stops[i + 1].t - stops[i].t);
      return [
        Math.round(stops[i].r + (stops[i + 1].r - stops[i].r) * localT),
        Math.round(stops[i].g + (stops[i + 1].g - stops[i].g) * localT),
        Math.round(stops[i].b + (stops[i + 1].b - stops[i].b) * localT),
      ];
    }
  }
  const last = stops[stops.length - 1];
  return [last.r, last.g, last.b];
}

// Turbo: perceptually uniform rainbow (blue -> cyan -> green -> yellow -> red)
const TURBO_STOPS: ColorStop[] = [
  { t: 0.00, r: 48, g: 18, b: 59 },
  { t: 0.05, r: 59, g: 62, b: 130 },
  { t: 0.10, r: 63, g: 106, b: 170 },
  { t: 0.15, r: 53, g: 143, b: 193 },
  { t: 0.20, r: 36, g: 173, b: 198 },
  { t: 0.25, r: 23, g: 196, b: 186 },
  { t: 0.30, r: 20, g: 212, b: 163 },
  { t: 0.35, r: 31, g: 224, b: 133 },
  { t: 0.40, r: 58, g: 233, b: 101 },
  { t: 0.45, r: 98, g: 238, b: 70 },
  { t: 0.50, r: 143, g: 241, b: 44 },
  { t: 0.55, r: 183, g: 239, b: 31 },
  { t: 0.60, r: 214, g: 230, b: 30 },
  { t: 0.65, r: 237, g: 214, b: 29 },
  { t: 0.70, r: 250, g: 190, b: 28 },
  { t: 0.75, r: 254, g: 159, b: 29 },
  { t: 0.80, r: 251, g: 122, b: 32 },
  { t: 0.85, r: 243, g: 86, b: 32 },
  { t: 0.90, r: 227, g: 53, b: 27 },
  { t: 0.95, r: 198, g: 24, b: 16 },
  { t: 1.00, r: 122, g: 4, b: 3 },
];

// Viridis: perceptually uniform (dark purple -> teal -> green -> yellow)
const VIRIDIS_STOPS: ColorStop[] = [
  { t: 0.00, r: 68, g: 1, b: 84 },
  { t: 0.05, r: 72, g: 26, b: 108 },
  { t: 0.10, r: 70, g: 49, b: 127 },
  { t: 0.15, r: 64, g: 71, b: 136 },
  { t: 0.20, r: 55, g: 93, b: 141 },
  { t: 0.25, r: 45, g: 113, b: 142 },
  { t: 0.30, r: 36, g: 132, b: 141 },
  { t: 0.35, r: 30, g: 151, b: 138 },
  { t: 0.40, r: 34, g: 168, b: 131 },
  { t: 0.45, r: 53, g: 183, b: 121 },
  { t: 0.50, r: 82, g: 196, b: 106 },
  { t: 0.55, r: 115, g: 207, b: 86 },
  { t: 0.60, r: 150, g: 216, b: 62 },
  { t: 0.65, r: 185, g: 223, b: 44 },
  { t: 0.70, r: 214, g: 226, b: 39 },
  { t: 0.75, r: 236, g: 225, b: 40 },
  { t: 0.80, r: 249, g: 214, b: 39 },
  { t: 0.85, r: 253, g: 196, b: 39 },
  { t: 0.90, r: 254, g: 173, b: 39 },
  { t: 0.95, r: 254, g: 150, b: 38 },
  { t: 1.00, r: 253, g: 231, b: 37 },
];

// RdBu_r: diverging (red at 0 -> white at 0.5 -> blue at 1)
const RDBU_R_STOPS: ColorStop[] = [
  { t: 0.00, r: 103, g: 0, b: 31 },
  { t: 0.05, r: 126, g: 10, b: 33 },
  { t: 0.10, r: 150, g: 22, b: 37 },
  { t: 0.15, r: 174, g: 39, b: 46 },
  { t: 0.20, r: 196, g: 65, b: 57 },
  { t: 0.25, r: 214, g: 96, b: 74 },
  { t: 0.30, r: 228, g: 128, b: 96 },
  { t: 0.35, r: 239, g: 160, b: 124 },
  { t: 0.40, r: 247, g: 190, b: 160 },
  { t: 0.45, r: 252, g: 216, b: 196 },
  { t: 0.50, r: 247, g: 247, b: 247 },
  { t: 0.55, r: 224, g: 236, b: 243 },
  { t: 0.60, r: 197, g: 222, b: 238 },
  { t: 0.65, r: 165, g: 206, b: 228 },
  { t: 0.70, r: 130, g: 186, b: 216 },
  { t: 0.75, r: 94, g: 162, b: 203 },
  { t: 0.80, r: 62, g: 135, b: 186 },
  { t: 0.85, r: 39, g: 108, b: 168 },
  { t: 0.90, r: 24, g: 82, b: 148 },
  { t: 0.95, r: 12, g: 60, b: 120 },
  { t: 1.00, r: 5, g: 48, b: 97 },
];

export function turbo(t: number): [number, number, number] {
  return interpolateStops(TURBO_STOPS, t);
}

export function viridis(t: number): [number, number, number] {
  return interpolateStops(VIRIDIS_STOPS, t);
}

export function rdBu(t: number): [number, number, number] {
  return interpolateStops(RDBU_R_STOPS, t);
}

/**
 * Convert [r, g, b] in [0, 255] to a CSS rgb() string.
 */
export function rgbToCSS(rgb: [number, number, number]): string {
  return `rgb(${rgb[0]},${rgb[1]},${rgb[2]})`;
}

/**
 * Get a colormap function by name.
 */
export function getColormap(name: string): ColormapFn {
  switch (name) {
    case 'turbo':
      return turbo;
    case 'viridis':
      return viridis;
    case 'rdBu':
      return rdBu;
    default:
      return turbo;
  }
}

/**
 * Get the default colormap for a given field type.
 * Stress fields use turbo (sequential), displacement uses viridis.
 */
export function getDefaultColormap(field: string): string {
  if (field === 'sigma_xy') return 'rdBu';
  return 'turbo';
}
