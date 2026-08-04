#pragma once
#include "fea_types.hpp"
#include "sparse.hpp"
#include "preconditioners.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// ==========================================================================
// SOLVERS -- Cholesky direct + Conjugate Gradient iterative
// ==========================================================================

// ------------------------------------------------------------------
// Cholesky decomposition: K = L * L^T (for symmetric positive definite)
// Input: dense symmetric matrix, output: lower triangular L
// ------------------------------------------------------------------
struct CholeskySolver {
    int n = 0;
    std::vector<double> L;  // n*n, lower triangular (row-major)

    CholeskySolver() = default;

    // Factor K = L * L^T
    void factor(const DenseMatrix& K) {
        n = K.n;
        L.resize(n * n, 0.0);

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                double sum = 0.0;
                for (int k = 0; k < j; ++k) {
                    sum += L[i * n + k] * L[j * n + k];
                }
                if (i == j) {
                    double diag = K.at(i, i) - sum;
                    if (diag <= 0.0)
                        throw std::runtime_error("Cholesky: matrix is not SPD (non-positive diagonal)");
                    L[i * n + j] = std::sqrt(diag);
                } else {
                    L[i * n + j] = (K.at(i, j) - sum) / L[j * n + j];
                }
            }
        }
    }

    // Solve L * y = b (forward substitution)
    std::vector<double> forward(const std::vector<double>& b) const {
        std::vector<double> y(n, 0.0);
        for (int i = 0; i < n; ++i) {
            double sum = 0.0;
            for (int k = 0; k < i; ++k) {
                sum += L[i * n + k] * y[k];
            }
            y[i] = (b[i] - sum) / L[i * n + i];
        }
        return y;
    }

    // Solve L^T * x = y (back substitution)
    std::vector<double> backward(const std::vector<double>& y) const {
        std::vector<double> x(n, 0.0);
        for (int i = n - 1; i >= 0; --i) {
            double sum = 0.0;
            for (int k = i + 1; k < n; ++k) {
                sum += L[k * n + i] * x[k];
            }
            x[i] = (y[i] - sum) / L[i * n + i];
        }
        return x;
    }

    // Full solve: K * x = b
    std::vector<double> solve(const std::vector<double>& b) const {
        auto y = forward(b);
        return backward(y);
    }
};

// ------------------------------------------------------------------
// Conjugate Gradient solver (for large sparse SPD systems)
// Supports pluggable preconditioners via template parameter
// ------------------------------------------------------------------
struct CGSolver {
    int max_iter = 10000;
    double tol = 1e-10;

    CGSolver() = default;
    CGSolver(int maxit, double tolerance) : max_iter(maxit), tol(tolerance) {}

    struct Result {
        std::vector<double> x;
        int iterations = 0;
        double residual_norm = 0.0;
        bool converged = false;
        preconditioners::PreconditionerType prec_type =
            preconditioners::PreconditionerType::JACOBI;
    };

    // Solve K * x = b using preconditioned CG with a specific preconditioner
    template<typename Preconditioner>
    Result solve(const CSRMatrix& K, const std::vector<double>& b,
                 const Preconditioner& M) const {
        int n = K.nrows;
        if (static_cast<int>(b.size()) != n)
            throw std::runtime_error("CG: dimension mismatch");

        // Initialize
        std::vector<double> x(n, 0.0);
        std::vector<double> r = b;  // r = b - K*x (x=0 initially)
        std::vector<double> z(n, 0.0);

        // z = M^{-1} * r
        M.apply(r, z);

        std::vector<double> p = z;
        double rz = 0.0;
        for (int i = 0; i < n; ++i) rz += r[i] * z[i];

        double b_norm = 0.0;
        for (int i = 0; i < n; ++i) b_norm += b[i] * b[i];
        b_norm = std::sqrt(b_norm);

        Result result;
        result.x = x;

        for (int iter = 0; iter < max_iter; ++iter) {
            auto Kp = K * p;
            double pKp = 0.0;
            for (int i = 0; i < n; ++i) pKp += p[i] * Kp[i];

            if (std::abs(pKp) < 1e-30) break;

            double alpha = rz / pKp;

            for (int i = 0; i < n; ++i) {
                x[i] += alpha * p[i];
                r[i] -= alpha * Kp[i];
            }

            double r_norm = 0.0;
            for (int i = 0; i < n; ++i) r_norm += r[i] * r[i];
            r_norm = std::sqrt(r_norm);

            result.iterations = iter + 1;
            result.residual_norm = r_norm;

            if (b_norm > 0.0 ? (r_norm / b_norm < tol) : (r_norm < tol)) {
                result.converged = true;
                result.x = x;
                return result;
            }

            // z = M^{-1} * r
            M.apply(r, z);

            double rz_new = 0.0;
            for (int i = 0; i < n; ++i) rz_new += r[i] * z[i];

            double beta = rz_new / rz;
            for (int i = 0; i < n; ++i) {
                p[i] = z[i] + beta * p[i];
            }

            rz = rz_new;
        }

        result.x = x;
        return result;
    }

    // Convenience: solve with Jacobi preconditioner (backward compatible)
    Result solve(const CSRMatrix& K, const std::vector<double>& b) const {
        preconditioners::Jacobi M;
        M.setup(K);
        auto result = solve(K, b, M);
        result.prec_type = preconditioners::PreconditionerType::JACOBI;
        return result;
    }
};
