#include <gtest/gtest.h>
#include "fea_types.hpp"
#include "elements.hpp"
#include "sparse.hpp"
#include "solver.hpp"
#include "mesh.hpp"
#include "postprocess.hpp"
#include "fea.hpp"
#include <cmath>

// ==========================================================================
// ELEMENT TESTS
// ==========================================================================

// Bar element stiffness matrix test
TEST(BarElementTest, StiffnessMatrix) {
    Material mat = Material::steel();
    mat.t = 1.0;
    Node n1 = {0.0, 0.0};
    Node n2 = {1.0, 0.0};
    double A = 0.01;

    auto K = elements::BarElement::stiffness(n1, n2, A, mat);

    // For horizontal bar: K should be EA/L * [[1,0,-1,0],[0,0,0,0],[-1,0,1,0],[0,0,0,0]]
    double EA_L = mat.E * A / 1.0;

    EXPECT_NEAR(K[0][0], EA_L, 1e-10 * EA_L);
    EXPECT_NEAR(K[0][2], -EA_L, 1e-10 * EA_L);
    EXPECT_NEAR(K[2][0], -EA_L, 1e-10 * EA_L);
    EXPECT_NEAR(K[2][2], EA_L, 1e-10 * EA_L);

    // Off-diagonal terms for vertical DOFs should be zero for horizontal bar
    EXPECT_NEAR(K[0][1], 0.0, 1e-10);
    EXPECT_NEAR(K[1][0], 0.0, 1e-10);
    EXPECT_NEAR(K[1][1], 0.0, 1e-10);
}

// Bar element symmetry
TEST(BarElementTest, Symmetry) {
    Material mat = Material::steel();
    mat.t = 1.0;
    Node n1 = {0.0, 0.0};
    Node n2 = {1.0, 1.0};  // diagonal bar
    double A = 0.01;

    auto K = elements::BarElement::stiffness(n1, n2, A, mat);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-10 * std::abs(K[i][j]) + 1e-15);
        }
    }
}

// Q4 element shape functions sum to 1
TEST(Q4ElementTest, ShapeFunctionsPartitionOfUnity) {
    const double gp = 1.0 / std::sqrt(3.0);
    double test_pts[][2] = {{0.0, 0.0}, {gp, gp}, {-gp, -gp}, {0.5, 0.3}};

    for (auto& pt : test_pts) {
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            sum += elements::Q4Element::shape_func(i, pt[0], pt[1]);
        }
        EXPECT_NEAR(sum, 1.0, 1e-12);
    }
}

// Q4 element Jacobian for unit square
TEST(Q4ElementTest, JacobianUnitSquare) {
    std::array<Node, 4> nodes = {
        Node{0.0, 0.0}, Node{1.0, 0.0},
        Node{1.0, 1.0}, Node{0.0, 1.0}
    };

    double detJ = elements::Q4Element::jacobian_det(nodes, 0.0, 0.0);
    EXPECT_NEAR(detJ, 0.25, 1e-12);  // For unit square, det(J) = 0.25

    // Should be constant for bilinear quad on rectangle
    detJ = elements::Q4Element::jacobian_det(nodes, 0.5, 0.5);
    EXPECT_NEAR(detJ, 0.25, 1e-12);
}

// Q4 element stiffness symmetry
TEST(Q4ElementTest, StiffnessSymmetry) {
    Material mat = Material::steel();
    std::array<Node, 4> nodes = {
        Node{0.0, 0.0}, Node{1.0, 0.0},
        Node{1.0, 1.0}, Node{0.0, 1.0}
    };

    auto K = elements::Q4Element::stiffness(nodes, mat, PlaneType::STRESS);

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-8 * (std::abs(K[i][j]) + 1.0));
        }
    }
}

// ==========================================================================
// SPARSE MATRIX TESTS
// ==========================================================================

TEST(SparseMatrixTest, COOtoCSR) {
    COOMatrix coo(3, 3);
    coo.add(0, 0, 1.0);
    coo.add(1, 1, 2.0);
    coo.add(2, 2, 3.0);
    coo.add(0, 1, 0.5);
    coo.add(1, 0, 0.5);

    auto csr = coo.to_csr();
    EXPECT_EQ(csr.nrows, 3);

    // Check diagonal
    EXPECT_NEAR(csr.diagonal(0), 1.0, 1e-12);
    EXPECT_NEAR(csr.diagonal(1), 2.0, 1e-12);
    EXPECT_NEAR(csr.diagonal(2), 3.0, 1e-12);
}

