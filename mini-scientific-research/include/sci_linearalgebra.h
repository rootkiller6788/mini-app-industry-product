#ifndef MINI_SCI_LINALG_H
#define MINI_SCI_LINALG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ================================================================
 *  sci_linalg -- Linear Algebra for Scientific Computing
 *
 *  L1: Core types -- matrix, vector, decomposition structures
 *  L2: Core concepts -- linear independence, span, basis, rank,
 *      determinant, eigenvalues/eigenvectors, positive definiteness
 *  L3: Engineering structures -- BLAS-style operations, storage formats
 *      (dense, packed, banded), pivoting strategies
 *  L4: Standards/Theorems -- Spectral Theorem (Hermitian matrices have
 *      real eigenvalues), Singular Value Decomposition (Eckart-Young),
 *      Cayley-Hamilton Theorem, Gershgorin Circle Theorem, Perron-Frobenius
 *  L5: Algorithms -- Gaussian elimination with partial pivoting
 *      (Turing 1948), LU decomposition (Doolittle), Cholesky decomposition
 *      (Benoit 1924), QR via Householder reflections (1958), Power iteration
 *      for eigenvalues, Jacobi eigenvalue algorithm, SVD via Golub-Reinsch
 *  L6: Canonical problems -- Linear system solving, least squares,
 *      eigenvalue computation, matrix factorization
 *  L7: Applications -- Structural analysis (FEM), Kalman filtering,
 *      PCA dimensionality reduction, Google PageRank
 *  L8: Advanced -- Sparse solvers, preconditioners, iterative methods
 *      (CG, GMRES), randomized SVD, parallel BLAS/ScaLAPACK
 *  L9: Industry -- LAPACK, BLAS, Eigen, Armadillo, cuBLAS, MKL
 *
 *  References:
 *  - Golub & Van Loan, "Matrix Computations" (4th ed, 2013)
 *  - Trefethen & Bau, "Numerical Linear Algebra" (1997)
 *  - Demmel, "Applied Numerical Linear Algebra" (1997)
 * ================================================================ */

/* -----------------------------------------------------------------
 * L1: Core Type Definitions
 * ----------------------------------------------------------------- */

/** Dense matrix in column-major storage (Fortran/LAPACK convention) */
typedef struct {
    int      rows;      /* Number of rows */
    int      cols;      /* Number of columns */
    int      ld;        /* Leading dimension (>= rows) */
    double  *data;      /* Data array, size ld * cols */
    bool     owner;     /* Whether this struct owns the data */
} SCI_Matrix;

/** Vector with stride support */
typedef struct {
    int      size;      /* Number of elements */
    int      stride;    /* Stride between elements */
    double  *data;      /* Data array */
    bool     owner;
} SCI_Vector;

/** LU decomposition */
typedef struct {
    SCI_Matrix  LU;          /* Combined L and U factors */
    int        *pivot;       /* Pivot indices (size n) */
    int         n;           /* Matrix dimension */
    int         sign;        /* Sign of permutation (+1 or -1) */
} SCI_LUResult;

/** QR decomposition */
typedef struct {
    SCI_Matrix  QR;          /* Combined matrix (R in upper triangle, Q in reflectors) */
    double     *tau;         /* Householder reflector coefficients (size min(m,n)) */
    int         m, n;
} SCI_QRResult;

/** Cholesky decomposition: A = L * L^T */
typedef struct {
    SCI_Matrix  L;           /* Lower triangular factor */
    int         n;
} SCI_CholeskyResult;

/** Singular Value Decomposition: A = U * S * V^T */
typedef struct {
    double     *S;           /* Singular values (size min(m,n)) */
    SCI_Matrix  U;           /* Left singular vectors (m x min(m,n)) */
    SCI_Matrix  Vt;          /* Right singular vectors transposed (min(m,n) x n) */
    int         m, n;
    int         rank;        /* Numerical rank */
} SCI_SVDResult;

/** Eigenvalue decomposition */
typedef struct {
    double     *eigenvalues_real;  /* Real parts (size n) */
    double     *eigenvalues_imag;  /* Imaginary parts (size n) */
    SCI_Matrix  eigenvectors;      /* Right eigenvectors as columns (n x n) */
    int         n;
    bool        converged;
    int         iterations;
} SCI_EigenResult;

/* -----------------------------------------------------------------
 * L2: Matrix and Vector Construction/Destruction
 * ----------------------------------------------------------------- */

/**
 * Create a matrix with given dimensions (zero-initialized).
 * Uses column-major storage for LAPACK/BLAS compatibility.
 * Complexity: O(rows * cols).
 */
