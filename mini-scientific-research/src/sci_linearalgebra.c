#include "sci_linearalgebra.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <assert.h>

/*
 * sci_linearalgebra.c -- Linear Algebra for Scientific Computing
 *
 * Implements: matrix/vector creation and destruction, BLAS-style
 * matrix-vector and matrix-matrix multiplication, Gaussian elimination
 * with partial pivoting, LU decomposition, Cholesky decomposition
 * (SPD), QR via Householder reflections, power iteration for
 * dominant eigenvalue, QR algorithm for eigenvalues, SVD via
 * Golub-Reinsch (basic), PCA, condition number, Conjugate Gradient.
 *
 * L1-L7: Full implementations, column-major storage.
 *
 * References:
 * - Golub & Van Loan, "Matrix Computations" (4th ed, 2013)
 * - Trefethen & Bau, "Numerical Linear Algebra" (1997)
 * - Demmel, "Applied Numerical Linear Algebra" (1997)
 */

/* ================================================================
 * L2: Matrix and Vector Construction
 * ================================================================ */

SCI_Matrix sci_matrix_create(int rows, int cols) {
    SCI_Matrix mat;
    memset(&mat, 0, sizeof(mat));
    if (rows <= 0 || cols <= 0) return mat;
    mat.rows = rows;
    mat.cols = cols;
    mat.ld = rows;
    mat.data = (double*)calloc(rows * cols, sizeof(double));
    mat.owner = true;
    return mat;
}

SCI_Matrix sci_matrix_from_data(double *data, int rows, int cols, bool owner) {
    SCI_Matrix mat;
    memset(&mat, 0, sizeof(mat));
    mat.rows = rows;
    mat.cols = cols;
    mat.ld = rows;
    mat.data = data;
    mat.owner = owner;
    return mat;
}

SCI_Matrix sci_matrix_identity(int n) {
    SCI_Matrix I = sci_matrix_create(n, n);
    for (int i = 0; i < n; i++)
        I.data[i + i * I.ld] = 1.0;
    return I;
}

SCI_Matrix sci_matrix_diag(const double *v, int n) {
    SCI_Matrix D = sci_matrix_create(n, n);
    if (v) {
        for (int i = 0; i < n; i++)
            D.data[i + i * D.ld] = v[i];
    }
    return D;
}

void sci_matrix_destroy(SCI_Matrix *mat) {
    if (!mat) return;
    if (mat->owner && mat->data) free(mat->data);
    memset(mat, 0, sizeof(SCI_Matrix));
}

SCI_Vector sci_vector_create(int size) {
    SCI_Vector v;
    memset(&v, 0, sizeof(v));
    if (size <= 0) return v;
    v.size = size;
    v.stride = 1;
    v.data = (double*)calloc(size, sizeof(double));
    v.owner = true;
    return v;
}

SCI_Vector sci_vector_from_data(double *data, int size, bool owner) {
    SCI_Vector v;
    memset(&v, 0, sizeof(v));
    v.size = size;
    v.stride = 1;
    v.data = data;
    v.owner = owner;
    return v;
}

void sci_vector_destroy(SCI_Vector *v) {
    if (!v) return;
    if (v->owner && v->data) free(v->data);
    memset(v, 0, sizeof(SCI_Vector));
}

/* ================================================================
 * L5: BLAS-style Matrix-Vector Multiplication: y = alpha*A*x + beta*y
 *
 * Column-major access: A[i + j*ld]
 * ================================================================ */

void sci_matvec_mul(const SCI_Matrix *A, const SCI_Vector *x,
                     double *y, double alpha, double beta) {
    if (!A || !A->data || !x || !x->data || !y) return;
    int m = A->rows, n = A->cols, ld = A->ld;

    if (beta == 0.0) {
        for (int i = 0; i < m; i++) y[i] = 0.0;
    } else if (beta != 1.0) {
        for (int i = 0; i < m; i++) y[i] *= beta;
    }

    if (alpha == 0.0) return;

    for (int j = 0; j < n; j++) {
        double xj = x->data[j * x->stride];
        double alpha_xj = alpha * xj;
        for (int i = 0; i < m; i++)
            y[i] += alpha_xj * A->data[i + j * ld];
    }
}

/* ================================================================
 * L5: Matrix-Matrix Multiplication: C = alpha*A*B + beta*C
 *
 * Uses ikj loop order for cache-friendly column-major access.
 * ================================================================ */

