#pragma once
#include "sparse.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <numeric>

// ==========================================================================
// PRECONDITIONERS -- Jacobi, IC(0), SSOR, Block Jacobi (Additive Schwarz)
// ==========================================================================

namespace preconditioners {

// ------------------------------------------------------------------
// Preconditioner interface (concept-like for C++20)
// All preconditioners implement:
//   void setup(const CSRMatrix& A)
//   void apply(const std::vector<double>& r, std::vector<double>& z) const
// ------------------------------------------------------------------

// ------------------------------------------------------------------
// Jacobi preconditioner: M = diag(A), z = D^{-1} r
// Fastest setup, weakest convergence acceleration
// ------------------------------------------------------------------
struct Jacobi {
    std::vector<double> d_inv;

    void setup(const CSRMatrix& A) {
        d_inv.resize(A.nrows);
        for (int i = 0; i < A.nrows; ++i) {
            double d = A.diagonal(i);
            d_inv[i] = (std::abs(d) > 1e-15) ? (1.0 / d) : 1.0;
        }
    }

    void apply(const std::vector<double>& r, std::vector<double>& z) const {
        for (size_t i = 0; i < r.size(); ++i) {
            z[i] = d_inv[i] * r[i];
        }
    }
};

// ------------------------------------------------------------------
// SSOR preconditioner: Symmetric Successive Over-Relaxation
// z = (D + omega*L)^{-1} * (omega*D) * (D + omega*U)^{-1} * r
// where A = D + L + U (D diagonal, L strictly lower, U strictly upper)
//
// Implementation uses forward/backward sweeps:
//   Forward:  (D + omega*L) z_temp = omega * D * r
//   Backward: (D + omega*U) z     = z_temp
//
// omega = 1.0 reduces to Gauss-Seidel
// omega in (0, 2) for convergence; optimal omega depends on spectral radius
// ------------------------------------------------------------------
struct SSOR {
    double omega = 1.0;
    std::vector<double> diag;       // diagonal of A
    std::vector<double> omega_diag; // omega * diagonal

    // CSR representation of L and U parts
    std::vector<double> L_vals;
    std::vector<int> L_cols;
    std::vector<int> L_row_ptr;
    std::vector<double> U_vals;
    std::vector<int> U_cols;
    std::vector<int> U_row_ptr;

    SSOR() = default;
    explicit SSOR(double w) : omega(w) {}

    void setup(const CSRMatrix& A) {
        diag.resize(A.nrows);
        omega_diag.resize(A.nrows);
        L_row_ptr.assign(A.nrows + 1, 0);
        U_row_ptr.assign(A.nrows + 1, 0);

        // Count L and U entries per row
        for (int i = 0; i < A.nrows; ++i) {
            diag[i] = A.diagonal(i);
            omega_diag[i] = omega * diag[i];

            for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
                int j = A.col_ind[k];
                if (j < i) L_row_ptr[i + 1]++;       // strictly lower
                else if (j > i) U_row_ptr[i + 1]++;  // strictly upper
            }
        }

        // Prefix sum
        for (int i = 0; i < A.nrows; ++i) {
            L_row_ptr[i + 1] += L_row_ptr[i];
            U_row_ptr[i + 1] += U_row_ptr[i];
        }

        int nnz_L = L_row_ptr[A.nrows];
        int nnz_U = U_row_ptr[A.nrows];
        L_vals.resize(nnz_L);
        L_cols.resize(nnz_L);
        U_vals.resize(nnz_U);
        U_cols.resize(nnz_U);

        // Fill L and U
        std::vector<int> L_pos(A.nrows + 1);
        std::vector<int> U_pos(A.nrows + 1);
        std::copy(L_row_ptr.begin(), L_row_ptr.end(), L_pos.begin());
        std::copy(U_row_ptr.begin(), U_row_ptr.end(), U_pos.begin());