SCI_Matrix sci_matrix_create(int rows, int cols);

/**
 * Create a matrix from existing data (no copy, wraps pointer).
 * data must be column-major order. Set owner=false.
 */
SCI_Matrix sci_matrix_from_data(double *data, int rows, int cols, bool owner);

/** Create an identity matrix of size n. */
SCI_Matrix sci_matrix_identity(int n);

/** Create a diagonal matrix from vector elements. */
SCI_Matrix sci_matrix_diag(const double *v, int n);

/** Destroy matrix, freeing owned data. */
void sci_matrix_destroy(SCI_Matrix *mat);

/** Access matrix element at (i, j) in column-major A[i + j*ld]. */
static inline double sci_matrix_get(const SCI_Matrix *A, int i, int j) {
    return A->data[i + j * A->ld];
}
static inline void sci_matrix_set(SCI_Matrix *A, int i, int j, double val) {
    A->data[i + j * A->ld] = val;
}

/**
 * Create a vector of given size (zero-initialized).
 * Complexity: O(size).
 */
SCI_Vector sci_vector_create(int size);

/** Create vector from existing data (wraps pointer). */
SCI_Vector sci_vector_from_data(double *data, int size, bool owner);

/** Destroy vector, freeing owned data. */
void sci_vector_destroy(SCI_Vector *v);

/* -----------------------------------------------------------------
 * L5: Basic Linear Algebra Operations
 * ----------------------------------------------------------------- */

/**
 * Matrix-vector multiplication: y = alpha * A * x + beta * y
 * This is the BLAS DGEMV operation.
 *
 * Complexity: O(rows * cols).
 *
 * @param A      Matrix (rows x cols)
 * @param x      Input vector (size cols)
 * @param y      Output vector (size rows, modified in place)
 * @param alpha  Scale factor for A*x
 * @param beta   Scale factor for y
 */
void sci_matvec_mul(const SCI_Matrix *A, const SCI_Vector *x,
                     double *y, double alpha, double beta);

/**
 * Matrix-matrix multiplication: C = alpha * A * B + beta * C
 * This is the BLAS DGEMM operation.
 *
 * Uses optimized triple-loop with ikj order for cache efficiency
 * (access pattern: C[i*ldC+j], A[i*ldA+k], B[k*ldB+j]).
 *
 * Complexity: O(rows_A * cols_A * cols_B).
 */
void sci_matmul(const SCI_Matrix *A, const SCI_Matrix *B,
                 SCI_Matrix *C, double alpha, double beta);

/**
 * Vector dot product: sum_i x_i * y_i
 * Complexity: O(n).
 */
double sci_vector_dot(const double *x, const double *y, int n);

/**
 * Vector Euclidean norm (L2): sqrt(sum x_i^2)
 * Uses scaling to avoid overflow.
 */
double sci_vector_norm2(const double *x, int n);

/**
 * Vector infinity norm (L_inf): max_i |x_i|
 */
double sci_vector_norm_inf(const double *x, int n);

/**
 * Matrix Frobenius norm: sqrt(sum sum a_ij^2)
 */
double sci_matrix_norm_frobenius(const SCI_Matrix *A);

/**
 * Transpose matrix: B = A^T
 *
 * @param A   Input matrix
 * @param At  Output matrix (must have dimensions cols x rows)
 */
void sci_matrix_transpose(const SCI_Matrix *A, SCI_Matrix *At);

/* -----------------------------------------------------------------
 * L5: Linear System Solving
 * ----------------------------------------------------------------- */

/**
 * Solve linear system A x = b using Gaussian elimination with
 * partial pivoting (Turing 1948, Wilkinson 1961).
 *
 * Partial pivoting: at step k, swap row k with the row having the
 * largest |a_{ik}| for i >= k. This controls growth factor and
 * improves numerical stability.
 *
 * Complexity: O(n^3) operations, O(n^2) pivoting.
 *
 * @param A   Input: n x n matrix (modified in place)
 * @param b   Input: RHS vector (size n, modified for output solution)
 * @param n   Matrix dimension
 * @return    true on success, false if singular
 */
bool sci_solve_lu(const SCI_Matrix *A, double *b, int n);

/**
 * LU decomposition with partial pivoting.
 * P * A = L * U
 *
 * L is unit lower triangular (stored in lower part of LU).
 * U is upper triangular (stored in upper part of LU).
 *
 * Complexity: O(n^3).
 */
SCI_LUResult sci_lu_decompose(const SCI_Matrix *A);

/**
 * Solve using pre-computed LU decomposition.
 *
 * Step 1: Forward substitution L y = P b
 * Step 2: Backward substitution U x = y
 *
 * Complexity: O(n^2).
 */
