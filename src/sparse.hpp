#pragma once
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>

// ==========================================================================
// SPARSE MATRIX -- COO (assembly) and CSR (solve) formats
// ==========================================================================

// ------------------------------------------------------------------
// CSR format: fast SpMV for iterative solvers
// ------------------------------------------------------------------
struct CSRMatrix {
    std::vector<double> values;
    std::vector<int> col_ind;
    std::vector<int> row_ptr;
    int nrows = 0;

    CSRMatrix() = default;

    // Matrix-vector product: y = A * x
    std::vector<double> operator*(const std::vector<double>& x) const {
        if (static_cast<int>(x.size()) != nrows)
            throw std::runtime_error("CSR: dimension mismatch in operator*");
        std::vector<double> y(nrows, 0.0);
        for (int i = 0; i < nrows; ++i) {
            double sum = 0.0;
            for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
                sum += values[k] * x[col_ind[k]];
            }
            y[i] = sum;
        }
        return y;
    }

    // Diagonal element
    double diagonal(int i) const {
        for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
            if (col_ind[k] == i) return values[k];
        }
        return 0.0;
    }

    // Extract diagonal as vector
    std::vector<double> diagonal() const {
        std::vector<double> d(nrows, 0.0);
        for (int i = 0; i < nrows; ++i) {
            d[i] = diagonal(i);
        }
        return d;
    }
};

// ------------------------------------------------------------------
// COO format: natural for element assembly (append-only)
// ------------------------------------------------------------------
struct COOMatrix {
    std::vector<int> row;
    std::vector<int> col;
    std::vector<double> val;
    int nrows = 0;
    int ncols = 0;

    COOMatrix() = default;
    COOMatrix(int n) : nrows(n), ncols(n) {}
    COOMatrix(int nr, int nc) : nrows(nr), ncols(nc) {}

    // Append an entry (allows duplicates, sorted later)
    void add(int r, int c, double v) {
        row.push_back(r);
        col.push_back(c);
        val.push_back(v);
    }

    // Convert to CSR by sorting and compressing duplicates
    CSRMatrix to_csr() const {
        int nnz = static_cast<int>(val.size());

        // Build COO triplets for sorting
        std::vector<std::tuple<int, int, double>> triplets(nnz);
        for (int k = 0; k < nnz; ++k) {
            triplets[k] = {row[k], col[k], val[k]};
        }

        // Sort by (row, col)
        std::sort(triplets.begin(), triplets.end(),
                  [](const auto& a, const auto& b) {
                      if (std::get<0>(a) != std::get<0>(b))
                          return std::get<0>(a) < std::get<0>(b);
                      return std::get<1>(a) < std::get<1>(b);
                  });

        // Compress duplicates: merge entries with same (row, col)
        CSRMatrix csr;
        csr.nrows = nrows;
        csr.values.reserve(nnz);
        csr.col_ind.reserve(nnz);

        int prev_r = -1, prev_c = -1;
        for (int k = 0; k < nnz; ++k) {
            int r = std::get<0>(triplets[k]);
            int c = std::get<1>(triplets[k]);
            double v = std::get<2>(triplets[k]);

            if (r == prev_r && c == prev_c) {
                csr.values.back() += v;
            } else {
                csr.values.push_back(v);
                csr.col_ind.push_back(c);
                prev_r = r;
                prev_c = c;
            }
        }

        // Build row_ptr: count entries per row, then prefix sum
        int nnz_compressed = static_cast<int>(csr.values.size());
        csr.row_ptr.assign(nrows + 1, 0);

        // We need to know which row each compressed entry belongs to.
        // Re-derive from sorted triplets (track row transitions).
        {
            std::vector<int> entry_rows;
            entry_rows.reserve(nnz_compressed);
            int last_r = -1;
            for (int k = 0; k < nnz; ++k) {
                int r = std::get<0>(triplets[k]);
                int c = std::get<1>(triplets[k]);
                if (k == 0 || r != std::get<0>(triplets[k - 1]) ||
                    c != std::get<1>(triplets[k - 1])) {
                    entry_rows.push_back(r);
                }
            }

            for (int r : entry_rows) {
                csr.row_ptr[r + 1]++;
            }
            for (int i = 0; i < nrows; ++i) {
                csr.row_ptr[i + 1] += csr.row_ptr[i];
            }
        }

        return csr;
    }
};

// ------------------------------------------------------------------
// Dense matrix utilities (for Cholesky)
// ------------------------------------------------------------------
struct DenseMatrix {
    int n = 0;
    std::vector<double> data;  // row-major: data[i*n + j]

    DenseMatrix() = default;
    explicit DenseMatrix(int size) : n(size), data(size * size, 0.0) {}

    double& at(int i, int j) { return data[i * n + j]; }
    double at(int i, int j) const { return data[i * n + j]; }

    // Convert sparse CSR to dense
    static DenseMatrix from_csr(const CSRMatrix& csr) {
        DenseMatrix D(csr.nrows);
        for (int i = 0; i < csr.nrows; ++i) {
            for (int k = csr.row_ptr[i]; k < csr.row_ptr[i + 1]; ++k) {
                D.at(i, csr.col_ind[k]) = csr.values[k];
            }
        }
        return D;
    }
};