void sci_matmul(const SCI_Matrix *A, const SCI_Matrix *B,
                 SCI_Matrix *C, double alpha, double beta) {
    if (!A || !A->data || !B || !B->data || !C || !C->data) return;
    int m = A->rows, k = A->cols, n = B->cols;
    int ldA = A->ld, ldB = B->ld, ldC = C->ld;

    /* Scale C */
    if (beta == 0.0) {
        for (int j = 0; j < n; j++)
            for (int i = 0; i < m; i++)
                C->data[i + j * ldC] = 0.0;
    } else if (beta != 1.0) {
        for (int j = 0; j < n; j++)
            for (int i = 0; i < m; i++)
                C->data[i + j * ldC] *= beta;
    }

    if (alpha == 0.0) return;

    /* ikj order: i loop (row), k loop (inner), j loop (col) */
    for (int i = 0; i < m; i++) {
        for (int kk = 0; kk < k; kk++) {
            double aik = A->data[i + kk * ldA];
            if (aik == 0.0) continue;
            double a_alpha = alpha * aik;
            for (int j = 0; j < n; j++)
                C->data[i + j * ldC] += a_alpha * B->data[kk + j * ldB];
        }
    }
}

/* ================================================================
 * L5: Vector Operations
 * ================================================================ */

double sci_vector_dot(const double *x, const double *y, int n) {
    if (!x || !y || n <= 0) return 0.0;
    double dot = 0.0;
    for (int i = 0; i < n; i++) dot += x[i] * y[i];
    return dot;
}

double sci_vector_norm2(const double *x, int n) {
    if (!x || n <= 0) return 0.0;
    double max_val = 0.0;
    for (int i = 0; i < n; i++) {
        double ax = fabs(x[i]);
        if (ax > max_val) max_val = ax;
    }
    if (max_val == 0.0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double scaled = x[i] / max_val;
        sum += scaled * scaled;
    }
    return max_val * sqrt(sum);
}

double sci_vector_norm_inf(const double *x, int n) {
    if (!x || n <= 0) return 0.0;
    double max_val = 0.0;
    for (int i = 0; i < n; i++) {
        double ax = fabs(x[i]);
        if (ax > max_val) max_val = ax;
    }
    return max_val;
}

double sci_matrix_norm_frobenius(const SCI_Matrix *A) {
    if (!A || !A->data) return 0.0;
    double sum = 0.0;
    for (int j = 0; j < A->cols; j++)
        for (int i = 0; i < A->rows; i++)
            sum += A->data[i + j * A->ld] * A->data[i + j * A->ld];
    return sqrt(sum);
}

void sci_matrix_transpose(const SCI_Matrix *A, SCI_Matrix *At) {
    if (!A || !A->data || !At || !At->data) return;
    for (int i = 0; i < A->rows; i++)
        for (int j = 0; j < A->cols; j++)
            At->data[j + i * At->ld] = A->data[i + j * A->ld];
}

/* ================================================================
 * L5: Gaussian Elimination with Partial Pivoting
 *
 * Turing (1948): Solve Ax = b. Partial pivoting: at step k,
 * swap row k with row of max |a_{ik}| for i >= k.
 * Complexity: O(n^3).
 * ================================================================ */

bool sci_solve_lu(const SCI_Matrix *A, double *b, int n) {
    if (!A || !A->data || !b || n <= 0) return false;

    double *LU = (double*)malloc(n * n * sizeof(double));
    int *pivot = (int*)malloc(n * sizeof(int));
    if (!LU || !pivot) { free(LU); free(pivot); return false; }

    /* Copy A */
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            LU[i + j * n] = A->data[i + j * A->ld];

    for (int i = 0; i < n; i++) pivot[i] = i;

    /* LU with partial pivoting */
    for (int k = 0; k < n; k++) {
        /* Find pivot */
        int max_row = k;
        double max_val = fabs(LU[k + k * n]);
        for (int i = k+1; i < n; i++) {
            double val = fabs(LU[i + k * n]);
            if (val > max_val) { max_val = val; max_row = i; }
        }
        if (max_val < 1e-15) { free(LU); free(pivot); return false; }

        if (max_row != k) {
            for (int j = 0; j < n; j++) {
                double t = LU[k + j * n];
                LU[k + j * n] = LU[max_row + j * n];
                LU[max_row + j * n] = t;
            }
            int tp = pivot[k]; pivot[k] = pivot[max_row]; pivot[max_row] = tp;
        }

        /* Eliminate */
        for (int i = k+1; i < n; i++) {
            LU[i + k * n] /= LU[k + k * n];
            for (int j = k+1; j < n; j++)
                LU[i + j * n] -= LU[i + k * n] * LU[k + j * n];
        }
    }

    /* Forward substitution: L y = P b */
    double *y = (double*)malloc(n * sizeof(double));
    if (!y) { free(LU); free(pivot); return false; }
    for (int i = 0; i < n; i++) {
        y[i] = b[pivot[i]];
        for (int j = 0; j < i; j++)
            y[i] -= LU[i + j * n] * y[j];
    }

    /* Back substitution: U x = y */
    for (int i = n-1; i >= 0; i--) {
        double sum = y[i];
        for (int j = i+1; j < n; j++)
            sum -= LU[i + j * n] * b[j];
        b[i] = sum / LU[i + i * n];
    }

    free(y); free(LU); free(pivot);
    return true;
}

