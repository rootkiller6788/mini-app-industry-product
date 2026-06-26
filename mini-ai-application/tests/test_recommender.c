#include "recommender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int run = 0, pass = 0;
#define TEST(nm) do { run++; printf("  TEST: %s ... ", nm); } while(0)
#define PASS do { puts("PASS"); pass++; } while(0)

static void t_init(void) {
    TEST("rating_matrix_init");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 10, 20, 100);
    assert(rm.user_count == 10);
    assert(rm.item_count == 20);
    assert(rm.user_item_matrix != NULL);
    PASS;
}

static void t_add_rating(void) {
    TEST("add_rating");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 10, 20, 5);
    rec_add_rating(&rm, 0, 0, 4.5);
    rec_add_rating(&rm, 0, 1, 3.0);
    assert(rm.rating_count == 2);
    assert(fabs(rm.user_item_matrix[0][0] - 4.5) < 1e-9);
    PASS;
}

static void t_user_mean(void) {
    TEST("user_mean / global_mean");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 3, 3, 6);
    rec_add_rating(&rm, 0, 0, 5.0);
    rec_add_rating(&rm, 0, 1, 3.0);
    rec_add_rating(&rm, 1, 0, 4.0);
    assert(fabs(rec_user_mean(&rm, 0) - 4.0) < 1e-9);
    assert(rec_global_mean(&rm) > 0.0);
    PASS;
}

static void t_pearson(void) {
    TEST("pearson_similarity");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 3, 3, 9);
    rec_add_rating(&rm, 0, 0, 5.0); rec_add_rating(&rm, 0, 1, 3.0); rec_add_rating(&rm, 0, 2, 4.0);
    rec_add_rating(&rm, 1, 0, 4.0); rec_add_rating(&rm, 1, 1, 2.0); rec_add_rating(&rm, 1, 2, 3.0);
    double sim = rec_pearson_similarity(&rm, 0, 1);
    assert(sim >= -1.0 && sim <= 1.0);
    PASS;
}

static void t_cosine(void) {
    TEST("cosine_similarity");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 3, 3, 6);
    rec_add_rating(&rm, 0, 0, 5.0); rec_add_rating(&rm, 0, 1, 3.0);
    rec_add_rating(&rm, 1, 0, 4.0); rec_add_rating(&rm, 1, 1, 2.0);
    double sim = rec_cosine_similarity(&rm, 0, 1);
    assert(sim >= 0.0 && sim <= 1.0);
    PASS;
}

static void t_knn_predict(void) {
    TEST("user_knn_predict");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 3, 3, 9);
    rec_add_rating(&rm, 0, 0, 5.0); rec_add_rating(&rm, 0, 1, 3.0);
    rec_add_rating(&rm, 1, 0, 4.0); rec_add_rating(&rm, 1, 1, 4.0); rec_add_rating(&rm, 1, 2, 4.0);
    double pred;
    rec_user_knn_predict(&rm, 0, 2, 2, &pred);
    assert(pred > 0.0);
    PASS;
}

static void t_mf(void) {
    TEST("MF train/predict");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 5, 5, 20);
    for (uint32_t u = 0; u < 5; u++)
        for (uint32_t i = 0; i < 4; i++)
            rec_add_rating(&rm, u, i, 1.0 + (double)((u+i)%5));
    MatrixFactorization mf;
    rec_mf_init(&mf, 5, 5, 8);
    assert(mf.num_users == 5);
    rec_mf_train_sgd(&mf, &rm);
    assert(mf.rmse >= 0.0);
    double pred = rec_mf_predict(&mf, 0, 0);
    (void)pred; /* MF prediction with random init */
    Recommendation recs[4];
    rec_mf_top_k(&mf, 0, recs, 4);
    assert(recs[0].item_id < 4);
    PASS;
}

static void t_tfidf(void) {
    TEST("tfidf_cosine");
    TermVector a, b;
    rec_tfidf_build(&a, "machine learning deep neural");
    rec_tfidf_build(&b, "deep learning neural AI");
    double sim = rec_tfidf_cosine(&a, &b);
    assert(sim >= 0.0 && sim <= 1.0);
    PASS;
}

static void t_ab_test(void) {
    TEST("AB test");
    ABTest ab; rec_ab_test_init(&ab);
    for (int i = 0; i < 100; i++) rec_ab_test_record(&ab, false, i < 10);
    for (int i = 0; i < 100; i++) rec_ab_test_record(&ab, true, i < 15);
    assert(ab.control_users == 100);
    double p = rec_ab_test_significance(&ab);
    assert(p >= 0.0 && p <= 1.0);
    PASS;
}