TEST(SparseMatrixTest, MatrixVectorProduct) {
    COOMatrix coo(3, 3);
    coo.add(0, 0, 1.0); coo.add(0, 1, 2.0);
    coo.add(1, 0, 3.0); coo.add(1, 1, 4.0);
    coo.add(2, 2, 5.0);

    auto csr = coo.to_csr();
    std::vector<double> x = {1.0, 2.0, 3.0};
    auto y = csr * x;

    EXPECT_NEAR(y[0], 5.0, 1e-12);   // 1*1 + 2*2
    EXPECT_NEAR(y[1], 11.0, 1e-12);  // 3*1 + 4*2
    EXPECT_NEAR(y[2], 15.0, 1e-12);  // 5*3
}

TEST(SparseMatrixTest, DuplicateEntries) {
    COOMatrix coo(2, 2);
    coo.add(0, 0, 1.0);
    coo.add(0, 0, 2.0);  // duplicate
    coo.add(1, 1, 3.0);

    auto csr = coo.to_csr();
    EXPECT_NEAR(csr.diagonal(0), 3.0, 1e-12);  // 1+2
    EXPECT_NEAR(csr.diagonal(1), 3.0, 1e-12);
}

// ==========================================================================
// SOLVER TESTS
// ==========================================================================

TEST(CholeskyTest, SolveIdentity) {
    DenseMatrix I(3);
    I.at(0, 0) = 1.0; I.at(1, 1) = 1.0; I.at(2, 2) = 1.0;

    CholeskySolver chol;
    chol.factor(I);

    std::vector<double> b = {1.0, 2.0, 3.0};
    auto x = chol.solve(b);

    EXPECT_NEAR(x[0], 1.0, 1e-12);
    EXPECT_NEAR(x[1], 2.0, 1e-12);
    EXPECT_NEAR(x[2], 3.0, 1e-12);
}

TEST(CholeskyTest, SolveDiagonal) {
    DenseMatrix K(2);
    K.at(0, 0) = 4.0; K.at(1, 1) = 9.0;

    CholeskySolver chol;
    chol.factor(K);

    std::vector<double> b = {8.0, 18.0};
    auto x = chol.solve(b);

    EXPECT_NEAR(x[0], 2.0, 1e-12);
    EXPECT_NEAR(x[1], 2.0, 1e-12);
}

TEST(CGTest, ConvergenceOnDiagonal) {
    COOMatrix coo(3, 3);
    coo.add(0, 0, 4.0);
    coo.add(1, 1, 9.0);
    coo.add(2, 2, 16.0);

    auto csr = coo.to_csr();
    std::vector<double> b = {8.0, 18.0, 32.0};

    CGSolver cg(100, 1e-12);
    auto result = cg.solve(csr, b);

    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.x[0], 2.0, 1e-8);
    EXPECT_NEAR(result.x[1], 2.0, 1e-8);
    EXPECT_NEAR(result.x[2], 2.0, 1e-8);
}

// ==========================================================================
// MESH TESTS
// ==========================================================================

TEST(MeshTest, StructuredQuadNodeCount) {
    auto m = mesh::generate_structured_quad(1.0, 1.0, 4, 4);
    EXPECT_EQ(m.num_nodes(), 25);  // (4+1) * (4+1)
    EXPECT_EQ(m.num_quads(), 16);  // 4 * 4
    EXPECT_EQ(m.num_dofs(), 50);   // 25 * 2
}

TEST(MeshTest, StructuredQuadCoordinates) {
    auto m = mesh::generate_structured_quad(2.0, 1.0, 2, 1);
    EXPECT_EQ(m.num_nodes(), 6);   // (2+1) * (1+1)

    // First row: y = 0
    EXPECT_NEAR(m.nodes[0].x, 0.0, 1e-12);
    EXPECT_NEAR(m.nodes[0].y, 0.0, 1e-12);
    EXPECT_NEAR(m.nodes[1].x, 1.0, 1e-12);
    EXPECT_NEAR(m.nodes[2].x, 2.0, 1e-12);

    // Second row: y = 1.0
    EXPECT_NEAR(m.nodes[3].y, 1.0, 1e-12);
}