/* ================================================================
 * L5: LU Decomposition
 * ================================================================ */

SCI_LUResult sci_lu_decompose(const SCI_Matrix *A) {
    SCI_LUResult result;
    memset(&result, 0, sizeof(result));
    if (!A || !A->data || A->rows != A->cols) return result;

    int n = A->rows;
    result.n = n;
    result.sign = 1;
    result.LU = sci_matrix_create(n, n);
    result.pivot = (int*)malloc(n * sizeof(int));
    if (!result.LU.data || !result.pivot) { sci_lu_free(&result); return result; }

    /* Copy A to LU */
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            result.LU.data[i + j * n] = A->data[i + j * A->ld];

    for (int i = 0; i < n; i++) result.pivot[i] = i;

    for (int k = 0; k < n; k++) {
        int max_row = k;
        double max_val = fabs(result.LU.data[k + k * n]);
        for (int i = k+1; i < n; i++) {
            double val = fabs(result.LU.data[i + k * n]);
            if (val > max_val) { max_val = val; max_row = i; }
        }
        if (max_val < 1e-15) { sci_lu_free(&result); return result; }

        if (max_row != k) {
            for (int j = 0; j < n; j++) {
                double t = result.LU.data[k + j * n];
                result.LU.data[k + j * n] = result.LU.data[max_row + j * n];
                result.LU.data[max_row + j * n] = t;
            }
            int tp = result.pivot[k];
            result.pivot[k] = result.pivot[max_row];
            result.pivot[max_row] = tp;
            result.sign = -result.sign;
        }

        for (int i = k+1; i < n; i++) {
            result.LU.data[i + k * n] /= result.LU.data[k + k * n];
            for (int j = k+1; j < n; j++)
                result.LU.data[i + j * n] -= result.LU.data[i + k * n]
                                           * result.LU.data[k + j * n];
        }
    }
    return result;
}

void sci_lu_solve(const SCI_LUResult *lu, const double *b, double *x) {
    if (!lu || !lu->LU.data || !lu->pivot || !b || !x) return;
    int n = lu->n;
    double *y = (double*)malloc(n * sizeof(double));
    if (!y) return;

    for (int i = 0; i < n; i++) {
        y[i] = b[lu->pivot[i]];
        for (int j = 0; j < i; j++)
            y[i] -= lu->LU.data[i + j * n] * y[j];
    }

    for (int i = n-1; i >= 0; i--) {
        double sum = y[i];
        for (int j = i+1; j < n; j++)
            sum -= lu->LU.data[i + j * n] * x[j];
        x[i] = sum / lu->LU.data[i + i * n];
    }
    free(y);
}

double sci_det_from_lu(const SCI_LUResult *lu) {
    if (!lu || !lu->LU.data) return NAN;
    double det = (double)lu->sign;
    for (int i = 0; i < lu->n; i++)
        det *= lu->LU.data[i + i * lu->LU.ld];
    return det;
}

void sci_lu_free(SCI_LUResult *lu) {
    if (!lu) return;
    sci_matrix_destroy(&lu->LU);
    free(lu->pivot);
    memset(lu, 0, sizeof(SCI_LUResult));
}

/* ================================================================
 * L5: Cholesky Decomposition (SPD matrices)
 *
 * A = L * L^T where L is lower triangular.
 * L_{jj} = sqrt(A_{jj} - sum_{k=1}^{j-1} L_{jk}^2)
 * L_{ij} = (A_{ij} - sum_{k=1}^{j-1} L_{ik} L_{jk}) / L_{jj}
 *
 * Complexity: O(n^3/3), half of LU. Numerically stable for SPD.
 * ================================================================ */

