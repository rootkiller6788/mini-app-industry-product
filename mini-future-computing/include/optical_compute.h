#ifndef MINI_FUTURE_OPTICAL_COMPUTE_H
#define MINI_FUTURE_OPTICAL_COMPUTE_H

#include <stdbool.h>
#include <stdint.h>

/* Optical Computing -- Photonic Matrix Operations
 * L1: OpticalSignal, OpticalMZI, OpticalMesh, OpticalProcessor
 * L2: Coherent optical processing, phase encoding, interference
 * L3: MZI mesh for unitary matrix (Clements mesh)
 * L4: Fourier optics (Goodman 1968), PIC (photonic integrated circuits)
 * L5: Optical matrix-vector multiplication, FFT via lens
 * L6: Optical neural network inference
 * L7: Photonic AI accelerators
 * L8: WDM, coherent detection
 * L9: Lightmatter, Lightelligence, Optalysys
 * References:
 * - Goodman, "Introduction to Fourier Optics" (1968)
 * - Reck et al., PRL (1994)
 * - Clements et al., Optica (2016)
 * - Shen et al., Nature Photonics (2017)
 */

#define OPT_MAX_WAVEGUIDES     256
#define OPT_MAX_MZI            1024
#define OPT_MAX_LAYERS         64
#define OPT_MAX_WAVELENGTHS    32

typedef struct {
    double amplitude;
    double phase;
    double wavelength_nm;
} OpticalSignal;

/* MZI: 2x2 unitary transform U(2) */
typedef struct {
    int id;
    double theta;
    double phi;
    double insertion_loss;
    int input_a;
    int input_b;
    int output_a;
    int output_b;
} OpticalMZI;

/* Clements mesh: N*(N-1)/2 MZIs implement any NxN unitary */
typedef struct {
    int num_ports;
    OpticalMZI mzis[OPT_MAX_MZI];
    int num_mzis;
    int topology[OPT_MAX_WAVEGUIDES];
} OpticalMesh;

typedef struct {
    int input_dim;
    int output_dim;
    OpticalMesh *unitary_mesh;
    double *bias;
    double *nonlinear_threshold;
    double activation_gain;
} OpticalLayer;

typedef struct {
    OpticalLayer layers[OPT_MAX_LAYERS];
    int num_layers;
    double wavelength_nm;
    double optical_power_mW;
    double detector_efficiency;
    double total_loss_dB;
} OpticalProcessor;

OpticalSignal opt_signal_create(double amplitude, double phase, double wl_nm);
OpticalSignal opt_signal_add(OpticalSignal a, OpticalSignal b);
OpticalSignal opt_signal_mul(OpticalSignal a, double scalar);
double opt_signal_intensity(const OpticalSignal *s);
double opt_signal_phase_diff(OpticalSignal a, OpticalSignal b);

OpticalMZI opt_mzi_create(int input_a, int input_b, double theta, double phi);
void opt_mzi_transform(const OpticalMZI *mzi,
                        const OpticalSignal *in_a, const OpticalSignal *in_b,
                        OpticalSignal *out_a, OpticalSignal *out_b);
void opt_mzi_set_unitary(OpticalMZI *mzi, double t11_real, double t11_imag,
                          double t12_real, double t12_imag);

OpticalMesh* opt_mesh_create(int num_ports);
void opt_mesh_destroy(OpticalMesh *mesh);
int  opt_mesh_add_mzi(OpticalMesh *mesh, int phase, double theta, double phi);
int  opt_mesh_forward(const OpticalMesh *mesh, const OpticalSignal *input,
                       OpticalSignal *output);
int  opt_mesh_decompose_matrix(OpticalMesh *mesh,
                                const double *matrix, int dim);
void opt_mesh_get_matrix(const OpticalMesh *mesh, double *matrix, int dim);

OpticalProcessor* opt_processor_create(double wavelength_nm, double power_mW);
void opt_processor_destroy(OpticalProcessor *proc);
int  opt_processor_add_layer(OpticalProcessor *proc, int input_dim,
                              int output_dim);
int  opt_processor_forward(const OpticalProcessor *proc,
                            const OpticalSignal *input, OpticalSignal *output);

int  opt_matrix_vector_mul(const double *matrix, int rows, int cols,
                            const double *x, double *y);
int  opt_batch_matmul(const double *matrix, int rows, int cols,
                       const double *batch_x, int batch_size, double *batch_y);

int  opt_fourier_transform(const OpticalSignal *input, int n,
                            OpticalSignal *output);
int  opt_inverse_fourier(const OpticalSignal *input, int n,
                          OpticalSignal *output);

double opt_saturable_absorber(double x, double I_sat);
double opt_electro_optic_nonlinear(double x, double bias);
void  opt_apply_activation(OpticalSignal *signals, int count, double I_sat);

void opt_print_mesh(const OpticalMesh *mesh);
double opt_total_loss(const OpticalProcessor *proc);
const char* opt_mzi_phase_name(int phase);

#endif