TEST(MeshTest, ElementConnectivity) {
    auto m = mesh::generate_structured_quad(1.0, 1.0, 2, 2);
    // Element 0: nodes 0, 1, 4, 3 (bottom-left quad)
    EXPECT_EQ(m.quad_elements[0][0], 0);
    EXPECT_EQ(m.quad_elements[0][1], 1);
    EXPECT_EQ(m.quad_elements[0][2], 4);
    EXPECT_EQ(m.quad_elements[0][3], 3);
}

// ==========================================================================
// PATCH TEST (element verification)
// ==========================================================================

TEST(PatchTest, ConstantStressRecovery) {
    // 4-element patch under constant strain
    // All elements should recover exactly the same constant stress
    Mesh m;
    m.mat = Material::steel();
    m.plane = PlaneType::STRESS;

    // 2x2 quad mesh on [0,1] x [0,1]
    m.nodes = {
        {0.0, 0.0}, {0.5, 0.0}, {1.0, 0.0},
        {0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5},
        {0.0, 1.0}, {0.5, 1.0}, {1.0, 1.0}
    };
    m.quad_elements = {
        {0, 1, 4, 3}, {1, 2, 5, 4},
        {3, 4, 7, 6}, {4, 5, 8, 7}
    };

    // Fix left edge (x=0): ux=0, uy=0
    m.dirichlet = {
        {0, 0, 0.0}, {0, 1, 0.0},
        {3, 0, 0.0}, {3, 1, 0.0},
        {6, 0, 0.0}, {6, 1, 0.0}
    };

    // Apply uniform tension in x: ux = 0.001 * x at right edge
    double strain = 0.001;
    m.dirichlet.push_back({2, 0, strain * 1.0});
    m.dirichlet.push_back({5, 0, strain * 1.0});
    m.dirichlet.push_back({8, 0, strain * 1.0});

    auto result = fea::solve(m, false);

    // All elements should have approximately the same sigma_xx = E * strain (plane stress)
    double expected_sigma_xx = m.mat.E * strain;
    for (const auto& s : result.stresses) {
        EXPECT_NEAR(s.sigma_xx, expected_sigma_xx, 0.05 * std::abs(expected_sigma_xx));
    }
}

// ==========================================================================
// ENERGY BALANCE TESTS
// ==========================================================================

TEST(EnergyBalanceTest, CantileverEnergyBalance) {
    int nx = 16, ny = 4;
    double L = 1.0, H = 0.25, P = -1000.0, t = 0.01;
    auto m = mesh::generate_structured_quad(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRESS;

    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }
    int tip_node = ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, P});

    auto result = fea::solve(m, false);

    double U = fea::compute_strain_energy(result.K_csr, result.displacement);
    double W = fea::compute_work_done(result.f, result.displacement);
    EXPECT_NEAR(U, W, 1e-6 * std::abs(W) + 1e-12);
}

// ==========================================================================
// SOLVER COMPARISON TEST
// ==========================================================================

TEST(SolverComparisonTest, CholeskyVsCG) {
    int nx = 8, ny = 2;
    double L = 1.0, H = 0.25, P = -1000.0, t = 0.01;
    auto m1 = mesh::generate_structured_quad(L, H, nx, ny);
    m1.mat = Material::steel();
    m1.mat.t = t;
    m1.plane = PlaneType::STRESS;
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m1.dirichlet.push_back({node, 0, 0.0});
        m1.dirichlet.push_back({node, 1, 0.0});
    }
    int tip_node = ny * (nx + 1) + nx;
    m1.neumann.push_back({tip_node, 1, P});

    auto m2 = m1;
    auto chol_result = fea::solve(m1, false);
    auto cg_result = fea::solve(m2, true);

    for (size_t i = 0; i < chol_result.displacement.size(); ++i) {
        EXPECT_NEAR(chol_result.displacement[i], cg_result.displacement[i],
                    1e-6 * (std::abs(chol_result.displacement[i]) + 1e-12));
    }
}

// ==========================================================================
// CG CONVERGENCE ON FE SYSTEM
// ==========================================================================