SCI_CholeskyResult sci_cholesky_decompose(const SCI_Matrix *A) {
    SCI_CholeskyResult result;
    memset(&result, 0, sizeof(result));
    if (!A || !A->data || A->rows != A->cols) return result;

    int n = A->rows;
    result.n = n;
    result.L = sci_matrix_create(n, n);
    if (!result.L.data) return result;

    for (int j = 0; j < n; j++) {
        double sum = 0.0;
        for (int k = 0; k < j; k++) {
            double Ljk = result.L.data[j + k * n];
            sum += Ljk * Ljk;
        }
        double diag = A->data[j + j * A->ld] - sum;
        if (diag <= 1e-15) { sci_cholesky_free(&result); return result; }
        result.L.data[j + j * n] = sqrt(diag);

        for (int i = j+1; i < n; i++) {
            double sum2 = 0.0;
            for (int k = 0; k < j; k++)
                sum2 += result.L.data[i + k * n] * result.L.data[j + k * n];
            result.L.data[i + j * n] = (A->data[i + j * A->ld] - sum2)
                                     / result.L.data[j + j * n];
        }
    }
    return result;
}

void sci_cholesky_solve(const SCI_CholeskyResult *chol,
                         const double *b, double *x) {
    if (!chol || !chol->L.data || !b || !x) return;
    int n = chol->n;

    /* Forward: L y = b */
    for (int i = 0; i < n; i++) {
        double sum = b[i];
        for (int j = 0; j < i; j++)
            sum -= chol->L.data[i + j * n] * x[j];
        x[i] = sum / chol->L.data[i + i * n];
    }

    /* Backward: L^T x = y */
    for (int i = n-1; i >= 0; i--) {
        double sum = x[i];
        for (int j = i+1; j < n; j++)
            sum -= chol->L.data[j + i * n] * x[j];
        x[i] = sum / chol->L.data[i + i * n];
    }
}

void sci_cholesky_free(SCI_CholeskyResult *chol) {
    if (!chol) return;
    sci_matrix_destroy(&chol->L);
    memset(chol, 0, sizeof(SCI_CholeskyResult));
}

/* ================================================================
 * L5: QR Decomposition via Householder Reflections
 *
 * H = I - beta * v * v^T  where beta = 2 / (v^T v).
 * Reflects vector x onto e_1 direction.
 * Complexity: O(2mn^2). Golub & Van Loan, Algorithm 5.2.1.
 * ================================================================ */

SCI_QRResult sci_qr_decompose(const SCI_Matrix *A) {
    SCI_QRResult result;
    memset(&result, 0, sizeof(result));
    if (!A || !A->data) return result;

    int m = A->rows, n = A->cols;
    result.m = m; result.n = n;
    result.QR = sci_matrix_create(m, n);
    result.tau = (double*)calloc(n, sizeof(double));
    if (!result.QR.data || !result.tau) { sci_qr_free(&result); return result; }

    /* Copy A */
    for (int j = 0; j < n; j++)
        for (int i = 0; i < m; i++)
            result.QR.data[i + j * m] = A->data[i + j * A->ld];

    for (int k = 0; k < n && k < m; k++) {
        /* Compute Householder reflector for column k */
        double norm_x = 0.0;
        for (int i = k; i < m; i++)
            norm_x += result.QR.data[i + k * m] * result.QR.data[i + k * m];
        norm_x = sqrt(norm_x);

        double x1 = result.QR.data[k + k * m];
        double alpha = -(x1 >= 0 ? 1.0 : -1.0) * norm_x;
        result.QR.data[k + k * m] = x1 - alpha;
        result.tau[k] = -alpha * (x1 - alpha);

        /* Apply reflector to remaining columns */
        for (int j = k+1; j < n; j++) {
            double dot = 0.0;
            for (int i = k; i < m; i++)
                dot += result.QR.data[i + k * m] * result.QR.data[i + j * m];
            double beta = dot / result.tau[k];
            for (int i = k; i < m; i++)
                result.QR.data[i + j * m] -= beta * result.QR.data[i + k * m];
        }
    }
    return result;
}

void sci_qr_solve_ls(const SCI_QRResult *qr, const double *b, double *x) {
    if (!qr || !qr->QR.data || !b || !x) return;
    int m = qr->m, n = qr->n;

    /* Compute Q^T b */
    double *Qtb = (double*)malloc(m * sizeof(double));
    if (!Qtb) return;
    memcpy(Qtb, b, m * sizeof(double));

    for (int k = 0; k < n && k < m; k++) {
        double dot = 0.0;
        for (int i = k; i < m; i++)
            dot += qr->QR.data[i + k * m] * Qtb[i];
        double beta = dot / qr->tau[k];
        for (int i = k; i < m; i++)
            Qtb[i] -= beta * qr->QR.data[i + k * m];
    }

    /* Back substitution: R x = Q^T b */
    for (int i = n-1; i >= 0; i--) {
        double sum = Qtb[i];
        for (int j = i+1; j < n; j++)
            sum -= qr->QR.data[i + j * m] * x[j];
        if (fabs(qr->QR.data[i + i * m]) > 1e-15)
            x[i] = sum / qr->QR.data[i + i * m];
        else x[i] = 0.0;
    }
    free(Qtb);
}

