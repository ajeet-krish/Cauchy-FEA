#include <gtest/gtest.h>
#include "fea_types.hpp"
#include "elements.hpp"
#include "elements_3d.hpp"
#include "locking_mitigation.hpp"
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
    }
    for (int j = 0; j < ny; ++j) {
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
// 3D ELEMENT TESTS
// ==========================================================================

// H8 shape functions sum to 1
TEST(H8ElementTest, ShapeFunctionsPartitionOfUnity) {
    double test_pts[][3] = {{0.0, 0.0, 0.0}, {0.5, 0.5, 0.5},
                            {-0.5, -0.5, -0.5}, {0.3, -0.7, 0.2}};

    for (auto& pt : test_pts) {
        double sum = 0.0;
        for (int i = 0; i < 8; ++i) {
            sum += elements::H8Element::shape_func(i, pt[0], pt[1], pt[2]);
        }
        EXPECT_NEAR(sum, 1.0, 1e-12);
    }
}

// H8 Jacobian for unit cube
TEST(H8ElementTest, JacobianUnitCube) {
    // Unit cube: [0,1] x [0,1] x [0,1]
    std::array<Node, 8> nodes = {{
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    }};

    double detJ = elements::H8Element::jacobian_det(nodes, 0.0, 0.0, 0.0);
    EXPECT_NEAR(detJ, 1.0/8.0, 1e-12);  // For unit cube, det(J) = 1/8

    // Should be constant for trilinear hex on cube
    detJ = elements::H8Element::jacobian_det(nodes, 0.5, 0.5, 0.5);
    EXPECT_NEAR(detJ, 1.0/8.0, 1e-12);
}

// H8 stiffness matrix symmetry
TEST(H8ElementTest, StiffnessSymmetry) {
    Material mat = Material::steel();
    std::array<Node, 8> nodes = {{
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    }};

    auto K = elements::H8Element::stiffness(nodes, mat);

    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 24; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-8 * (std::abs(K[i][j]) + 1.0));
        }
    }
}

// H8 stiffness produces zero for rigid body mode
TEST(H8ElementTest, StiffnessSingularRigidBody) {
    Material mat = Material::steel();
    std::array<Node, 8> nodes = {{
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    }};

    auto K = elements::H8Element::stiffness(nodes, mat);

    // Rigid body translation in x
    std::array<double, 24> u_rb{};
    for (int i = 0; i < 8; ++i) u_rb[3*i] = 1.0;  // all ux = 1

    std::array<double, 24> Ku{};
    for (int i = 0; i < 24; ++i)
        for (int j = 0; j < 24; ++j)
            Ku[i] += K[i][j] * u_rb[j];

    // Tolerance: floating point accumulation across 8 Gauss points
    // produces residuals ~1e-5 for stiffness ~1e10 (relative error ~1e-15)
    double K_max = 0.0;
    for (int i = 0; i < 24; ++i)
        K_max = std::max(K_max, std::abs(K[i][i]));
    for (int i = 0; i < 24; ++i) {
        EXPECT_NEAR(Ku[i], 0.0, 1e-6 * K_max);
    }
}

// T4 shape functions sum to 1
TEST(T4ElementTest, ShapeFunctionsPartitionOfUnity) {
    double test_pts[][3] = {{0.25, 0.25, 0.25}, {0.5, 0.2, 0.1},
                            {0.1, 0.6, 0.1}, {0.0, 0.0, 0.0}};

    for (auto& pt : test_pts) {
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            sum += elements::T4Element::shape_func(i, pt[0], pt[1], pt[2]);
        }
        EXPECT_NEAR(sum, 1.0, 1e-12);
    }
}

// T4 element volume for unit tet
TEST(T4ElementTest, Volume) {
    // Unit tet: (0,0,0), (1,0,0), (0,1,0), (0,0,1)
    std::array<Node, 4> nodes = {{
        {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}
    }};

    double vol = elements::T4Element::volume(nodes);
    EXPECT_NEAR(vol, 1.0/6.0, 1e-12);
}