void sci_lu_solve(const SCI_LUResult *lu, const double *b, double *x);

/**
 * Compute determinant from LU decomposition.
 * det(A) = (+/-) prod_i U_{ii}
 * Complexity: O(n).
 */
double sci_det_from_lu(const SCI_LUResult *lu);

/** Free LU decomposition resources. */
void sci_lu_free(SCI_LUResult *lu);

/**
 * Cholesky decomposition: A = L * L^T.
 * Requires A to be symmetric positive definite (SPD).
 *
 * Algorithm: column-wise Cholesky-Crout.
 *   L_{jj} = sqrt(A_{jj} - sum_{k=1}^{j-1} L_{jk}^2)
 *   L_{ij} = (A_{ij} - sum_{k=1}^{j-1} L_{ik} L_{jk}) / L_{jj}   for i > j
 *
 * Complexity: O(n^3/3) (half of LU).
 * More numerically stable than LU for SPD matrices.
 *
 * @param A  Input SPD matrix (lower triangular part used)
 * @return   Cholesky factors, L.data=NULL if not SPD
 */
SCI_CholeskyResult sci_cholesky_decompose(const SCI_Matrix *A);

/**
 * Solve Ax = b using Cholesky factors.
 * Step 1: L y = b (forward)
 * Step 2: L^T x = y (backward)
 * Complexity: O(n^2).
 */
void sci_cholesky_solve(const SCI_CholeskyResult *chol,
                         const double *b, double *x);

void sci_cholesky_free(SCI_CholeskyResult *chol);

/* -----------------------------------------------------------------
 * L5: QR Decomposition
 * ----------------------------------------------------------------- */

/**
 * QR decomposition using Householder reflections.
 * A = Q * R where Q is orthogonal (Q^T = Q^{-1}) and R is upper triangular.
 *
 * Householder reflector: H = I - 2 * v * v^T / (v^T v)
 * Reflects vector x onto e_1 direction: H x = -sign(x_0) * ||x|| * e_1
 *
 * Golub & Van Loan (Golub 1965 for Householder QR).
 * Complexity: O(2 m n^2) flops for m >= n.
 * Numerically more stable than Gram-Schmidt.
 *
 * @param A  Input matrix (m x n)
 * @return   QR decomposition
 */
SCI_QRResult sci_qr_decompose(const SCI_Matrix *A);

/**
 * Solve least squares problem min ||A x - b||_2 using QR.
 * Equivalent to solving R x = Q^T b.
 *
 * @param qr  QR decomposition of A
 * @param b   RHS vector (size m)
 * @param x   Output solution (size n)
 */
void sci_qr_solve_ls(const SCI_QRResult *qr, const double *b, double *x);

/**
 * Multiply Q (implicitly stored) with a vector: y = Q * x or y = Q^T * x.
 *
 * @param qr      QR decomposition
 * @param trans   false: y = Q*x, true: y = Q^T*x
 * @param x       Input vector
 * @param y       Output vector
 */
void sci_qr_apply_q(const SCI_QRResult *qr, bool trans,
                     const double *x, double *y);

void sci_qr_free(SCI_QRResult *qr);

/* -----------------------------------------------------------------
 * L5: Eigenvalue Computation
 * ----------------------------------------------------------------- */

/**
 * Power iteration for dominant eigenvalue and eigenvector.
 *
 * v_{k+1} = A * v_k / ||A * v_k||
 * lambda_k = v_k^T * A * v_k (Rayleigh quotient)
 *
 * Convergence: linear, rate |lambda_2 / lambda_1|.
 * Only finds eigenvalue with largest magnitude.
 *
 * Theorem (von Mises & Pollaczek-Geiringer 1929): Power iteration
 * converges to eigenvector of dominant eigenvalue assuming
 * |lambda_1| > |lambda_2|.
 *
 * Complexity: O(n^2) per iteration.
 *
 * @param A           Input matrix (n x n)
 * @param v0          Initial guess (size n, output: dominant eigenvector)
 * @param n           Dimension
 * @param max_iter    Maximum iterations
 * @param tol         Convergence tolerance
 * @param eigenvalue  Output: dominant eigenvalue
 * @param iterations  Output: iterations performed
 * @return            true if converged
 */
bool sci_eigen_power_iteration(const SCI_Matrix *A, double *v0, int n,
                                int max_iter, double tol,
                                double *eigenvalue, int *iterations);