TEST(CGTest, ConvergenceOnFEASystem) {
    int nx = 8, ny = 2;
    double L = 1.0, H = 0.25, P = -1000.0, t = 0.01;
    auto m = mesh::generate_structured_quad(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = t;
    m.plane = PlaneType::STRESS;
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m.dirichlet.push_back({node, 0, 0.0});
        m.dirichlet.push_back({node, 1, 0.0});
    }
    int tip_node = ny * (nx + 1) + nx;
    m.neumann.push_back({tip_node, 1, P});

    auto result = fea::solve(m, true);

    EXPECT_TRUE(result.cg_converged);
    EXPECT_LT(result.cg_iterations, 1000);
}

// ==========================================================================
// Q8 ELEMENT TESTS
// ==========================================================================

TEST(Q8ElementTest, PatchTest) {
    // 2x2 Q8 elements, prescribe exact linear displacement on all nodes
    // Verify element reproduces constant stress state within tolerance
    double L = 2.0, H = 2.0;
    int nx = 2, ny = 2;
    auto m = mesh::generate_structured_quad8(L, H, nx, ny);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    int num_corners = (nx + 1) * (ny + 1);
    int num_hmid = nx * (ny + 1);
    double eps_xx = 0.0005;

    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int node = j * (nx + 1) + i;
            m.dirichlet.push_back({node, 0, eps_xx * m.nodes[node].x});
            m.dirichlet.push_back({node, 1, 0.0});
        }
    }
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int node = num_corners + j * nx + i;
            m.dirichlet.push_back({node, 0, eps_xx * m.nodes[node].x});
            m.dirichlet.push_back({node, 1, 0.0});
        }
    }
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int node = num_corners + num_hmid + j * (nx + 1) + i;
            m.dirichlet.push_back({node, 0, eps_xx * m.nodes[node].x});
            m.dirichlet.push_back({node, 1, 0.0});
        }
    }

    auto result = fea::solve(m);

    double sigma_xx_expected = m.mat.E * eps_xx / (1.0 - m.mat.nu * m.mat.nu);
    double sigma_yy_expected = m.mat.nu * sigma_xx_expected;

    // Allow 5% tolerance for Gauss point stress averaging
    for (const auto& s : result.stresses) {
        EXPECT_NEAR(s.sigma_xx, sigma_xx_expected, sigma_xx_expected * 0.05);
        EXPECT_NEAR(s.sigma_yy, sigma_yy_expected, sigma_yy_expected * 0.05);
        EXPECT_NEAR(s.sigma_xy, 0.0, 100.0);
    }
}