        for (int i = 0; i < A.nrows; ++i) {
            for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
                int j = A.col_ind[k];
                double v = A.values[k];
                if (j < i) {
                    L_vals[L_pos[i]] = omega * v;
                    L_cols[L_pos[i]] = j;
                    L_pos[i]++;
                } else if (j > i) {
                    U_vals[U_pos[i]] = omega * v;
                    U_cols[U_pos[i]] = j;
                    U_pos[i]++;
                }
            }
        }
    }

    // Forward sweep: (D + omega*L) z_temp = omega * D * r
    std::vector<double> forward(const std::vector<double>& r) const {
        int n = static_cast<int>(r.size());
        std::vector<double> z(n, 0.0);

        for (int i = 0; i < n; ++i) {
            double sum = omega_diag[i] * r[i];
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                sum -= L_vals[k] * z[L_cols[k]];
            }
            z[i] = (std::abs(diag[i]) > 1e-15) ? (sum / diag[i]) : 0.0;
        }
        return z;
    }

    // Backward sweep: (D + omega*U) z = z_temp
    std::vector<double> backward(const std::vector<double>& z_temp) const {
        int n = static_cast<int>(z_temp.size());
        std::vector<double> z(n, 0.0);

        for (int i = n - 1; i >= 0; --i) {
            double sum = z_temp[i];
            for (int k = U_row_ptr[i]; k < U_row_ptr[i + 1]; ++k) {
                sum -= U_vals[k] * z[U_cols[k]];
            }
            z[i] = (std::abs(diag[i]) > 1e-15) ? (sum / diag[i]) : 0.0;
        }
        return z;
    }

    void apply(const std::vector<double>& r, std::vector<double>& z) const {
        auto z_temp = forward(r);
        z = backward(z_temp);
    }
};

// ------------------------------------------------------------------
// Incomplete Cholesky IC(0): M = L * L^T where L has same sparsity as A
//
// Algorithm (Cholesky factorization with zero fill-in):
//   For each row i:
//     For each nonzero (i,j) with j < i:
//       L[i][j] = (A[i][j] - sum_k L[i][k]*L[j][k]) / L[j][j]
//       where k runs over common nonzeros of rows i and j in L
//     L[i][i] = sqrt(A[i][i] - sum_k L[i][k]^2)
//
// Solve M*z = r:
//   Forward:  L * y = r
//   Backward: L^T * z = y
// ------------------------------------------------------------------
struct IncompleteCholesky {
    int n = 0;
    std::vector<double> L_vals;
    std::vector<int> L_cols;
    std::vector<int> L_row_ptr;

    void setup(const CSRMatrix& A) {
        n = A.nrows;
        if (n == 0) return;

        // Build CSR lower-triangular part of A (including diagonal)
        // This is the sparsity pattern we keep
        std::vector<double> a_vals;
        std::vector<int> a_cols;
        std::vector<int> a_row_ptr(n + 1, 0);

        for (int i = 0; i < n; ++i) {
            for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
                if (A.col_ind[k] <= i) {
                    a_vals.push_back(A.values[k]);
                    a_cols.push_back(A.col_ind[k]);
                }
            }
            a_row_ptr[i + 1] = static_cast<int>(a_vals.size());
        }

        // IC(0) factorization
        L_vals.resize(a_vals.size());
        L_cols.resize(a_cols.size());
        L_row_ptr = a_row_ptr;

        std::copy(a_vals.begin(), a_vals.end(), L_vals.begin());
        std::copy(a_cols.begin(), a_cols.end(), L_cols.begin());

        for (int i = 0; i < n; ++i) {
            // For each j < i in row i
            for (int ii = L_row_ptr[i]; ii < L_row_ptr[i + 1]; ++ii) {
                int j = L_cols[ii];
                if (j >= i) break;  // only lower triangular

                double sum = 0.0;
                // Compute dot product of L[i] and L[j] over common columns
                // Both rows are sorted by column index
                int jj = L_row_ptr[j];
                for (int ik = L_row_ptr[i]; ik < ii; ++ik) {
                    int col = L_cols[ik];
                    // Advance jj to find matching column
                    while (jj < L_row_ptr[j + 1] && L_cols[jj] < col) ++jj;
                    if (jj < L_row_ptr[j + 1] && L_cols[jj] == col) {
                        sum += L_vals[ik] * L_vals[jj];
                    }
                }

                // Find diagonal of row j
                double diag_j = 0.0;
                for (int k = L_row_ptr[j]; k < L_row_ptr[j + 1]; ++k) {
                    if (L_cols[k] == j) { diag_j = L_vals[k]; break; }
                }

                if (std::abs(diag_j) > 1e-30) {
                    L_vals[ii] = (L_vals[ii] - sum) / diag_j;
                } else {
                    L_vals[ii] = 0.0;
                }
            }

            // Compute diagonal: L[i][i] = sqrt(A[i][i] - sum_k L[i][k]^2)
            double sum_sq = 0.0;
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                if (L_cols[k] < i) {
                    sum_sq += L_vals[k] * L_vals[k];
                }
            }

