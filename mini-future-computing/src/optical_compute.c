#include "optical_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Optical Computing -- Photonic Matrix Operations
 * L1: OpticalSignal, OpticalMZI, OpticalMesh, OpticalProcessor
 * L2: Coherent optical processing, phase encoding
 * L3: MZI mesh for unitary matrix (Clements mesh)
 * L4: Fourier optics (Goodman 1968)
 * L5: Optical matrix-vector multiplication, FFT via lens
 * L6: Optical neural network inference
 * References: Goodman (1968), Reck et al. (PRL 1994), Clements et al. (2016)
 */

OpticalSignal opt_signal_create(double amplitude, double phase, double wl_nm) {
    OpticalSignal s;
    s.amplitude = amplitude;
    s.phase = phase;
    s.wavelength_nm = wl_nm;
    return s;
}

OpticalSignal opt_signal_add(OpticalSignal a, OpticalSignal b) {
    double re_a = a.amplitude * cos(a.phase);
    double im_a = a.amplitude * sin(a.phase);
    double re_b = b.amplitude * cos(b.phase);
    double im_b = b.amplitude * sin(b.phase);
    double re_sum = re_a + re_b;
    double im_sum = im_a + im_b;
    OpticalSignal result;
    result.amplitude = sqrt(re_sum * re_sum + im_sum * im_sum);
    result.phase = atan2(im_sum, re_sum);
    result.wavelength_nm = a.wavelength_nm;
    return result;
}

OpticalSignal opt_signal_mul(OpticalSignal a, double scalar) {
    a.amplitude *= scalar;
    return a;
}

double opt_signal_intensity(const OpticalSignal *s) {
    if (!s) return 0.0;
    return s->amplitude * s->amplitude;
}

double opt_signal_phase_diff(OpticalSignal a, OpticalSignal b) {
    double diff = a.phase - b.phase;
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    return diff;
}

/* ---- MZI Operations (L2) ---- */

OpticalMZI opt_mzi_create(int input_a, int input_b, double theta, double phi) {
    OpticalMZI mzi;
    memset(&mzi, 0, sizeof(mzi));
    mzi.input_a = input_a;
    mzi.input_b = input_b;
    mzi.theta = theta;
    mzi.phi = phi;
    mzi.insertion_loss = 0.1;
    return mzi;
}

void opt_mzi_transform(const OpticalMZI *mzi,
                        const OpticalSignal *in_a, const OpticalSignal *in_b,
                        OpticalSignal *out_a, OpticalSignal *out_b) {
    if (!mzi || !in_a || !in_b || !out_a || !out_b) return;
    double T11_re = cos(mzi->theta);
    double T11_im = 0.0;
    double T12_re = 0.0;
    double T12_im = sin(mzi->theta);
    double T21_re = 0.0;
    double T21_im = sin(mzi->theta);
    double T22_re = cos(mzi->theta);
    double T22_im = 0.0;

    double re_a = in_a->amplitude * cos(in_a->phase);
    double im_a = in_a->amplitude * sin(in_a->phase);
    double re_b = in_b->amplitude * cos(in_b->phase);
    double im_b = in_b->amplitude * sin(in_b->phase);

    double re_out_a = T11_re * re_a - T11_im * im_a + T12_re * re_b - T12_im * im_b;
    double im_out_a = T11_re * im_a + T11_im * re_a + T12_re * im_b + T12_im * re_b;
    double re_out_b = T21_re * re_a - T21_im * im_a + T22_re * re_b - T22_im * im_b;
    double im_out_b = T21_re * im_a + T21_im * re_a + T22_re * im_b + T22_im * re_b;

    double loss = pow(10.0, -mzi->insertion_loss / 10.0);
    out_a->amplitude = sqrt(re_out_a * re_out_a + im_out_a * im_out_a) * loss;
    out_a->phase = atan2(im_out_a, re_out_a);
    out_a->wavelength_nm = in_a->wavelength_nm;
    out_b->amplitude = sqrt(re_out_b * re_out_b + im_out_b * im_out_b) * loss;
    out_b->phase = atan2(im_out_b, re_out_b);
    out_b->wavelength_nm = in_b->wavelength_nm;
}

void opt_mzi_set_unitary(OpticalMZI *mzi, double t11_real, double t11_imag,
                          double t12_real, double t12_imag) {
    if (!mzi) return;
    mzi->theta = acos(t11_real);
    mzi->phi = atan2(t11_imag, t11_real);
    (void)t12_real; (void)t12_imag;
}

/* ---- Mesh Operations (L3) ---- */

OpticalMesh* opt_mesh_create(int num_ports) {
    if (num_ports < 2 || num_ports > OPT_MAX_WAVEGUIDES) return NULL;
    OpticalMesh *mesh = (OpticalMesh*)calloc(1, sizeof(OpticalMesh));
    if (!mesh) return NULL;
    mesh->num_ports = num_ports;
    for (int i = 0; i < num_ports; i++) mesh->topology[i] = i;
    return mesh;
}

