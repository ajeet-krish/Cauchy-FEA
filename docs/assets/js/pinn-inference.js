/**
 * FEAPinnSurrogate -- Browser-based PINN inference for 2D linear elasticity.
 *
 * Loads an ONNX model via ONNX Runtime Web and runs real-time inference
 * at any (E, nu, P) parameter combination within the training range.
 *
 * Input: (x, y, E_norm, nu_norm, P_norm) -> (N, 5)
 * Output: (ux, uy, sigma_xx, sigma_yy, sigma_xy, von_mises) -> (N, 6)
 *
 * Usage:
 *   const pinn = new FEAPinnSurrogate('/path/to/pinn_model.onnx');
 *   await pinn.load();
 *   const result = pinn.predict(xArray, yArray, eNorm, nuNorm, pNorm);
 */

class FEAPinnSurrogate {
    constructor(modelPath) {
        this.modelPath = modelPath;
        this.session = null;
        this.loaded = false;

        // Parameter ranges (must match training)
        this.E_MIN = 1e9;
        this.E_MAX = 200e9;
        this.NU_MIN = 0.20;
        this.NU_MAX = 0.45;
        this.P_MIN = 100.0;
        this.P_MAX = 10000.0;
    }

    async load() {
        if (typeof ort === 'undefined') {
            console.error('ONNX Runtime Web not loaded. Include ort.min.js first.');
            return false;
        }
        try {
            this.session = await ort.InferenceSession.create(this.modelPath);
            this.loaded = true;
            console.log(`PINN model loaded: ${this.modelPath}`);
            return true;
        } catch (e) {
            console.error('Failed to load PINN model:', e);
            return false;
        }
    }

    normalizeE(E) {
        return (E - this.E_MIN) / (this.E_MAX - this.E_MIN);
    }

    normalizeNu(nu) {
        return (nu - this.NU_MIN) / (this.NU_MAX - this.NU_MIN);
    }

    normalizeP(P) {
        return (P - this.P_MIN) / (this.P_MAX - this.P_MIN);
    }

    normalizeCoord(val, valMin, valMax) {
        return 2.0 * (val - valMin) / (valMax - valMin) - 1.0;
    }

    /**
     * Run PINN inference on a set of points.
     *
     * @param {Float32Array} x - Physical x coordinates.
     * @param {Float32Array} y - Physical y coordinates.
     * @param {number} E - Young's modulus (Pa).
     * @param {number} nu - Poisson's ratio.
     * @param {number} P - Load magnitude (N).
     * @returns {Object} Prediction results with ux, uy, sigma_xx, sigma_yy, sigma_xy, von_mises arrays.
     */
    predict(x, y, E, nu, P) {
        if (!this.loaded) {
            console.error('Model not loaded. Call load() first.');
            return null;
        }

        const N = x.length;

        // Compute coordinate bounds for normalization
        let xMin = Infinity, xMax = -Infinity;
        let yMin = Infinity, yMax = -Infinity;
        for (let i = 0; i < N; i++) {
            if (x[i] < xMin) xMin = x[i];
            if (x[i] > xMax) xMax = x[i];
            if (y[i] < yMin) yMin = y[i];
            if (y[i] > yMax) yMax = y[i];
        }

        // Build input tensor (N, 5)
        const inputData = new Float32Array(N * 5);
        const eNorm = this.normalizeE(E);
        const nuNorm = this.normalizeNu(nu);
        const pNorm = this.normalizeP(P);

        for (let i = 0; i < N; i++) {
            inputData[i * 5 + 0] = this.normalizeCoord(x[i], xMin, xMax);
            inputData[i * 5 + 1] = this.normalizeCoord(y[i], yMin, yMax);
            inputData[i * 5 + 2] = eNorm;
            inputData[i * 5 + 3] = nuNorm;
            inputData[i * 5 + 4] = pNorm;
        }

        // Run inference
        const inputTensor = new ort.Tensor('float32', inputData, [N, 5]);
        const results = this.session.run({ input: inputTensor });
        const output = results.output.data;

        // Split output (N, 6) into individual fields
        const ux = new Float32Array(N);
        const uy = new Float32Array(N);
        const sigma_xx = new Float32Array(N);
        const sigma_yy = new Float32Array(N);
        const sigma_xy = new Float32Array(N);
        const von_mises = new Float32Array(N);

        for (let i = 0; i < N; i++) {
            ux[i] = output[i * 6 + 0];
            uy[i] = output[i * 6 + 1];
            sigma_xx[i] = output[i * 6 + 2];
            sigma_yy[i] = output[i * 6 + 3];
            sigma_xy[i] = output[i * 6 + 4];
            von_mises[i] = output[i * 6 + 5];
        }

        return { ux, uy, sigma_xx, sigma_yy, sigma_xy, von_mises };
    }

    /**
     * Predict displacement at a single point.
     */
    predictPoint(x, y, E, nu, P) {
        const result = this.predict(
            new Float32Array([x]),
            new Float32Array([y]),
            E, nu, P
        );
        if (!result) return null;
        return {
            ux: result.ux[0],
            uy: result.uy[0],
            sigma_xx: result.sigma_xx[0],
            sigma_yy: result.sigma_yy[0],
            sigma_xy: result.sigma_xy[0],
            von_mises: result.von_mises[0],
        };
    }
}

// Export for module usage
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { FEAPinnSurrogate };
}
