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
// MAIN
// ==========================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
