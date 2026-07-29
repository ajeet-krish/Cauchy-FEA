/**
 * Scientific colormaps for FEA visualization.
 * Based on matplotlib perceptually uniform colormaps.
 *
 * Usage:
 *   const color = Colormaps.turbo(0.5);  // 0..1 normalized value
 *   const [r, g, b] = Colormaps.turbo_rgb(0.5);  // returns [0..255, 0..255, 0..255]
 */

const Colormaps = (() => {
  'use strict';

  // Turbo colormap (256 entries, perceptually uniform rainbow)
  // Derived from matplotlib turbo
  const TURBO_STOPS = [
    [0.000, 0.188, 0.078, 0.380],
    [0.039, 0.251, 0.329, 0.616],
    [0.078, 0.125, 0.580, 0.780],
    [0.118, 0.094, 0.796, 0.804],
    [0.157, 0.165, 0.957, 0.718],
    [0.196, 0.349, 0.984, 0.553],
    [0.235, 0.525, 0.961, 0.365],
    [0.275, 0.682, 0.886, 0.212],
    [0.314, 0.812, 0.769, 0.106],
    [0.353, 0.906, 0.635, 0.051],
    [0.392, 0.961, 0.490, 0.016],
    [0.431, 0.976, 0.345, 0.004],
    [0.471, 0.945, 0.216, 0.008],
    [0.510, 0.875, 0.110, 0.027],
    [0.549, 0.769, 0.039, 0.059],
    [0.588, 0.643, 0.008, 0.102],
    [0.627, 0.506, 0.000, 0.145],
    [0.667, 0.373, 0.000, 0.180],
    [0.706, 0.251, 0.004, 0.208],
    [0.745, 0.149, 0.020, 0.227],
    [0.784, 0.071, 0.047, 0.239],
    [0.824, 0.016, 0.090, 0.243],
    [0.863, 0.000, 0.137, 0.239],
    [0.902, 0.000, 0.192, 0.227],
    [0.941, 0.016, 0.255, 0.208],
    [0.980, 0.047, 0.325, 0.180],
    [1.000, 0.090, 0.396, 0.145]
  ];

  // Viridis colormap (256 entries)
  const VIRIDIS_STOPS = [
    [0.000, 0.267, 0.004, 0.329],
    [0.078, 0.283, 0.141, 0.457],
    [0.157, 0.253, 0.267, 0.537],
    [0.235, 0.188, 0.376, 0.553],
    [0.314, 0.129, 0.475, 0.533],
    [0.392, 0.090, 0.565, 0.482],
    [0.471, 0.106, 0.651, 0.404],
    [0.549, 0.212, 0.718, 0.306],
    [0.627, 0.365, 0.765, 0.196],
    [0.706, 0.541, 0.780, 0.098],
    [0.784, 0.725, 0.765, 0.039],
    [0.863, 0.902, 0.729, 0.008],
    [1.000, 0.996, 0.745, 0.145]
  ];

  // RdBu_r (reversed Red-Blue) for signed fields
  const RDBUR_STOPS = [
    [0.000, 0.047, 0.239, 0.557],
    [0.167, 0.196, 0.471, 0.722],
    [0.333, 0.541, 0.745, 0.839],
    [0.500, 0.867, 0.867, 0.867],
    [0.667, 0.835, 0.533, 0.455],
    [0.833, 0.702, 0.231, 0.216],
    [1.000, 0.620, 0.094, 0.118]
  ];

  // Coolwarm for temperature/stress
  const COOLWARM_STOPS = [
    [0.000, 0.230, 0.299, 0.754],
    [0.250, 0.435, 0.596, 0.835],
    [0.500, 0.865, 0.865, 0.865],
    [0.750, 0.882, 0.549, 0.349],
    [1.000, 0.706, 0.161, 0.137]
  ];

  // Hot (black-red-yellow-white) for stress magnitude
  const HOT_STOPS = [
    [0.000, 0.000, 0.000, 0.000],
    [0.333, 0.906, 0.000, 0.000],
    [0.667, 1.000, 0.878, 0.000],
    [1.000, 1.000, 1.000, 1.000]
  ];

  /**
   * Interpolate a colormap at a normalized value t in [0, 1].
   * Returns {r, g, b} in [0, 1].
   */
  function interpolate(stops, t) {
    t = Math.max(0, Math.min(1, t));

    for (let i = 0; i < stops.length - 1; i++) {
      if (t >= stops[i][0] && t <= stops[i + 1][0]) {
        const t0 = stops[i][0];
        const t1 = stops[i + 1][0];
        const f = (t1 > t0) ? (t - t0) / (t1 - t0) : 0;
        return {
          r: stops[i][1] + f * (stops[i + 1][1] - stops[i][1]),
          g: stops[i][2] + f * (stops[i + 1][2] - stops[i][2]),
          b: stops[i][3] + f * (stops[i + 1][3] - stops[i][3])
        };
      }
    }
    const last = stops[stops.length - 1];
    return { r: last[1], g: last[2], b: last[3] };
  }

  /**
   * Create a colormap function from stops.
   * Returns a function t -> {r, g, b} in [0, 1].
   */
  function createMap(stops) {
    return (t) => interpolate(stops, t);
  }

  /**
   * Get RGB as 0..255 integers.
   */
  function toRGB(fn) {
    return (t) => {
      const c = fn(t);
      return [Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)];
    };
  }

  /**
   * Generate a 256-entry lookup table as a Uint8Array.
   * Layout: [r0, g0, b0, r1, g1, b1, ...] (RGB, 768 bytes).
   */
  function generateLUT(fn) {
    const lut = new Uint8Array(768);
    for (let i = 0; i < 256; i++) {
      const t = i / 255;
      const [r, g, b] = toRGB(fn)(t);
      lut[i * 3] = r;
      lut[i * 3 + 1] = g;
      lut[i * 3 + 2] = b;
    }
    return lut;
  }

  /**
   * Map a scalar value to a CSS rgb() string.
   */
  function toCSS(fn, t) {
    const [r, g, b] = toRGB(fn)(t);
    return `rgb(${r},${g},${b})`;
  }

  // Public API
  return {
    turbo: createMap(TURBO_STOPS),
    viridis: createMap(VIRIDIS_STOPS),
    RdBu_r: createMap(RDBUR_STOPS),
    coolwarm: createMap(COOLWARM_STOPS),
    hot: createMap(HOT_STOPS),

    turbo_rgb: toRGB(createMap(TURBO_STOPS)),
    viridis_rgb: toRGB(createMap(VIRIDIS_STOPS)),
    RdBu_r_rgb: toRGB(createMap(RDBUR_STOPS)),
    coolwarm_rgb: toRGB(createMap(COOLWARM_STOPS)),
    hot_rgb: toRGB(createMap(HOT_STOPS)),

    turbo_css: (t) => toCSS(createMap(TURBO_STOPS), t),
    viridis_css: (t) => toCSS(createMap(VIRIDIS_STOPS), t),
    RdBu_r_css: (t) => toCSS(createMap(RDBUR_STOPS), t),
    coolwarm_css: (t) => toCSS(createMap(COOLWARM_STOPS), t),
    hot_css: (t) => toCSS(createMap(HOT_STOPS), t),

    turbo_lut: generateLUT(createMap(TURBO_STOPS)),
    viridis_lut: generateLUT(createMap(VIRIDIS_STOPS)),
    RdBu_r_lut: generateLUT(createMap(RDBUR_STOPS)),

    /**
     * Map a list of values to a CSS color array using a given colormap.
     * Returns array of rgb() strings.
     */
    mapValues: (fn, values, vmin, vmax) => {
      const range = vmax - vmin;
      return values.map(v => {
        const t = range > 0 ? (v - vmin) / range : 0;
        return toCSS(fn, t);
      });
    }
  };
})();