void opt_mesh_destroy(OpticalMesh *mesh) { free(mesh); }

int opt_mesh_add_mzi(OpticalMesh *mesh, int phase, double theta, double phi) {
    if (!mesh || mesh->num_mzis >= OPT_MAX_MZI) return -1;
    int idx = mesh->num_mzis++;
    mesh->mzis[idx].id = idx;
    mesh->mzis[idx].theta = theta;
    mesh->mzis[idx].phi = phi;
    mesh->mzis[idx].insertion_loss = 0.05;
    return idx;
}

int opt_mesh_forward(const OpticalMesh *mesh, const OpticalSignal *input,
                      OpticalSignal *output) {
    if (!mesh || !input || !output) return -1;
    memcpy(output, input, (size_t)mesh->num_ports * sizeof(OpticalSignal));
    for (int m = 0; m < mesh->num_mzis; m++) {
        const OpticalMZI *mzi = &mesh->mzis[m];
        if (mzi->input_a < mesh->num_ports && mzi->input_b < mesh->num_ports) {
            OpticalSignal tmp_a = output[mzi->input_a];
            OpticalSignal tmp_b = output[mzi->input_b];
            opt_mzi_transform(mzi, &tmp_a, &tmp_b,
                             &output[mzi->input_a], &output[mzi->input_b]);
        }
    }
    return 0;
}

void opt_mesh_get_matrix(const OpticalMesh *mesh, double *matrix, int dim) {
    if (!mesh || !matrix || dim != mesh->num_ports) return;
    for (int i = 0; i < dim * dim; i++) matrix[i] = 0.0;
    for (int i = 0; i < dim; i++) {
        OpticalSignal inputs[OPT_MAX_WAVEGUIDES] = {{0}};
        OpticalSignal outputs[OPT_MAX_WAVEGUIDES] = {{0}};
        inputs[i] = opt_signal_create(1.0, 0.0, 1550.0);
        opt_mesh_forward(mesh, inputs, outputs);
        for (int j = 0; j < dim; j++) {
            matrix[j * dim + i] = outputs[j].amplitude * cos(outputs[j].phase);
        }
    }
}

/* ---- Optical Processor (L5) ---- */

OpticalProcessor* opt_processor_create(double wavelength_nm, double power_mW) {
    OpticalProcessor *proc = (OpticalProcessor*)calloc(1, sizeof(OpticalProcessor));
    if (!proc) return NULL;
    proc->wavelength_nm = wavelength_nm;
    proc->optical_power_mW = power_mW;
    proc->detector_efficiency = 0.8;
    return proc;
}

void opt_processor_destroy(OpticalProcessor *proc) {
    if (!proc) return;
    for (int i = 0; i < proc->num_layers; i++) {
        free(proc->layers[i].unitary_mesh);
        free(proc->layers[i].bias);
        free(proc->layers[i].nonlinear_threshold);
    }
    free(proc);
}

int opt_processor_add_layer(OpticalProcessor *proc, int input_dim,
                             int output_dim) {
    if (!proc || proc->num_layers >= OPT_MAX_LAYERS) return -1;
    int idx = proc->num_layers++;
    OpticalLayer *layer = &proc->layers[idx];
    layer->input_dim = input_dim;
    layer->output_dim = output_dim;
    layer->unitary_mesh = opt_mesh_create(output_dim);
    layer->activation_gain = 1.0;
    layer->bias = (double*)calloc((size_t)output_dim, sizeof(double));
    layer->nonlinear_threshold = (double*)calloc((size_t)output_dim, sizeof(double));
    if (!layer->bias || !layer->nonlinear_threshold) {
        free(layer->bias); free(layer->nonlinear_threshold);
        free(layer->unitary_mesh);
        return -1;
    }
    for (int i = 0; i < output_dim; i++) layer->nonlinear_threshold[i] = 1.0;
    return idx;
}