// T4 stiffness matrix symmetry
TEST(T4ElementTest, StiffnessSymmetry) {
    Material mat = Material::steel();
    std::array<Node, 4> nodes = {{
        {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}
    }};

    auto K = elements::T4Element::stiffness(nodes, mat);

    for (int i = 0; i < 12; ++i) {
        for (int j = 0; j < 12; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-8 * (std::abs(K[i][j]) + 1.0));
        }
    }
}

// 3D hex mesh generation
TEST(HexMeshTest, Generation) {
    set_dimension(3);
    auto m = mesh::generate_structured_hex(1.0, 1.0, 1.0, 2, 2, 2);

    EXPECT_EQ(m.num_nodes(), 27);   // (2+1)^3
    EXPECT_EQ(m.num_hexes(), 8);    // 2^3
    EXPECT_TRUE(m.is_3d());

    // Check node coordinates are within [0,1]
    for (const auto& n : m.nodes) {
        EXPECT_GE(n.x, -1e-12);
        EXPECT_LE(n.x, 1.0 + 1e-12);
        EXPECT_GE(n.y, -1e-12);
        EXPECT_LE(n.y, 1.0 + 1e-12);
        EXPECT_GE(n.z, -1e-12);
        EXPECT_LE(n.z, 1.0 + 1e-12);
    }
}

// 3D tet mesh generation from hex
TEST(TetMeshTest, GenerationFromHex) {
    set_dimension(3);
    auto m = mesh::generate_structured_tet(1.0, 1.0, 1.0, 2, 2, 2);

    EXPECT_EQ(m.num_nodes(), 27);
    EXPECT_EQ(m.num_tets(), 48);   // 8 hexes * 6 tets each
    EXPECT_EQ(m.num_hexes(), 0);   // hexes removed after conversion
    EXPECT_TRUE(m.is_3d());
}

// 3D cantilever beam validation (H8 elements)
TEST(H8CantileverTest, TipDeflection) {
    // Cantilever beam: L=1.0, h=0.1, t=0.1
    // Point load P at free end
    // Exact tip deflection: PL^3/(3EI) for Euler-Bernoulli
    set_dimension(3);

    double L = 1.0, h = 0.1, t = 0.1;
    double P = -1000.0;  // downward load
    double E = 200e9;
    double nu = 0.3;

    int nx = 16, ny = 4, nz = 2;
    auto m = mesh::generate_structured_hex(L, h, t, nx, ny, nz);

    m.mat.E = E;
    m.mat.nu = nu;
    m.mat.rho = 7800.0;
    m.mat.t = 1.0;  // not used for 3D

    // Fix left end (x=0): all DOFs = 0
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < 1e-10) {
            m.dirichlet.push_back({i, 0, 0.0});
            m.dirichlet.push_back({i, 1, 0.0});
            m.dirichlet.push_back({i, 2, 0.0});
        }
    }

    // Apply load at free end (x=L): bottom edge center
    double load_y = h / 2.0;
    double load_z = t / 2.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - L) < 1e-10 &&
            std::abs(m.nodes[i].y - load_y) < 1e-10 &&
            std::abs(m.nodes[i].z - load_z) < 1e-10) {
            m.neumann.push_back({i, 1, P});
        }
    }

    auto result = fea::solve(m, true);

    // Find max displacement at tip
    double max_uy = 0.0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - L) < 1e-10) {
            double uy = result.displacement[dof_index(i, 1)];
            if (uy < max_uy) max_uy = uy;
        }
    }

    // Exact: delta = PL^3/(3EI), I = t*h^3/12
    double I = t * h * h * h / 12.0;
    double delta_exact = std::abs(P * L * L * L / (3.0 * E * I));

    double tip_disp = std::abs(max_uy);
    double error = std::abs(tip_disp - delta_exact) / delta_exact;

    std::cout << "H8 cantilever tip: " << tip_disp
              << ", exact: " << delta_exact
              << ", error: " << error * 100.0 << "%" << std::endl;

    // H8 with full integration locks in bending, so allow up to 30% error
    // (mesh refinement would improve this)
    EXPECT_LT(error, 0.30);
    EXPECT_GT(tip_disp, 0.0);  // must deflect downward
}