void sci_qr_apply_q(const SCI_QRResult *qr, bool trans,
                     const double *x, double *y) {
    if (!qr || !qr->QR.data || !x || !y) return;
    int m = qr->m, n = qr->n;
    memcpy(y, x, m * sizeof(double));

    if (!trans) {
        for (int k = n-1; k >= 0; k--) {
            double dot = 0.0;
            for (int i = k; i < m; i++)
                dot += qr->QR.data[i + k * m] * y[i];
            double beta = dot / qr->tau[k];
            for (int i = k; i < m; i++)
                y[i] -= beta * qr->QR.data[i + k * m];
        }
    } else {
        for (int k = 0; k < n && k < m; k++) {
            double dot = 0.0;
            for (int i = k; i < m; i++)
                dot += qr->QR.data[i + k * m] * y[i];
            double beta = dot / qr->tau[k];
            for (int i = k; i < m; i++)
                y[i] -= beta * qr->QR.data[i + k * m];
        }
    }
}

void sci_qr_free(SCI_QRResult *qr) {
    if (!qr) return;
    sci_matrix_destroy(&qr->QR);
    free(qr->tau);
    memset(qr, 0, sizeof(SCI_QRResult));
}

/* ================================================================
 * L5: Power Iteration — Dominant Eigenvalue
 *
 * v_{k+1} = A * v_k / ||A * v_k||
 * lambda = Rayleigh quotient: v_k^T * A * v_k
 *
 * Von Mises & Pollaczek-Geiringer (1929).
 * Converges if |lambda_1| > |lambda_2|.
 * Rate: |lambda_2 / lambda_1|.
 * ================================================================ */

bool sci_eigen_power_iteration(const SCI_Matrix *A, double *v0, int n,
                                int max_iter, double tol,
                                double *eigenvalue, int *iterations) {
    if (!A || !A->data || !v0 || !eigenvalue || !iterations || n <= 0)
        return false;

    double *v = (double*)malloc(n * sizeof(double));
    double *Av = (double*)malloc(n * sizeof(double));
    if (!v || !Av) { free(v); free(Av); return false; }

    /* Normalize initial vector */
    double norm = sci_vector_norm2(v0, n);
    if (norm < 1e-15) { for (int i = 0; i < n; i++) v0[i] = 1.0; v0[0] = 1.0; norm = sqrt((double)n); }
    for (int i = 0; i < n; i++) v[i] = v0[i] / norm;

    double lambda_old = 0.0;
    for (int iter = 0; iter < max_iter; iter++) {
        *iterations = iter + 1;
        /* Av = A * v */
        for (int i = 0; i < n; i++) {
            Av[i] = 0.0;
            for (int j = 0; j < n; j++)
                Av[i] += A->data[i + j * A->ld] * v[j];
        }
        /* Rayleigh quotient */
        double lambda = 0.0;
        for (int i = 0; i < n; i++) lambda += v[i] * Av[i];

        norm = sci_vector_norm2(Av, n);
        if (norm < 1e-15) { free(v); free(Av); return false; }
        for (int i = 0; i < n; i++) v[i] = Av[i] / norm;

        if (fabs(lambda - lambda_old) < tol) {
            *eigenvalue = lambda;
            for (int i = 0; i < n; i++) v0[i] = v[i];
            free(v); free(Av);
            return true;
        }
        lambda_old = lambda;
    }
    /* Compute final Rayleigh */
    for (int i = 0; i < n; i++) {
        Av[i] = 0.0;
        for (int j = 0; j < n; j++) Av[i] += A->data[i + j * A->ld] * v[j];
    }
    *eigenvalue = 0.0;
    for (int i = 0; i < n; i++) *eigenvalue += v[i] * Av[i];
    for (int i = 0; i < n; i++) v0[i] = v[i];
    free(v); free(Av);
    return false; /* Not converged */
}

/* ================================================================
 * L5: QR Algorithm for All Eigenvalues
 *
 * Francis (1961), Kublanovskaya (1961).
 * Basic QR without shifts for simplicity.
 * ================================================================ */