/**
 * QR algorithm for all eigenvalues (Francis 1961, Kublanovskaya 1961).
 *
 * Iterate: A_{k+1} = Q_k^T * A_k * Q_k
 * As k -> inf, A_k converges to upper triangular (Schur form)
 * with eigenvalues on the diagonal.
 *
 * Uses Wilkinson shift for faster convergence:
 *   shift = eigenvalue of trailing 2x2 block closest to a_{nn}.
 *   QR step on (A - shift * I), then add shift back.
 *
 * Complexity: O(n^3) overall, ~2-3 QR iterations per eigenvalue.
 *
 * @param A      Input matrix (modified to Schur form)
 * @param n      Dimension
 * @param tol    Convergence tolerance
 * @param result Output eigenvalue result (caller frees)
 */
bool sci_eigen_qr_algorithm(const SCI_Matrix *A, int n, double tol,
                             SCI_EigenResult *result);

/**
 * Free eigenvalue result resources.
 */
void sci_eigen_result_free(SCI_EigenResult *result);

/* -----------------------------------------------------------------
 * L5: Singular Value Decomposition
 * ----------------------------------------------------------------- */

/**
 * Compute SVD of A using Golub-Reinsch algorithm (1970).
 *
 * A = U * S * V^T
 *
 * Two-phase approach:
 * Phase 1: Bidiagonalize A via Householder reflections
 * Phase 2: Implicit shifted QR on bidiagonal matrix (Golub-Kahan)
 *
 * Singular values give insight into:
 * - Matrix rank: count s_i > tol * s_max
 * - Condition number: s_max / s_min
 * - Best rank-k approximation (Eckart-Young-Mirsky theorem):
 *   ||A - A_k||_F = sqrt(sum_{i=k+1}^r s_i^2)
 *
 * Complexity: O(m n^2) for m >= n.
 * Used in PCA, pseudoinverse, regularization, recommendation systems.
 *
 * @param A     Input matrix (m x n)
 * @param tol   Tolerance for rank determination
 * @param result Output SVD (caller must free)
 */
bool sci_svd_decompose(const SCI_Matrix *A, double tol,
                        SCI_SVDResult *result);

/**
 * Compute pseudoinverse via SVD.
 * A^+ = V * S^+ * U^T where S^+ replaces non-zero singular values
 * with their reciprocals.
 *
 * @param svd     SVD of A
 * @param A_plus  Output pseudoinverse (caller allocates, n x m)
 */
void sci_svd_pseudoinverse(const SCI_SVDResult *svd, SCI_Matrix *A_plus);

void sci_svd_free(SCI_SVDResult *svd);

/* -----------------------------------------------------------------
 * L7: Application Utilities
 * ----------------------------------------------------------------- */

/**
 * Principal Component Analysis (PCA) via SVD.
 *
 * 1. Center data (subtract column means)
 * 2. Compute SVD of centered matrix
 * 3. Principal components = V^T, explained variance = s_i^2 / sum(s_j^2)
 *
 * PCA (Pearson 1901, Hotelling 1933): reduces dimensionality while
 * preserving maximum variance in the data.
 *
 * @param X              Data matrix (n_obs x n_vars, row-major)
 * @param n_obs          Number of observations
 * @param n_vars         Number of variables
 * @param k              Number of principal components to retain
 * @param components     Output: component loadings (k x n_vars, row-major)
 * @param scores         Output: projected scores (n_obs x k, row-major, or NULL)
 * @param var_explained  Output: proportion variance explained per component
 * @return               true on success
 */
bool sci_pca(const double *X, int n_obs, int n_vars, int k,
              double *components, double *scores, double *var_explained);

/**
 * Condition number estimate using power iteration.
 * kappa_2(A) approx = ||A||_2 * ||A^{-1}||_2
 *
 * @param A  Input matrix (n x n)
 * @param n  Dimension
 * @return   Condition number estimate
 */
double sci_condition_number(const SCI_Matrix *A, int n);

/**
 * Solve symmetric positive definite system via Conjugate Gradient.
 *
 * CG (Hestenes & Stiefel 1952): iterative method for Ax = b with A SPD.
 * Each iteration: O(n^2) for dense, O(nnz) for sparse.
 * Converges in at most n iterations in exact arithmetic.
 *
 * @param A        SPD matrix (n x n)
 * @param b        RHS vector
 * @param x        Initial guess / output solution (size n)
 * @param n        Dimension
 * @param max_iter Maximum iterations
 * @param tol      Tolerance on residual ||r||
 * @param iters    Output: iterations performed
 * @return         true if converged
 */
bool sci_cg_solve(const SCI_Matrix *A, const double *b, double *x, int n,
                   int max_iter, double tol, int *iters);

#endif /* MINI_SCI_LINALG_H */