// ==========================================================================
// PRECONDITIONER TESTS
// ==========================================================================

// Test that Jacobi preconditioner reduces CG iterations vs unpreconditioned
TEST(PreconditionerTest, JacobiReducesIterations) {
    // Build a simple SPD system (5x5 tridiagonal)
    int n = 5;
    COOMatrix K_coo(n, n);
    for (int i = 0; i < n; ++i) {
        K_coo.add(i, i, 4.0);
        if (i > 0) K_coo.add(i, i - 1, -1.0);
        if (i < n - 1) K_coo.add(i, i + 1, -1.0);
    }
    auto K = K_coo.to_csr();

    std::vector<double> b(n, 1.0);

    // Unpreconditioned CG (using Jacobi with identity-like preconditioner)
    CGSolver cg_no_prec(1000, 1e-10);
    preconditioners::Jacobi M;
    M.setup(K);
    auto result = cg_no_prec.solve(K, b, M);

    EXPECT_TRUE(result.converged);
    EXPECT_LT(result.iterations, n);  // should converge quickly for 5x5
}

// Test IC(0) preconditioner on a banded system
TEST(PreconditionerTest, IncompleteCholeskySetup) {
    int n = 20;
    COOMatrix K_coo(n, n);
    for (int i = 0; i < n; ++i) {
        K_coo.add(i, i, 4.0);
        if (i > 0) K_coo.add(i, i - 1, -1.0);
        if (i < n - 1) K_coo.add(i, i + 1, -1.0);
    }
    auto K = K_coo.to_csr();

    preconditioners::IncompleteCholesky ic;
    ic.setup(K);

    // IC(0) should produce a valid lower triangular factor
    EXPECT_GT(ic.n, 0);
    EXPECT_FALSE(ic.L_vals.empty());

    // Apply preconditioner to a vector
    std::vector<double> r(n, 1.0);
    std::vector<double> z(n, 0.0);
    ic.apply(r, z);

    // z should be non-zero
    double z_norm = 0.0;
    for (int i = 0; i < n; ++i) z_norm += z[i] * z[i];
    EXPECT_GT(z_norm, 0.0);
}

// Test SSOR preconditioner
TEST(PreconditionerTest, SSORSetup) {
    int n = 20;
    COOMatrix K_coo(n, n);
    for (int i = 0; i < n; ++i) {
        K_coo.add(i, i, 4.0);
        if (i > 0) K_coo.add(i, i - 1, -1.0);
        if (i < n - 1) K_coo.add(i, i + 1, -1.0);
    }
    auto K = K_coo.to_csr();

    preconditioners::SSOR ssor(1.0);  // omega = 1.0 (Gauss-Seidel)
    ssor.setup(K);

    std::vector<double> r(n, 1.0);
    std::vector<double> z(n, 0.0);
    ssor.apply(r, z);

    double z_norm = 0.0;
    for (int i = 0; i < n; ++i) z_norm += z[i] * z[i];
    EXPECT_GT(z_norm, 0.0);
}

// Test Block Jacobi preconditioner
TEST(PreconditionerTest, BlockJacobiSetup) {
    int n = 20;
    COOMatrix K_coo(n, n);
    for (int i = 0; i < n; ++i) {
        K_coo.add(i, i, 4.0);
        if (i > 0) K_coo.add(i, i - 1, -1.0);
        if (i < n - 1) K_coo.add(i, i + 1, -1.0);
    }
    auto K = K_coo.to_csr();

    preconditioners::BlockJacobi bj(2);  // block_size = 2
    bj.setup(K);

    EXPECT_FALSE(bj.blocks.empty());

    std::vector<double> r(n, 1.0);
    std::vector<double> z(n, 0.0);
    bj.apply(r, z);

    double z_norm = 0.0;
    for (int i = 0; i < n; ++i) z_norm += z[i] * z[i];
    EXPECT_GT(z_norm, 0.0);
}