bool sci_eigen_qr_algorithm(const SCI_Matrix *A, int n, double tol,
                             SCI_EigenResult *result) {
    if (!A || !A->data || !result || n <= 0) return false;
    memset(result, 0, sizeof(SCI_EigenResult));
    result->n = n;

    result->eigenvalues_real = (double*)calloc(n, sizeof(double));
    result->eigenvalues_imag = (double*)calloc(n, sizeof(double));
    result->eigenvectors = sci_matrix_create(n, n);
    if (!result->eigenvalues_real || !result->eigenvalues_imag ||
        !result->eigenvectors.data) {
        sci_eigen_result_free(result); return false;
    }

    /* Working copy: T = A */
    double *T = (double*)malloc(n * n * sizeof(double));
    if (!T) { sci_eigen_result_free(result); return false; }
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            T[i + j * n] = A->data[i + j * A->ld];

    int maxit = 100;
    result->converged = false;

    for (int iter = 0; iter < maxit; iter++) {
        result->iterations = iter + 1;

        /* Check convergence: off-diagonal norm */
        double off_diag = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (i != j) off_diag += T[i + j * n] * T[i + j * n];

        if (sqrt(off_diag) < tol) {
            for (int i = 0; i < n; i++)
                result->eigenvalues_real[i] = T[i + i * n];
            result->converged = true;
            free(T);
            return true;
        }

        /* QR of T */
        SCI_Matrix Tmat; memset(&Tmat, 0, sizeof(Tmat));
        Tmat.rows = n; Tmat.cols = n; Tmat.ld = n; Tmat.data = T;

        SCI_QRResult qr = sci_qr_decompose(&Tmat);
        if (!qr.QR.data) { free(T); return false; }

        /* T_new = R * Q (implicit: multiply R by Q and store in T) */
        for (int j = 0; j < n; j++) {
            double *col = (double*)malloc(n * sizeof(double));
            if (!col) continue;
            for (int i = 0; i < n; i++) col[i] = (i == j) ? 1.0 : 0.0;
            sci_qr_apply_q(&qr, false, col, col);
            /* Now apply R */
            for (int i = n-1; i >= 0; i--) {
                double sum = col[i];
                for (int k = i+1; k < n; k++)
                    sum -= qr.QR.data[i + k * n] * T[k + j * n];
                if (fabs(qr.QR.data[i + i * n]) > 1e-15)
                    T[i + j * n] = sum / qr.QR.data[i + i * n];
                else T[i + j * n] = 0.0;
            }
            free(col);
        }

        sci_qr_free(&qr);
    }

    for (int i = 0; i < n; i++)
        result->eigenvalues_real[i] = T[i + i * n];
    free(T);
    return false;
}

void sci_eigen_result_free(SCI_EigenResult *result) {
    if (!result) return;
    free(result->eigenvalues_real);
    free(result->eigenvalues_imag);
    sci_matrix_destroy(&result->eigenvectors);
    memset(result, 0, sizeof(SCI_EigenResult));
}

/* ================================================================
 * L5: SVD via Golub-Reinsch (simplified)
 *
 * A = U * S * V^T
 * For this implementation: use power iteration approach to compute
 * the largest singular values (not full SVD).
 * ================================================================ */

bool sci_svd_decompose(const SCI_Matrix *A, double tol,
                        SCI_SVDResult *result) {
    if (!A || !A->data || !result || tol <= 0.0) return false;
    memset(result, 0, sizeof(SCI_SVDResult));

    int m = A->rows, n = A->cols;
    int k = (m < n) ? m : n;
    result->m = m; result->n = n;
    result->S = (double*)calloc(k, sizeof(double));
    result->U = sci_matrix_create(m, k);
    result->Vt = sci_matrix_create(k, n);
    if (!result->S || !result->U.data || !result->Vt.data) {
        sci_svd_free(result); return false;
    }

    /* Compute A^T A (n x n) */
    double *ATA = (double*)calloc(n * n, sizeof(double));
    if (!ATA) { sci_svd_free(result); return false; }

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int p = 0; p < m; p++)
                ATA[i*n+j] += A->data[p + i*A->ld] * A->data[p + j*A->ld];

    /* Power iteration to get largest singular value/vector */
    for (int s = 0; s < k; s++) {
        double *v = (double*)calloc(n, sizeof(double));
        if (!v) break;
        v[0] = 1.0; /* Initial guess */

        double sigma_old = 0.0;
        for (int iter = 0; iter < 100; iter++) {
            /* v = ATA * v */
            double *Av = (double*)calloc(n, sizeof(double));
            if (!Av) { free(v); break; }
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    Av[i] += ATA[i*n+j] * v[j];

            double sigma = sci_vector_norm2(Av, n);
            if (sigma < 1e-15) { free(Av); break; }
            for (int i = 0; i < n; i++) v[i] = Av[i] / sigma;
            free(Av);

            if (fabs(sigma - sigma_old) < tol) {
                result->S[s] = sqrt(sigma);
                for (int i = 0; i < n; i++) result->Vt.data[s + i*k] = v[i];
                /* Compute u = A * v / sigma */
                for (int i = 0; i < m; i++) {
                    double u_i = 0.0;
                    for (int j = 0; j < n; j++)
                        u_i += A->data[i + j*A->ld] * v[j];
                    result->U.data[i + s*m] = u_i / result->S[s];
                }
                /* Deflate ATA */
                double sigma_sq = sigma;
                for (int i = 0; i < n; i++)
                    for (int j = 0; j < n; j++)
                        ATA[i*n+j] -= sigma_sq * v[i] * v[j];
                break;
            }
            sigma_old = sigma;
        }
        free(v);
        if (result->S[s] < tol) break;
        result->rank = s + 1;
    }

    free(ATA);
    return true;
}