static void t_cold_start(void) {
    TEST("cold_start");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 5, 10, 50);
    for (uint32_t i = 0; i < 8; i++) {
        rec_add_rating(&rm, 0, i, (double)(i%5) + 1.0);
        rec_add_rating(&rm, 1, i, (double)(i%5) + 1.0);
    }
    Recommendation recs[5];
    rec_cold_start_popularity(&rm, recs, 5);
    assert(recs[0].predicted_rating > 0.0);
    PASS;
}

static void t_als(void) {
    TEST("ALS implicit");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 4, 4, 16);
    for (uint32_t u = 0; u < 4; u++)
        for (uint32_t i = 0; i < 4; i++)
            rec_add_rating(&rm, u, i, u == i ? 1.0 : 0.0);
    ImplicitALS als;
    rec_als_init(&als, 4, 4, 4, 40.0, 0.01);
    rec_als_train(&als, &rm);
    double pred = rec_als_predict(&als, 0, 0);
    (void)pred; /* MF prediction with random init */
    PASS;
}

static void t_eval(void) {
    TEST("RMSE / Precision / DCG");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 3, 3, 9);
    for (uint32_t i = 0; i < 3; i++)
        for (uint32_t j = 0; j < 3; j++)
            rec_add_rating(&rm, i, j, 3.0 + (double)((i+j)%3));
    Recommendation preds[3];
    preds[0].user_id = 0; preds[0].item_id = 0; preds[0].predicted_rating = 3.5;
    preds[1].user_id = 1; preds[1].item_id = 1; preds[1].predicted_rating = 4.0;
    preds[2].user_id = 2; preds[2].item_id = 2; preds[2].predicted_rating = 5.0;
    assert(rec_rmse_evaluate(&rm, preds, 3) >= 0.0);
    assert(rec_precision_at_k(preds, 2, &rm, 3.0) >= 0.0);
    assert(rec_dcg_at_k(preds, 3, &rm) > 0.0);
    PASS;
}

static void t_diversity(void) {
    TEST("diversity");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 5, 5, 25);
    for (uint32_t u = 0; u < 5; u++)
        for (uint32_t i = 0; i < 5; i++)
            rec_add_rating(&rm, u, i, (double)((u+i)%5) + 1.0);
    Recommendation recs[3] = {{0,0,5.0},{0,1,4.0},{0,2,3.0}};
    double ild = rec_intra_list_diversity(recs, 3, &rm);
    assert(ild >= 0.0 && ild <= 1.0);
    PASS;
}

static void t_novelty(void) {
    TEST("novelty");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 5, 5, 25);
    rec_add_rating(&rm, 0, 0, 5.0); rec_add_rating(&rm, 1, 0, 4.0);
    Recommendation rec = {0, 1, 4.0};
    assert(rec_novelty(&rec, &rm) > 0.0);
    PASS;
}

static void t_split(void) {
    TEST("train/test split");
    RatingMatrix rm;
    rec_rating_matrix_init(&rm, 5, 5, 20);
    for (uint32_t i = 0; i < 10; i++)
        rec_add_rating(&rm, i%5, i%5, (double)(i%5)+1.0);
    RecTrainTestSplit split;
    rec_split_temporal(&rm, &split, 0.8);
    assert(split.train.rating_count > 0);
    PASS;
}

static void t_null(void) {
    TEST("null safety");
    rec_rating_matrix_init(NULL, 0, 0, 0);
    rec_add_rating(NULL, 0, 0, 0);
    rec_user_mean(NULL, 0);
    rec_global_mean(NULL);
    rec_pearson_similarity(NULL, 0, 0);
    rec_mf_init(NULL, 0, 0, 0);
    rec_mf_train_sgd(NULL, NULL);
    rec_mf_predict(NULL, 0, 0);
    rec_tfidf_build(NULL, NULL);
    rec_ab_test_init(NULL);
    rec_rmse_evaluate(NULL, NULL, 0);
    rec_als_init(NULL, 0, 0, 0, 0, 0);
    PASS;
}

int main(void) {
    puts("\n=== Recommender Tests ===");
    puts("");
    t_init(); t_add_rating(); t_user_mean();
    t_pearson(); t_cosine(); t_knn_predict();
    t_mf(); t_tfidf(); t_ab_test(); t_cold_start();
    t_als(); t_eval(); t_diversity(); t_novelty();
    t_split(); t_null();
    printf("\n=== Results: %d/%d tests passed ===\n", pass, run);
    return (pass == run) ? 0 : 1;
}