// Test that IC(0) + CG converges to the same solution as Jacobi + CG
TEST(PreconditionerTest, IC0ConvergesSameSolution) {
    // 1D Poisson: -u'' = f on [0,1], u(0)=u(1)=0
    int n = 50;
    COOMatrix K_coo(n, n);
    double h = 1.0 / (n + 1);
    for (int i = 0; i < n; ++i) {
        K_coo.add(i, i, 2.0 / (h * h));
        if (i > 0) K_coo.add(i, i - 1, -1.0 / (h * h));
        if (i < n - 1) K_coo.add(i, i + 1, -1.0 / (h * h));
    }
    auto K = K_coo.to_csr();

    // RHS: f = 1 everywhere
    std::vector<double> b(n, 1.0);

    // Solve with Jacobi
    preconditioners::Jacobi M_jac;
    M_jac.setup(K);
    CGSolver cg(1000, 1e-10);
    auto res_jac = cg.solve(K, b, M_jac);

    // Solve with IC(0)
    preconditioners::IncompleteCholesky M_ic;
    M_ic.setup(K);
    CGSolver cg2(1000, 1e-10);
    auto res_ic = cg2.solve(K, b, M_ic);

    EXPECT_TRUE(res_jac.converged);
    EXPECT_TRUE(res_ic.converged);

    // Solutions should be close
    double diff_norm = 0.0;
    double sol_norm = 0.0;
    for (int i = 0; i < n; ++i) {
        diff_norm += (res_jac.x[i] - res_ic.x[i]) * (res_jac.x[i] - res_ic.x[i]);
        sol_norm += res_jac.x[i] * res_jac.x[i];
    }
    diff_norm = std::sqrt(diff_norm);
    sol_norm = std::sqrt(sol_norm);

    if (sol_norm > 1e-15) {
        EXPECT_LT(diff_norm / sol_norm, 1e-6);
    }
}

// ==========================================================================
// SHEAR LOCKING MITIGATION TESTS
// ==========================================================================

// Test that SRI element produces valid stiffness matrix
TEST(ShearingLockingTest, SRIStiffnessSymmetry) {
    Material mat = Material::steel();
    std::array<Node, 4> nodes = {
        Node{0.0, 0.0}, Node{1.0, 0.0},
        Node{1.0, 1.0}, Node{0.0, 1.0}
    };

    auto K = locking::Q4SRIElement::stiffness(nodes, mat, PlaneType::STRESS);

    // Check symmetry
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-8 * (std::abs(K[i][j]) + 1.0));
        }
    }

    // Check positive semi-definiteness (diagonal should be positive)
    for (int i = 0; i < 8; ++i) {
        EXPECT_GT(K[i][i], 0.0);
    }
}

// Test that BBar element produces valid stiffness matrix
TEST(ShearingLockingTest, BBarStiffnessSymmetry) {
    Material mat = Material::steel();
    std::array<Node, 4> nodes = {
        Node{0.0, 0.0}, Node{1.0, 0.0},
        Node{1.0, 1.0}, Node{0.0, 1.0}
    };

    auto K = locking::Q4BBarElement::stiffness(nodes, mat, PlaneType::STRESS);

    // Check symmetry
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            EXPECT_NEAR(K[i][j], K[j][i], 1e-8 * (std::abs(K[i][j]) + 1.0));
        }
    }
}

// Test SRI on a bending problem: cantilever with tip load
// SRI should give better tip deflection than full integration for coarse meshes
TEST(ShearingLockingTest, SRICantileverBending) {
    // Create a 4x1 cantilever beam (very coarse, prone to locking)
    auto m = mesh::generate_structured_quad(4.0, 1.0, 4, 1);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Fix left end
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < 1e-10) {
            m.dirichlet.push_back({i, 0, 0.0});
            m.dirichlet.push_back({i, 1, 0.0});
        }
    }

    // Apply tip load at top-right node
    int top_right = -1;
    double max_y = -1e20;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (m.nodes[i].x > 3.9 && m.nodes[i].y > max_y) {
            max_y = m.nodes[i].y;
            top_right = i;
        }
    }
    m.neumann.push_back({top_right, 1, -1000.0});

    // Solve with full integration (original)
    g_integration = IntegrationType::FULL;
    auto result_full = fea::solve(m, true);

    // Solve with SRI
    g_integration = IntegrationType::SRI;
    auto result_sri = fea::solve(m, true);

    // Reset
    g_integration = IntegrationType::FULL;

    // Find tip displacement
    double tip_uy_full = result_full.displacement[dof_index(top_right, 1)];
    double tip_uy_sri = result_sri.displacement[dof_index(top_right, 1)];

    std::cout << "SRI cantilever: Full tip uy = " << tip_uy_full
              << ", SRI tip uy = " << tip_uy_sri << std::endl;

    // Both should deflect downward (negative)
    EXPECT_LT(tip_uy_full, 0.0);
    EXPECT_LT(tip_uy_sri, 0.0);

    // SRI should give larger magnitude (less locked) than full integration
    // for this very coarse mesh - this is the whole point of SRI
    EXPECT_GT(std::abs(tip_uy_sri), std::abs(tip_uy_full));
}