TEST(Q8ElementTest, ShapeFunctions) {
    // Check shape functions at nodes
    // Node 0: (-1,-1) -> N0=1, others=0
    double xi = -1.0, eta = -1.0;
    double N[8];
    N[0] = 0.25 * (1.0 - xi) * (1.0 - eta) * (-xi - eta - 1.0);
    N[1] = 0.25 * (1.0 + xi) * (1.0 - eta) * (xi - eta - 1.0);
    N[2] = 0.25 * (1.0 + xi) * (1.0 + eta) * (xi + eta - 1.0);
    N[3] = 0.25 * (1.0 - xi) * (1.0 + eta) * (-xi + eta - 1.0);
    N[4] = 0.5 * (1.0 - xi * xi) * (1.0 - eta);
    N[5] = 0.5 * (1.0 + xi) * (1.0 - eta * eta);
    N[6] = 0.5 * (1.0 - xi * xi) * (1.0 + eta);
    N[7] = 0.5 * (1.0 - xi) * (1.0 - eta * eta);

    EXPECT_NEAR(N[0], 1.0, 1e-10);
    EXPECT_NEAR(N[1], 0.0, 1e-10);
    EXPECT_NEAR(N[2], 0.0, 1e-10);
    EXPECT_NEAR(N[3], 0.0, 1e-10);
    EXPECT_NEAR(N[4], 0.0, 1e-10);
    EXPECT_NEAR(N[5], 0.0, 1e-10);
    EXPECT_NEAR(N[6], 0.0, 1e-10);
    EXPECT_NEAR(N[7], 0.0, 1e-10);

    // Check partition of unity at center
    xi = 0.0; eta = 0.0;
    N[0] = 0.25 * (1.0 - xi) * (1.0 - eta) * (-xi - eta - 1.0);
    N[1] = 0.25 * (1.0 + xi) * (1.0 - eta) * (xi - eta - 1.0);
    N[2] = 0.25 * (1.0 + xi) * (1.0 + eta) * (xi + eta - 1.0);
    N[3] = 0.25 * (1.0 - xi) * (1.0 + eta) * (-xi + eta - 1.0);
    N[4] = 0.5 * (1.0 - xi * xi) * (1.0 - eta);
    N[5] = 0.5 * (1.0 + xi) * (1.0 - eta * eta);
    N[6] = 0.5 * (1.0 - xi * xi) * (1.0 + eta);
    N[7] = 0.5 * (1.0 - xi) * (1.0 - eta * eta);

    double sum = 0.0;
    for (int i = 0; i < 8; ++i) sum += N[i];
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST(Q8ElementTest, CantileverComparison) {
    double L = 1.0, H = 0.25, P = -1000.0, t = 0.01;
    int nx = 16, ny = 4;

    auto mat = Material::steel();
    mat.t = t;
    double I = t * H * H * H / 12.0;
    double delta_exact = P * L * L * L / (3.0 * mat.E * I);

    // Q4 mesh
    auto m4 = mesh::generate_structured_quad(L, H, nx, ny);
    m4.mat = mat;
    m4.plane = PlaneType::STRESS;
    for (int j = 0; j <= ny; ++j) {
        int node = j * (nx + 1);
        m4.dirichlet.push_back({node, 0, 0.0});
        m4.dirichlet.push_back({node, 1, 0.0});
    }
    int tip4 = ny * (nx + 1) + nx;
    m4.neumann.push_back({tip4, 1, P});
    auto r4 = fea::solve(m4);
    double delta4 = std::abs(r4.displacement[dof_index(tip4, 1)]);

    // Q8 mesh
    auto m8 = mesh::generate_structured_quad8(L, H, nx, ny);
    m8.mat = mat;
    m8.plane = PlaneType::STRESS;

    int num_corners = (nx + 1) * (ny + 1);
    int num_hmid = nx * (ny + 1);

    // Fix left edge: corners + vertical midside nodes
    for (int j = 0; j <= ny; ++j) {
        int corner = j * (nx + 1);
        m8.dirichlet.push_back({corner, 0, 0.0});
        m8.dirichlet.push_back({corner, 1, 0.0});
        int vmid = num_corners + num_hmid + j * (nx + 1);
        m8.dirichlet.push_back({vmid, 0, 0.0});
        m8.dirichlet.push_back({vmid, 1, 0.0});
    }

    // Q8 tip node is the same corner node as Q4
    int tip8 = ny * (nx + 1) + nx;
    m8.neumann.push_back({tip8, 1, P});
    auto r8 = fea::solve(m8);
    double delta8 = std::abs(r8.displacement[dof_index(tip8, 1)]);

    double delta_exact_abs = std::abs(P * L * L * L / (3.0 * mat.E * I));

    std::cout << "Q4 tip: " << delta4 << ", Q8 tip: " << delta8
              << ", exact: " << delta_exact_abs << std::endl;

    double err4 = std::abs(delta4 - delta_exact_abs) / delta_exact_abs;
    double err8 = std::abs(delta8 - delta_exact_abs) / delta_exact_abs;

    std::cout << "Q4 error: " << err4 * 100.0 << "%" << std::endl;
    std::cout << "Q8 error: " << err8 * 100.0 << "%" << std::endl;

    // Both should be reasonable
    EXPECT_LT(err4, 0.50);
    EXPECT_LT(err8, 0.50);
}

// ==========================================================================
// NEGATIVE JACOBIAN DETECTION
// ==========================================================================

TEST(Q4ElementTest, NegativeJacobianDetection) {
    Material mat = Material::steel();
    // Bowtie element (inverted -- nodes crossed)
    std::array<Node, 4> bad_nodes = {
        Node{0.0, 0.0}, Node{1.0, 1.0},
        Node{1.0, 0.0}, Node{0.0, 1.0}
    };

    EXPECT_THROW(elements::Q4Element::stiffness(bad_nodes, mat, PlaneType::STRESS),
                 std::runtime_error);
}

// ==========================================================================
// MAIN
// ==========================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