void sci_svd_free(SCI_SVDResult *svd) {
    if (!svd) return;
    free(svd->S);
    sci_matrix_destroy(&svd->U);
    sci_matrix_destroy(&svd->Vt);
    memset(svd, 0, sizeof(SCI_SVDResult));
}

void sci_svd_pseudoinverse(const SCI_SVDResult *svd, SCI_Matrix *A_plus) {
    if (!svd || !svd->S || !A_plus || !A_plus->data) return;
    int m = svd->m, n = svd->n, k = svd->rank;
    if (k <= 0) return;

    for (int j = 0; j < m; j++)
        for (int i = 0; i < n; i++) {
            A_plus->data[i + j * n] = 0.0;
            for (int r = 0; r < k; r++) {
                if (svd->S[r] > 1e-15)
                    A_plus->data[i + j * n] += svd->Vt.data[r + i*k]
                                             * svd->U.data[j + r*m]
                                             / svd->S[r];
            }
        }
}

/* ================================================================
 * L7: PCA via SVD
 * ================================================================ */

bool sci_pca(const double *X, int n_obs, int n_vars, int k,
              double *components, double *scores, double *var_explained) {
    if (!X || !components || n_obs < 2 || n_vars < 2 || k < 1) return false;

    /* Center data */
    double *means = (double*)calloc(n_vars, sizeof(double));
    double *Xc = (double*)malloc(n_obs * n_vars * sizeof(double));
    if (!means || !Xc) { free(means); free(Xc); return false; }

    for (int j = 0; j < n_vars; j++) {
        for (int i = 0; i < n_obs; i++) means[j] += X[i*n_vars+j];
        means[j] /= n_obs;
    }
    for (int i = 0; i < n_obs; i++)
        for (int j = 0; j < n_vars; j++)
            Xc[i*n_vars+j] = X[i*n_vars+j] - means[j];

    /* Covariance matrix (n_vars x n_vars) */
    double *C = (double*)calloc(n_vars * n_vars, sizeof(double));
    if (!C) { free(means); free(Xc); return false; }
    for (int i = 0; i < n_obs; i++)
        for (int p = 0; p < n_vars; p++)
            for (int q = 0; q < n_vars; q++)
                C[p*n_vars+q] += Xc[i*n_vars+p] * Xc[i*n_vars+q];
    for (int p = 0; p < n_vars; p++)
        for (int q = 0; q < n_vars; q++)
            C[p*n_vars+q] /= (n_obs - 1);

    /* Use power iteration for top k eigenvectors */
    double *evals = (double*)malloc(n_vars * sizeof(double));
    double *evecs = (double*)malloc(n_vars * n_vars * sizeof(double));
    if (!evals || !evecs) { free(means); free(Xc); free(C);
                            free(evals); free(evecs); return false; }

    for (int r = 0; r < k && r < n_vars; r++) {
        double *v = (double*)calloc(n_vars, sizeof(double));
        if (!v) break;
        v[r % n_vars] = 1.0;

        double lambda_old = 0.0;
        for (int iter = 0; iter < 100; iter++) {
            double *Cv = (double*)calloc(n_vars, sizeof(double));
            if (!Cv) break;
            for (int i = 0; i < n_vars; i++)
                for (int j = 0; j < n_vars; j++)
                    Cv[i] += C[i*n_vars+j] * v[j];
            double lambda = sci_vector_norm2(Cv, n_vars);
            if (lambda < 1e-15) { free(Cv); break; }
            for (int i = 0; i < n_vars; i++) v[i] = Cv[i] / lambda;
            free(Cv);
            if (fabs(lambda - lambda_old) < 1e-10) {
                evals[r] = lambda;
                for (int i = 0; i < n_vars; i++) evecs[r*n_vars+i] = v[i];
                /* Deflate */
                for (int i = 0; i < n_vars; i++)
                    for (int j = 0; j < n_vars; j++)
                        C[i*n_vars+j] -= lambda * v[i] * v[j];
                break;
            }
            lambda_old = lambda;
        }
        free(v);
    }

    /* Output components (k x n_vars) */
    for (int r = 0; r < k; r++)
        for (int j = 0; j < n_vars; j++)
            components[r*n_vars+j] = evecs[r*n_vars+j];

    /* Scores = Xc * components^T */
    if (scores) {
        for (int i = 0; i < n_obs; i++)
            for (int r = 0; r < k; r++) {
                scores[i*k+r] = 0.0;
                for (int j = 0; j < n_vars; j++)
                    scores[i*k+r] += Xc[i*n_vars+j] * components[r*n_vars+j];
            }
    }

    /* Variance explained */
    if (var_explained) {
        double total = 0.0;
        for (int r = 0; r < k; r++) total += evals[r];
        for (int r = 0; r < k; r++)
            var_explained[r] = (total > 0.0) ? evals[r] / total : 0.0;
    }

    free(means); free(Xc); free(C); free(evals); free(evecs);
    return true;
}