int opt_processor_forward(const OpticalProcessor *proc,
                           const OpticalSignal *input, OpticalSignal *output) {
    if (!proc || !input || !output) return -1;
    if (proc->num_layers < 1) {
        memcpy(output, input, sizeof(OpticalSignal));
        return 0;
    }
    int cur_dim = proc->layers[0].input_dim;
    OpticalSignal *cur = (OpticalSignal*)malloc((size_t)cur_dim * sizeof(OpticalSignal));
    if (!cur) return -1;
    memcpy(cur, input, (size_t)cur_dim * sizeof(OpticalSignal));

    for (int l = 0; l < proc->num_layers; l++) {
        const OpticalLayer *layer = &proc->layers[l];
        OpticalSignal *next = (OpticalSignal*)calloc((size_t)layer->output_dim, sizeof(OpticalSignal));
        if (!next) { free(cur); return -1; }
        if (layer->unitary_mesh && layer->unitary_mesh->num_mzis > 0) {
            opt_mesh_forward(layer->unitary_mesh, cur, next);
        } else {
            for (int i = 0; i < layer->output_dim && i < cur_dim; i++)
                next[i] = cur[i];
        }
        for (int i = 0; i < layer->output_dim; i++) {
            next[i] = opt_signal_mul(next[i], layer->activation_gain);
        }
        free(cur);
        cur = next;
        cur_dim = layer->output_dim;
    }
    memcpy(output, cur, (size_t)cur_dim * sizeof(OpticalSignal));
    free(cur);
    return 0;
}

/* ---- Matrix-Vector Multiplication ---- */

int opt_matrix_vector_mul(const double *matrix, int rows, int cols,
                           const double *x, double *y) {
    if (!matrix || !x || !y || rows < 1 || cols < 1) return -1;
    for (int i = 0; i < rows; i++) {
        y[i] = 0.0;
        for (int j = 0; j < cols; j++) {
            y[i] += matrix[i * cols + j] * x[j];
        }
    }
    return 0;
}

int opt_batch_matmul(const double *matrix, int rows, int cols,
                      const double *batch_x, int batch_size, double *batch_y) {
    if (!matrix || !batch_x || !batch_y) return -1;
    for (int b = 0; b < batch_size; b++)
        opt_matrix_vector_mul(matrix, rows, cols,
                               &batch_x[b * cols], &batch_y[b * rows]);
    return 0;
}

/* ---- Optical FFT (L5) ---- */

int opt_fourier_transform(const OpticalSignal *input, int n,
                           OpticalSignal *output) {
    if (!input || !output || n < 2) return -1;
    for (int k = 0; k < n; k++) {
        double re = 0.0, im = 0.0;
        for (int j = 0; j < n; j++) {
            double angle = -2.0 * M_PI * (double)(j * k) / (double)n;
            double amp = input[j].amplitude;
            double phase = input[j].phase + angle;
            re += amp * cos(phase);
            im += amp * sin(phase);
        }
        output[k].amplitude = sqrt(re * re + im * im) / sqrt((double)n);
        output[k].phase = atan2(im, re);
        output[k].wavelength_nm = input[0].wavelength_nm;
    }
    return 0;
}

int opt_inverse_fourier(const OpticalSignal *input, int n,
                         OpticalSignal *output) {
    if (!input || !output || n < 2) return -1;
    for (int k = 0; k < n; k++) {
        double re = 0.0, im = 0.0;
        for (int j = 0; j < n; j++) {
            double angle = 2.0 * M_PI * (double)(j * k) / (double)n;
            double amp = input[j].amplitude;
            double phase = input[j].phase + angle;
            re += amp * cos(phase);
            im += amp * sin(phase);
        }
        output[k].amplitude = sqrt(re * re + im * im) / sqrt((double)n);
        output[k].phase = atan2(im, re);
        output[k].wavelength_nm = input[0].wavelength_nm;
    }
    return 0;
}

/* ---- Nonlinearities ---- */

double opt_saturable_absorber(double x, double I_sat) {
    if (I_sat <= 0.0) return x;
    return x / (1.0 + fabs(x) / I_sat);
}

double opt_electro_optic_nonlinear(double x, double bias) {
    return sin(x + bias);
}

void opt_apply_activation(OpticalSignal *signals, int count, double I_sat) {
    if (!signals) return;
    for (int i = 0; i < count; i++) {
        signals[i].amplitude = opt_saturable_absorber(signals[i].amplitude, I_sat);
    }
}

/* ---- Utility ---- */

void opt_print_mesh(const OpticalMesh *mesh) {
    if (!mesh) return;
    printf("OpticalMesh: %d ports, %d MZIs\n", mesh->num_ports, mesh->num_mzis);
    for (int i = 0; i < mesh->num_mzis; i++) {
        printf("  MZI[%d]: theta=%.3f phi=%.3f\n",
               i, mesh->mzis[i].theta, mesh->mzis[i].phi);
    }
}

double opt_total_loss(const OpticalProcessor *proc) {
    if (!proc) return 0.0;
    double total = 0.0;
    for (int l = 0; l < proc->num_layers; l++) {
        if (proc->layers[l].unitary_mesh) {
            for (int m = 0; m < proc->layers[l].unitary_mesh->num_mzis; m++) {
                total += proc->layers[l].unitary_mesh->mzis[m].insertion_loss;
            }
        }
    }
    return total;
}

const char* opt_mzi_phase_name(int phase) { return (phase == 0) ? "Internal" : "External"; }
