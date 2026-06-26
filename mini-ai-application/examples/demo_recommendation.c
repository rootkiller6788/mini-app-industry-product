#include "recommender.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void rec_mf_print(const MatrixFactorization *mf);
void rec_print_top_k(const Recommendation *recs, uint32_t k);
void rec_cold_start_popularity(const RatingMatrix *rm, Recommendation *results, uint32_t k);
double rec_intra_list_diversity(const Recommendation *recs, uint32_t k, const RatingMatrix *rm);
double rec_novelty(const Recommendation *rec, const RatingMatrix *rm);

int main(void) {
    printf("\n============================================================\n");
    printf("  Recommendation System Demo\n");
    printf("============================================================\n\n");

    printf("[1] Building rating matrix (10 users, 15 items)...\n");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 10, 15, 200);
    for (uint32_t u = 0; u < 10; u++) {
        for (uint32_t i = 0; i < 12; i++) {
            double base = 3.0;
            if (u < 3)      base += (double)((i % 5) == 0 ? 2 : 1);
            else if (u < 6) base += (double)((i % 5) == 1 ? 2 : 0);
            else            base += (double)((i % 5) == 2 ? 2 : -1);
            rec_add_rating(&rm, u, i, base + ((double)(rand()%100) - 50.0) / 50.0);
        }
    }
    printf("    %u ratings added, global mean = %.3f\n", rm.rating_count, rec_global_mean(&rm));

    printf("\n[2] User similarity:\n");
    printf("    Pearson(0,1) = %.3f (same cluster, should be high)\n", rec_pearson_similarity(&rm, 0, 1));
    printf("    Pearson(0,7) = %.3f (diff cluster, should be low)\n", rec_pearson_similarity(&rm, 0, 7));
    printf("    Cosine(0,1) = %.3f\n", rec_cosine_similarity(&rm, 0, 1));

    printf("\n[3] kNN prediction for user 0, item 13:\n");
    double knn_pred;
    rec_user_knn_predict(&rm, 0, 13, 5, &knn_pred);
    printf("    Predicted: %.3f\n", knn_pred);

    printf("\n[4] Training Matrix Factorization (SGD, 100 iters)...\n");
    MatrixFactorization mf;
    rec_mf_init(&mf, 10, 15, 8);
    mf.learning_rate = 0.01;
    mf.lambda_reg = 0.02;
    mf.iterations = 100;
    rec_mf_train_sgd(&mf, &rm);
    rec_mf_print(&mf);

    printf("\n[5] Top-5 recommendations for user 0:\n");
    Recommendation top_k[5];
    rec_mf_top_k(&mf, 0, top_k, 5);
    rec_print_top_k(top_k, 5);

    printf("\n[6] Content-based TF-IDF similarity:\n");
    TermVector tv1, tv2;
    rec_tfidf_build(&tv1, "action adventure sci-fi space exploration");
    rec_tfidf_build(&tv2, "action adventure superhero comic book");
    printf("    TF-IDF cosine: %.3f\n", rec_tfidf_cosine(&tv1, &tv2));

    printf("\n[7] A/B Test (1000 users per group):\n");
    ABTest ab;
    rec_ab_test_init(&ab);
    for (int i = 0; i < 1000; i++) rec_ab_test_record(&ab, false, i < 120);
    for (int i = 0; i < 1000; i++) rec_ab_test_record(&ab, true, i < 150);
    printf("    Control CTR: %.3f, Treatment CTR: %.3f\n",
           ab.control_ctr / ab.control_users, ab.treatment_ctr / ab.treatment_users);
    printf("    p-value: %.4f\n", rec_ab_test_significance(&ab));

    printf("\n[8] Evaluation metrics:\n");
    printf("    RMSE: %.4f\n", rec_mf_rmse(&mf, &rm));
    printf("    Intra-list diversity: %.3f\n", rec_intra_list_diversity(top_k, 5, &rm));
    printf("    Novelty of top item: %.3f\n", rec_novelty(&top_k[0], &rm));

    printf("\n============================================================\n");
    printf("  Demo complete!\n");
    printf("============================================================\n");
    return 0;
}