            double diag_val = a_vals[a_row_ptr[i]]; // diagonal is first entry (sorted, j<=i)
            // Find diagonal explicitly
            for (int k = a_row_ptr[i]; k < a_row_ptr[i + 1]; ++k) {
                if (a_cols[k] == i) { diag_val = a_vals[k]; break; }
            }

            double diag_ii = diag_val - sum_sq;
            if (diag_ii <= 0.0) {
                // Fallback: use original diagonal (incomplete factorization may not exist)
                diag_ii = std::abs(diag_val);
                if (diag_ii < 1e-15) diag_ii = 1e-10;
            }

            // Set diagonal in L
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                if (L_cols[k] == i) {
                    L_vals[k] = std::sqrt(diag_ii);
                    break;
                }
            }
        }
    }

    // Forward solve: L * y = r
    std::vector<double> forward(const std::vector<double>& r) const {
        std::vector<double> y(n, 0.0);
        for (int i = 0; i < n; ++i) {
            double sum = r[i];
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                if (L_cols[k] < i) {
                    sum -= L_vals[k] * y[L_cols[k]];
                } else {
                    break;
                }
            }
            // Find diagonal
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                if (L_cols[k] == i) {
                    y[i] = sum / L_vals[k];
                    break;
                }
            }
        }
        return y;
    }

    // Backward solve: L^T * z = y
    std::vector<double> backward(const std::vector<double>& y) const {
        std::vector<double> z(n, 0.0);
        // Copy y to z initially
        z = y;

        // Back-substitution: z[i] = (y[i] - sum_j L[j][i]*z[j]) / L[i][i]
        // Since we store L in CSR, L[j][i] means column i in row j
        // We need to traverse rows in reverse
        for (int i = n - 1; i >= 0; --i) {
            // Find diagonal
            double diag_val = 1.0;
            for (int k = L_row_ptr[i]; k < L_row_ptr[i + 1]; ++k) {
                if (L_cols[k] == i) { diag_val = L_vals[k]; break; }
            }

            // Subtract contribution from rows below that have column i
            // For each row j > i that has L[j][i] nonzero:
            for (int j = i + 1; j < n; ++j) {
                for (int k = L_row_ptr[j]; k < L_row_ptr[j + 1]; ++k) {
                    if (L_cols[k] == i) {
                        z[i] -= L_vals[k] * z[j];
                        break;
                    }
                }
            }

            z[i] /= diag_val;
        }
        return z;
    }

    void apply(const std::vector<double>& r, std::vector<double>& z) const {
        auto y = forward(r);
        z = backward(y);
    }
};

// ------------------------------------------------------------------
// Block Jacobi (Additive Schwarz): partition DOFs into overlapping blocks,
// solve each block independently.
//
// For FEA meshes: natural partition by node index ranges (block size = b).
// Block i handles DOFs [i*b, (i+1)*b).
// Overlap: each block extends `overlap` DOFs on each side.
//
// For each block:
//   Extract submatrix A_II and RHS r_I
//   Solve A_II * z_I = r_I via dense Cholesky
//   Scatter z_I back to global z
//
// In practice, for FEA problems, block size = DOF_PER_NODE (2 or 3)
// gives good results since DOFs at a node are strongly coupled.
// ------------------------------------------------------------------
struct BlockJacobi {
    int block_size = 2;   // DOFs per block (default: 2 for 2D)
    int overlap = 0;      // overlap in DOFs (0 = non-overlapping)

    // Block data
    struct Block {
        std::vector<int> global_dofs;   // mapping: local -> global
        std::vector<double> K_dense;    // dense submatrix (row-major)
        std::vector<int> pivot;         // for LU solve
    };
    std::vector<Block> blocks;

    BlockJacobi() = default;
    BlockJacobi(int bs, int ol = 0) : block_size(bs), overlap(ol) {}

    void setup(const CSRMatrix& A) {
        int n = A.nrows;
        blocks.clear();

        // Partition DOFs into blocks
        int num_blocks = (n + block_size - 1) / block_size;
        blocks.resize(num_blocks);

        for (int b = 0; b < num_blocks; ++b) {
            int start = b * block_size - overlap;
            int end = std::min((b + 1) * block_size + overlap, n);
            start = std::max(start, 0);

            int block_n = end - start;
            blocks[b].global_dofs.resize(block_n);
            std::iota(blocks[b].global_dofs.begin(), blocks[b].global_dofs.end(), start);

            // Extract dense submatrix
            blocks[b].K_dense.resize(block_n * block_n, 0.0);
            blocks[b].pivot.resize(block_n);

            for (int li = 0; li < block_n; ++li) {
                int gi = blocks[b].global_dofs[li];
                for (int k = A.row_ptr[gi]; k < A.row_ptr[gi + 1]; ++k) {
                    int gj = A.col_ind[k];
                    if (gj >= start && gj < end) {
                        int lj = gj - start;
                        blocks[b].K_dense[li * block_n + lj] = A.values[k];
                    }
                }
            }

            // LU factorization (in-place, no pivoting for SPD)
            lu_factorize(blocks[b].K_dense.data(), blocks[b].pivot.data(), block_n);
        }
    }