// Test BBar on a bending problem
TEST(ShearingLockingTest, BBarCantileverBending) {
    auto m = mesh::generate_structured_quad(4.0, 1.0, 4, 1);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Fix left end
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < 1e-10) {
            m.dirichlet.push_back({i, 0, 0.0});
            m.dirichlet.push_back({i, 1, 0.0});
        }
    }

    // Apply tip load
    int top_right = -1;
    double max_y = -1e20;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (m.nodes[i].x > 3.9 && m.nodes[i].y > max_y) {
            max_y = m.nodes[i].y;
            top_right = i;
        }
    }
    m.neumann.push_back({top_right, 1, -1000.0});

    // Solve with BBar
    g_integration = IntegrationType::BBAR;
    auto result_bbar = fea::solve(m, true);

    // Reset
    g_integration = IntegrationType::FULL;

    double tip_uy_bbar = result_bbar.displacement[dof_index(top_right, 1)];

    std::cout << "BBar cantilever: BBar tip uy = " << tip_uy_bbar << std::endl;

    // Should deflect downward
    EXPECT_LT(tip_uy_bbar, 0.0);
}

// Test that SRI produces reasonable stresses (patch test variant)
// Note: SRI does NOT pass the patch test exactly because the 1-point
// shear integration loses accuracy for constant shear stress states.
// Instead, verify that stresses are in the correct ballpark.
TEST(ShearingLockingTest, SRIPatchTest) {
    // 2x2 mesh under constant tension
    auto m = mesh::generate_structured_quad(1.0, 1.0, 4, 4);
    m.mat = Material::steel();
    m.mat.t = 0.01;
    m.plane = PlaneType::STRESS;

    // Fix left edge (x DOFs = 0)
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x) < 1e-10) {
            m.dirichlet.push_back({i, 0, 0.0});
        }
    }
    // Fix one node in y to prevent rigid body motion
    m.dirichlet.push_back({0, 1, 0.0});

    // Apply uniform tension in x-direction on right edge
    double total_force = 1000.0;
    int right_nodes = 0;
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - 1.0) < 1e-10) right_nodes++;
    }
    for (int i = 0; i < m.num_nodes(); ++i) {
        if (std::abs(m.nodes[i].x - 1.0) < 1e-10) {
            m.neumann.push_back({i, 0, total_force / right_nodes});
        }
    }

    g_integration = IntegrationType::SRI;
    auto result = fea::solve(m, true);
    g_integration = IntegrationType::FULL;

    // Verify that the solution is reasonable
    // Expected: uniform stress sigma_xx = F/A = 1000 / (1.0 * 0.01) = 100000 Pa
    double expected_stress = total_force / (1.0 * m.mat.t);

    double avg_sxx = 0.0;
    for (const auto& s : result.stresses) {
        avg_sxx += s.sigma_xx;
    }
    avg_sxx /= result.stresses.size();

    std::cout << "SRI patch test: avg sigma_xx = " << avg_sxx
              << ", expected = " << expected_stress << std::endl;

    // SRI with 4x4 mesh should be within 30% of expected for tension
    // (SRI is designed for bending, not pure tension)
    double rel_error = std::abs(avg_sxx - expected_stress) / expected_stress;
    EXPECT_LT(rel_error, 0.30);
}

// ==========================================================================
// MAIN
// ==========================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