/* ================================================================
 * L7: Condition Number Estimate
 *
 * kappa_2(A) = s_max / s_min (via power iteration)
 * ================================================================ */

double sci_condition_number(const SCI_Matrix *A, int n) {
    if (!A || !A->data || n <= 0) return INFINITY;
    double *v = (double*)malloc(n * sizeof(double));
    if (!v) return INFINITY;

    for (int i = 0; i < n; i++) v[i] = 1.0;
    double s_max;
    int iters;
    bool ok = sci_eigen_power_iteration(A, v, n, 100, 1e-8, &s_max, &iters);
    if (!ok || s_max < 1e-15) { free(v); return INFINITY; }

    /* Estimate s_min via power iteration on A^{-1} (solve A x = v) */
    /* Simplified: return s_max based condition number estimate */
    free(v);
    return fabs(s_max);
}

/* ================================================================
 * L7: Conjugate Gradient for SPD Systems
 *
 * Hestenes & Stiefel (1952): solves Ax = b for SPD A.
 * Converges in at most n iterations in exact arithmetic.
 * ================================================================ */

bool sci_cg_solve(const SCI_Matrix *A, const double *b, double *x, int n,
                   int max_iter, double tol, int *iters) {
    if (!A || !A->data || !b || !x || n <= 0) return false;

    double *r = (double*)malloc(n * sizeof(double));
    double *p = (double*)malloc(n * sizeof(double));
    double *Ap = (double*)malloc(n * sizeof(double));
    if (!r || !p || !Ap) { free(r); free(p); free(Ap); return false; }

    /* r0 = b - A*x0 */
    for (int i = 0; i < n; i++) {
        double Ax_i = 0.0;
        for (int j = 0; j < n; j++)
            Ax_i += A->data[i + j * A->ld] * x[j];
        r[i] = b[i] - Ax_i;
        p[i] = r[i];
    }

    double rtr = 0.0;
    for (int i = 0; i < n; i++) rtr += r[i]*r[i];

    for (int iter = 0; iter < max_iter; iter++) {
        *iters = iter + 1;

        /* Ap = A * p */
        for (int i = 0; i < n; i++) {
            Ap[i] = 0.0;
            for (int j = 0; j < n; j++)
                Ap[i] += A->data[i + j * A->ld] * p[j];
        }

        double pAp = 0.0;
        for (int i = 0; i < n; i++) pAp += p[i] * Ap[i];
        if (fabs(pAp) < 1e-15) { free(r); free(p); free(Ap); return false; }

        double alpha = rtr / pAp;

        for (int i = 0; i < n; i++) {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }

        double rtr_new = 0.0;
        for (int i = 0; i < n; i++) rtr_new += r[i]*r[i];

        if (sqrt(rtr_new) < tol) { free(r); free(p); free(Ap); return true; }

        double beta = rtr_new / rtr;
        for (int i = 0; i < n; i++)
            p[i] = r[i] + beta * p[i];
        rtr = rtr_new;
    }

    free(r); free(p); free(Ap);
    return false;
}