    // Simple LU factorization for dense block (SPD, no pivoting needed)
    static void lu_factorize(double* A, int* pivot, int n) {
        for (int i = 0; i < n; ++i) pivot[i] = i;

        for (int k = 0; k < n; ++k) {
            // Find pivot (for numerical stability)
            int max_row = k;
            double max_val = std::abs(A[k * n + k]);
            for (int i = k + 1; i < n; ++i) {
                if (std::abs(A[i * n + k]) > max_val) {
                    max_val = std::abs(A[i * n + k]);
                    max_row = i;
                }
            }
            if (max_row != k) {
                for (int j = 0; j < n; ++j) {
                    std::swap(A[k * n + j], A[max_row * n + j]);
                }
                std::swap(pivot[k], pivot[max_row]);
            }

            if (std::abs(A[k * n + k]) < 1e-30) {
                A[k * n + k] = 1e-10;  // regularize
            }

            // Eliminate below
            for (int i = k + 1; i < n; ++i) {
                double factor = A[i * n + k] / A[k * n + k];
                A[i * n + k] = factor;
                for (int j = k + 1; j < n; ++j) {
                    A[i * n + j] -= factor * A[k * n + j];
                }
            }
        }
    }

    // Solve block system via LU
    static std::vector<double> lu_solve(const double* LU, const int* pivot, int n,
                                         const std::vector<double>& b) {
        std::vector<double> x(n);

        // Forward substitution with row swaps
        for (int i = 0; i < n; ++i) x[i] = b[pivot[i]];
        for (int i = 1; i < n; ++i) {
            for (int j = 0; j < i; ++j) {
                x[i] -= LU[i * n + j] * x[j];
            }
        }

        // Back substitution
        for (int i = n - 1; i >= 0; --i) {
            for (int j = i + 1; j < n; ++j) {
                x[i] -= LU[i * n + j] * x[j];
            }
            x[i] /= LU[i * n + i];
        }

        return x;
    }

    void apply(const std::vector<double>& r, std::vector<double>& z) const {
        for (size_t i = 0; i < z.size(); ++i) z[i] = 0.0;

        for (const auto& blk : blocks) {
            int block_n = static_cast<int>(blk.global_dofs.size());

            // Extract block RHS
            std::vector<double> r_block(block_n);
            for (int i = 0; i < block_n; ++i) {
                r_block[i] = r[blk.global_dofs[i]];
            }

            // Solve block
            auto z_block = lu_solve(blk.K_dense.data(), blk.pivot.data(), block_n, r_block);

            // Scatter back
            for (int i = 0; i < block_n; ++i) {
                z[blk.global_dofs[i]] = z_block[i];
            }
        }
    }
};

// ------------------------------------------------------------------
// Preconditioner selection: auto-select based on matrix properties
// ------------------------------------------------------------------
enum class PreconditionerType { JACOBI, SSOR, IC0, BLOCK_JACOBI };

inline const char* preconditioner_name(PreconditionerType t) {
    switch (t) {
        case PreconditionerType::JACOBI:       return "Jacobi";
        case PreconditionerType::SSOR:         return "SSOR";
        case PreconditionerType::IC0:          return "IC(0)";
        case PreconditionerType::BLOCK_JACOBI: return "Block Jacobi";
    }
    return "Unknown";
}

// Estimate condition number via Gershgorin circle theorem (rough)
inline double estimate_spectral_radius(const CSRMatrix& A) {
    double max_radius = 0.0;
    for (int i = 0; i < A.nrows; ++i) {
        double diag = 0.0;
        double off_diag_sum = 0.0;
        for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
            if (A.col_ind[k] == i) diag = std::abs(A.values[k]);
            else off_diag_sum += std::abs(A.values[k]);
        }
        double radius = off_diag_sum / std::max(diag, 1e-15);
        max_radius = std::max(max_radius, radius);
    }
    return 1.0 + 2.0 * max_radius;  // Gershgorin upper bound
}

}  // namespace preconditioners
