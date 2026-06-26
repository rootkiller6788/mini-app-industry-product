#ifndef RECOMMENDER_H
#define RECOMMENDER_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* L1: Core Definitions — Recommendation System */

#define REC_MAX_USERS     10000
#define REC_MAX_ITEMS     50000
#define REC_MAX_FEATURES  128
#define REC_TOP_K          20
#define REC_EMBED_DIM      64

typedef enum { REC_COLLABORATIVE, REC_CONTENT_BASED, REC_HYBRID, REC_MATRIX_FACT } RecAlgorithm;

typedef struct {
    uint32_t user_id, item_id;
    double rating;
    uint64_t timestamp;
} RatingRecord;

typedef struct {
    float *weights;
    uint32_t dim;
} Embedding;

typedef struct {
    uint32_t user_count, item_count, rating_count;
    RatingRecord *ratings;
    double **user_item_matrix;
    double global_mean, *user_bias, *item_bias;
} RatingMatrix;

typedef struct {
    Embedding *user_emb, *item_emb;
    uint32_t num_users, num_items, latent_dim;
    double learning_rate, lambda_reg;
    uint64_t iterations;
    double rmse;
} MatrixFactorization;

typedef struct {
    uint32_t user_id, item_id;
    double predicted_rating;
} Recommendation;

/* L1 API */
void rec_rating_matrix_init(RatingMatrix *rm, uint32_t users, uint32_t items, uint32_t max_ratings);
void rec_add_rating(RatingMatrix *rm, uint32_t uid, uint32_t iid, double rating);
double rec_user_mean(const RatingMatrix *rm, uint32_t uid);
double rec_item_mean(const RatingMatrix *rm, uint32_t iid);
double rec_global_mean(const RatingMatrix *rm);

/* L2: Collaborative Filtering — User-based kNN (Resnick et al. 1994) */
double rec_pearson_similarity(const RatingMatrix *rm, uint32_t u1, uint32_t u2);
double rec_cosine_similarity(const RatingMatrix *rm, uint32_t u1, uint32_t u2);
void rec_user_knn_predict(const RatingMatrix *rm, uint32_t uid, uint32_t iid, uint32_t k, double *pred);

/* L5: Matrix Factorization — SVD (Funk 2006, Netflix Prize) */
void rec_mf_init(MatrixFactorization *mf, uint32_t users, uint32_t items, uint32_t dim);
void rec_mf_train_sgd(MatrixFactorization *mf, const RatingMatrix *rm);
double rec_mf_predict(const MatrixFactorization *mf, uint32_t uid, uint32_t iid);
double rec_mf_rmse(const MatrixFactorization *mf, const RatingMatrix *rm);
void rec_mf_top_k(const MatrixFactorization *mf, uint32_t uid, Recommendation *results, uint32_t k);

/* L6: Content-Based — TF-IDF item similarity */
#define REC_TFIDF_MAX_TERMS 128
typedef struct { char *terms[REC_TFIDF_MAX_TERMS]; uint32_t freqs[REC_TFIDF_MAX_TERMS]; uint32_t count; } TermVector;
void rec_tfidf_build(TermVector *tv, const char *text);
double rec_tfidf_cosine(const TermVector *a, const TermVector *b);

/* L7: A/B Testing framework */
typedef struct { uint32_t control_users, treatment_users; double control_ctr, treatment_ctr; double p_value; } ABTest;
void rec_ab_test_init(ABTest *ab);
void rec_ab_test_record(ABTest *ab, bool is_treatment, bool clicked);
double rec_ab_test_significance(const ABTest *ab);

void rec_print_top_k(const Recommendation *recs, uint32_t k);
void rec_mf_print(const MatrixFactorization *mf);

/* L8: Implicit ALS */
typedef struct { double **X; double **Y; uint32_t num_users, num_items, latent_dim; double alpha, lambda; uint32_t iterations; } ImplicitALS;
void rec_als_init(ImplicitALS *als, uint32_t users, uint32_t items, uint32_t dim, double alpha, double lambda);
void rec_als_train(ImplicitALS *als, const RatingMatrix *rm);
double rec_als_predict(const ImplicitALS *als, uint32_t uid, uint32_t iid);

/* L8: Cold Start - popularity-based */
typedef struct { double popularity_score; uint32_t rating_count; double avg_rating; double recency; } ItemMetadata;
void rec_cold_start_popularity(const RatingMatrix *rm, Recommendation *results, uint32_t k);

/* L7: Evaluation */
double rec_rmse_evaluate(const RatingMatrix *rm, const Recommendation *preds, uint32_t n);
double rec_precision_at_k(const Recommendation *preds, uint32_t k, const RatingMatrix *rm, double threshold);
double rec_dcg_at_k(const Recommendation *preds, uint32_t k, const RatingMatrix *rm);

/* L9: Diversity & Novelty */
double rec_intra_list_diversity(const Recommendation *recs, uint32_t k, const RatingMatrix *rm);
double rec_novelty(const Recommendation *rec, const RatingMatrix *rm);

/* L7: Train/Test Split */
typedef struct { RatingMatrix train; RatingMatrix test; double train_ratio; uint64_t cutoff_timestamp; } RecTrainTestSplit;
void rec_split_temporal(const RatingMatrix *rm, RecTrainTestSplit *split, double train_ratio);
#endif
