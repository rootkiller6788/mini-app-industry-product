#include "recommender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void rec_rating_matrix_init(RatingMatrix *rm, uint32_t users, uint32_t items, uint32_t max_ratings) {
    if(!rm)return;memset(rm,0,sizeof(*rm));rm->user_count=users;rm->item_count=items;rm->ratings=(RatingRecord*)calloc(max_ratings,sizeof(RatingRecord));rm->user_item_matrix=(double**)malloc(users*sizeof(double*));for(uint32_t u=0;u<users;u++)rm->user_item_matrix[u]=(double*)calloc(items,sizeof(double));rm->user_bias=(double*)calloc(users,sizeof(double));rm->item_bias=(double*)calloc(items,sizeof(double));
}

void rec_add_rating(RatingMatrix *rm, uint32_t uid, uint32_t iid, double rating) {
    if(!rm||uid>=rm->user_count||iid>=rm->item_count)return;RatingRecord*r=&rm->ratings[rm->rating_count];r->user_id=uid;r->item_id=iid;r->rating=rating;r->timestamp=(uint64_t)time(NULL);rm->rating_count++;rm->user_item_matrix[uid][iid]=rating;
}

double rec_user_mean(const RatingMatrix *rm, uint32_t uid) {
    if(!rm||uid>=rm->user_count)return 0.0;double s=0;uint32_t c=0;for(uint32_t i=0;i<rm->item_count;i++)if(rm->user_item_matrix[uid][i]>0){s+=rm->user_item_matrix[uid][i];c++;}return c>0?s/(double)c:0;
}

double rec_item_mean(const RatingMatrix *rm, uint32_t iid) {
    if(!rm||iid>=rm->item_count)return 0;double s=0;uint32_t c=0;for(uint32_t u=0;u<rm->user_count;u++)if(rm->user_item_matrix[u][iid]>0){s+=rm->user_item_matrix[u][iid];c++;}return c>0?s/(double)c:0;
}

double rec_global_mean(const RatingMatrix *rm) {
    if (!rm || rm->rating_count == 0) return 0.0;
    double sum = 0.0;
    for (uint32_t r = 0; r < rm->rating_count; r++) sum += rm->ratings[r].rating;
    return sum / (double)rm->rating_count;
}

/* L2: Pearson correlation coefficient — measures linear relationship
 * r_xy = cov(X,Y) / (sd(X) * sd(Y)), range [-1, 1] */
double rec_pearson_similarity(const RatingMatrix *rm, uint32_t u1, uint32_t u2) {
    if (!rm || u1 >= rm->user_count || u2 >= rm->user_count) return 0.0;
    double s1=0, s2=0, s1q=0, s2q=0, ps=0; uint32_t n=0;
    for (uint32_t i=0; i<rm->item_count; i++) {
        double r1=rm->user_item_matrix[u1][i], r2=rm->user_item_matrix[u2][i];
        if (r1>0 && r2>0) { s1+=r1; s2+=r2; s1q+=r1*r1; s2q+=r2*r2; ps+=r1*r2; n++; }
    }
    if (n==0) return 0.0;
    double num = ps - (s1*s2)/(double)n;
    double den = sqrt((s1q - s1*s1/(double)n) * (s2q - s2*s2/(double)n));
    return (den > 1e-9) ? num/den : 0.0;
}

/* L2: Cosine similarity — angle between rating vectors, range [0,1] */
double rec_cosine_similarity(const RatingMatrix *rm, uint32_t u1, uint32_t u2) {
    if (!rm || u1>=rm->user_count || u2>=rm->user_count) return 0.0;
    double dot=0, n1=0, n2=0;
    for (uint32_t i=0; i<rm->item_count; i++) {
        double r1=rm->user_item_matrix[u1][i], r2=rm->user_item_matrix[u2][i];
        dot+=r1*r2; n1+=r1*r1; n2+=r2*r2;
    }
    if (n1<1e-9 || n2<1e-9) return 0.0;
    return dot/(sqrt(n1)*sqrt(n2));
}

/* L2: User-KNN prediction — weighted average of k nearest neighbors
 * pred(u,i) = mean(u) + sum(sim(u,v)*(r(v,i)-mean(v)))/sum(sim(u,v)) */
void rec_user_knn_predict(const RatingMatrix *rm, uint32_t uid, uint32_t iid,
                           uint32_t k, double *pred) {
    if (!rm || !pred) return;
    *pred = rec_global_mean(rm);
    typedef struct { uint32_t uid; double sim; } Nb;
    Nb nbs[64]; uint32_t nc=0;
    for (uint32_t u=0; u<rm->user_count && nc<64; u++) {
        if (u==uid || rm->user_item_matrix[u][iid]<=0) continue;
        double sim = rec_pearson_similarity(rm, uid, u);
        if (sim>0) {
            uint32_t p=nc;
            while (p>0 && nbs[p-1].sim<sim) { nbs[p]=nbs[p-1]; p--; }
            nbs[p].uid=u; nbs[p].sim=sim; if (nc<64) nc++;
        }
    }
    uint32_t tk = (k<nc) ? k : nc;
    if (tk==0) return;
    double ws=0, wr=0;
    for (uint32_t i=0; i<tk; i++) {
        double mu = rec_user_mean(rm, nbs[i].uid);
        ws += nbs[i].sim;
        wr += nbs[i].sim * (rm->user_item_matrix[nbs[i].uid][iid] - mu);
    }
    *pred = rec_user_mean(rm, uid) + (ws>0 ? wr/ws : 0.0);
}

/* L5: Matrix Factorization via SGD (Funk SVD, Netflix Prize 2006)
 * Objective: min sum (r_ui - mu - b_u - b_i - p_u·q_i)^2 + lambda*regularization
 * Gradient descent on user factors p_u, item factors q_i, biases b_u, b_i */
void rec_mf_init(MatrixFactorization *mf, uint32_t users, uint32_t items,
                  uint32_t dim) {
    if (!mf) return;
    memset(mf, 0, sizeof(*mf));
    mf->num_users=users; mf->num_items=items; mf->latent_dim=dim;
    mf->learning_rate=0.005; mf->lambda_reg=0.02; mf->iterations=50;
    float lim = sqrtf(6.0f/(float)dim);
    mf->user_emb = (Embedding *)calloc(users, sizeof(Embedding));
    mf->item_emb = (Embedding *)calloc(items, sizeof(Embedding));
    for (uint32_t u=0; u<users; u++) {
        mf->user_emb[u].dim=dim;
        mf->user_emb[u].weights=(float*)malloc(dim*sizeof(float));
        for (uint32_t d=0; d<dim; d++)
            mf->user_emb[u].weights[d]=((float)rand()/RAND_MAX*2.0f-1.0f)*lim;
    }
    for (uint32_t i=0; i<items; i++) {
        mf->item_emb[i].dim=dim;
        mf->item_emb[i].weights=(float*)malloc(dim*sizeof(float));
        for (uint32_t d=0; d<dim; d++)
            mf->item_emb[i].weights[d]=((float)rand()/RAND_MAX*2.0f-1.0f)*lim;
    }
}

void rec_mf_train_sgd(MatrixFactorization *mf, const RatingMatrix *rm) {
    if (!mf || !rm) return;
    double gb=rec_global_mean(rm);
    double *ub=(double*)calloc(mf->num_users, sizeof(double));
    double *ib=(double*)calloc(mf->num_items, sizeof(double));
    for (uint64_t iter=0; iter<mf->iterations; iter++) {
        double te=0.0;
        for (uint32_t r=0; r<rm->rating_count; r++) {
            RatingRecord *rr=&rm->ratings[r];
            uint32_t u=rr->user_id, i=rr->item_id;
            if (u>=mf->num_users || i>=mf->num_items) continue;
            double dot=0.0;
            for (uint32_t d=0; d<mf->latent_dim; d++)
                dot+=(double)mf->user_emb[u].weights[d]*(double)mf->item_emb[i].weights[d];
            double pred=gb+ub[u]+ib[i]+dot;
            double err=rr->rating-pred; te+=err*err;
            double lr=mf->learning_rate, reg=mf->lambda_reg;
            ub[u]+=lr*(err-reg*ub[u]); ib[i]+=lr*(err-reg*ib[i]);
            for (uint32_t d=0; d<mf->latent_dim; d++) {
                float uf=mf->user_emb[u].weights[d], vf=mf->item_emb[i].weights[d];
                mf->user_emb[u].weights[d]+=(float)(lr*(err*(double)vf-reg*(double)uf));
                mf->item_emb[i].weights[d]+=(float)(lr*(err*(double)uf-reg*(double)vf));
            }
        }
        mf->rmse=sqrt(te/(double)rm->rating_count);
    }
    free(ub); free(ib);
}

double rec_mf_predict(const MatrixFactorization *mf, uint32_t uid, uint32_t iid) {
    if (!mf || uid>=mf->num_users || iid>=mf->num_items) return 0.0;
    double dot=0.0;
    for (uint32_t d=0; d<mf->latent_dim; d++)
        dot+=(double)mf->user_emb[uid].weights[d]*(double)mf->item_emb[iid].weights[d];
    return dot;
}

double rec_mf_rmse(const MatrixFactorization *mf, const RatingMatrix *rm) {
    if (!mf || !rm || rm->rating_count==0) return 0.0;
    double sse=0.0;
    for (uint32_t r=0; r<rm->rating_count; r++) {
        double pred=rec_mf_predict(mf, rm->ratings[r].user_id, rm->ratings[r].item_id);
        double err=rm->ratings[r].rating-pred; sse+=err*err;
    }
    return sqrt(sse/(double)rm->rating_count);
}

void rec_mf_top_k(const MatrixFactorization *mf, uint32_t uid,
                   Recommendation *results, uint32_t k) {
    if (!mf || !results) return;
    typedef struct { uint32_t iid; double score; } SI;
    SI *scores=(SI*)calloc(mf->num_items, sizeof(SI));
    uint32_t sc=0;
    for (uint32_t i=0; i<mf->num_items && sc<mf->num_items; i++) {
        double pred=rec_mf_predict(mf, uid, i);
        uint32_t pos=sc;
        while (pos>0 && scores[pos-1].score<pred) {
            scores[pos]=scores[pos-1]; pos--;
        }
        scores[pos].iid=i; scores[pos].score=pred;
        if (sc<mf->num_items) sc++;
    }
    uint32_t top=(k<sc) ? k : sc;
    for (uint32_t i=0; i<top; i++) {
        results[i].user_id=uid; results[i].item_id=scores[i].iid;
        results[i].predicted_rating=scores[i].score;
    }
    free(scores);
}

void rec_mf_print(const MatrixFactorization *mf) {
    if (!mf) return;
    printf("MF: %u users x %u items x %u latent | RMSE=%.4f | lr=%.4f reg=%.2f\n",
           mf->num_users, mf->num_items, mf->latent_dim,
           mf->rmse, mf->learning_rate, mf->lambda_reg);
}

/* L6: Content-Based Filtering — TF-IDF term vector similarity */
void rec_tfidf_build(TermVector *tv, const char *text) {
    if (!tv || !text) return;
    memset(tv, 0, sizeof(*tv));
    char w[1024]; strncpy(w, text, 1023); w[1023]='\0';
    char *wd=strtok(w, " ,.!?");
    while (wd && tv->count < REC_TFIDF_MAX_TERMS) {
        bool f=false;
        for (uint32_t i=0; i<tv->count; i++) {
            if (strcmp(tv->terms[i], wd)==0) { tv->freqs[i]++; f=true; break; }
        }
        if (!f) {
            tv->terms[tv->count]=strdup(wd);
            tv->freqs[tv->count]=1; tv->count++;
        }
        wd=strtok(NULL, " ,.!?");
    }
}

double rec_tfidf_cosine(const TermVector *a, const TermVector *b) {
    if (!a || !b) return 0.0;
    double dot=0.0, na=0.0, nb=0.0;
    for (uint32_t i=0; i<a->count; i++) {
        double ta=(double)a->freqs[i]; na+=ta*ta;
        for (uint32_t j=0; j<b->count; j++)
            if (strcmp(a->terms[i], b->terms[j])==0) dot+=ta*(double)b->freqs[j];
    }
    for (uint32_t j=0; j<b->count; j++) {
        double tb=(double)b->freqs[j]; nb+=tb*tb;
    }
    return (na<1e-9 || nb<1e-9) ? 0.0 : dot/(sqrt(na)*sqrt(nb));
}

/* L7: A/B Testing — two-proportion z-test for statistical significance
 * H0: p1=p2, H1: p1!=p2
 * z = (p1-p2)/sqrt(pp*(1-pp)*(1/n1+1/n2))
 * p-value = 2*Phi(-|z|) via standard normal CDF approximation */
void rec_ab_test_init(ABTest *ab) {
    if (ab) memset(ab, 0, sizeof(*ab));
}

void rec_ab_test_record(ABTest *ab, bool is_treatment, bool clicked) {
    if (!ab) return;
    if (is_treatment) {
        ab->treatment_users++;
        if (clicked) ab->treatment_ctr += 1.0;
    } else {
        ab->control_users++;
        if (clicked) ab->control_ctr += 1.0;
    }
}

double rec_ab_test_significance(const ABTest *ab) {
    if (!ab || ab->control_users==0 || ab->treatment_users==0) return 1.0;
    double p1=ab->control_ctr/(double)ab->control_users;
    double p2=ab->treatment_ctr/(double)ab->treatment_users;
    double pp=(ab->control_ctr+ab->treatment_ctr)/
              (double)(ab->control_users+ab->treatment_users);
    double se=sqrt(pp*(1.0-pp)*
                   (1.0/ab->control_users + 1.0/ab->treatment_users));
    if (se<1e-9) return 1.0;
    double z=fabs(p2-p1)/se;
    /* Abramowitz & Stegun 26.2.17 — standard normal CDF approximation */
    double x=z, t=1.0/(1.0+0.2316419*x);
    double phi=exp(-x*x/2.0)/sqrt(2.0*M_PI);
    double cdf=1.0-phi*(0.319381530*t - 0.356563782*t*t
                + 1.781477937*t*t*t - 1.821255978*t*t*t*t
                + 1.330274429*t*t*t*t*t);
    return 2.0*(1.0-cdf);
}

void rec_print_top_k(const Recommendation *recs, uint32_t k) {
    if (!recs) return;
    printf("Top-%u Recommendations:\n", k);
    for (uint32_t i=0; i<k; i++)
        printf("  [%u] U%u -> I%u | Score: %.4f\n",
               i+1, recs[i].user_id, recs[i].item_id, recs[i].predicted_rating);
}

/* L7: Evaluation Metrics for Recommendation Systems
 *
 * Key offline metrics:
 * RMSE: sqrt(mean((r_ui - r_hat_ui)^2)) — for explicit feedback
 * MAE: mean(|r_ui - r_hat_ui|)
 * Precision@K: |relevant items in top-K| / K
 * Recall@K: |relevant items in top-K| / |all relevant items|
 * NDCG@K: DCG/IDCG, discounts by position (log2(i+1))
 * MAP: Mean Average Precision across users
 *
 * L4: Coverage vs. Accuracy trade-off
 * Popularity bias: CF methods favor popular items, reducing
 * catalog coverage and serendipity. Mitigation: inverse propensity
 * scoring (IPS), diversity regularization.
 *
 * Reference: Herlocker et al. 2004 "Evaluating Collaborative
 * Filtering Recommender Systems" (seminal GroupLens paper) */

double rec_rmse_evaluate(const RatingMatrix *rm, const Recommendation *preds, uint32_t n) {
    if (!rm || !preds || n == 0) return 0.0;
    double sse = 0.0; uint32_t cnt = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t u = preds[i].user_id, it = preds[i].item_id;
        if (u < rm->user_count && it < rm->item_count && rm->user_item_matrix[u][it] > 0.0) {
            double err = rm->user_item_matrix[u][it] - preds[i].predicted_rating;
            sse += err * err; cnt++;
        }
    }
    return cnt > 0 ? sqrt(sse / (double)cnt) : 0.0;
}

double rec_precision_at_k(const Recommendation *preds, uint32_t k, const RatingMatrix *rm, double threshold) {
    if (!preds || !rm || k == 0) return 0.0;
    uint32_t relevant = 0;
    for (uint32_t i = 0; i < k; i++) {
        uint32_t u = preds[i].user_id, it = preds[i].item_id;
        if (u < rm->user_count && it < rm->item_count && rm->user_item_matrix[u][it] >= threshold)
            relevant++;
    }
    return (double)relevant / (double)k;
}

double rec_dcg_at_k(const Recommendation *preds, uint32_t k, const RatingMatrix *rm) {
    if (!preds || !rm || k == 0) return 0.0;
    double dcg = 0.0;
    for (uint32_t i = 0; i < k; i++) {
        uint32_t u = preds[i].user_id, it = preds[i].item_id;
        double rel = (u < rm->user_count && it < rm->item_count) ? rm->user_item_matrix[u][it] : 0.0;
        dcg += (pow(2.0, rel) - 1.0) / log2((double)(i + 2));
    }
    return dcg;
}

/* L8: Cold Start Handling Strategies
 *
 * New user problem: no rating history for collaborative filtering.
 * New item problem: no ratings to compute item-item similarity.
 *
 * Solutions:
 * 1. Content-based: use item/user metadata features
 * 2. Demographic: use age, location, gender as proxy
 * 3. Popularity-based: recommend trending items first
 * 4. Interview-based: ask user to rate seed items (onboarding)
 * 5. Active learning: select most informative items to rate
 * 6. Cross-domain: transfer learned preferences from auxiliary domain
 *
 * L4: Exploration-Exploitation trade-off in cold start —
 * Multi-armed bandit (epsilon-greedy, Thompson sampling, UCB)
 * balances recommending known good items vs exploring new items.
 *
 * Reference: Schein et al. 2002 "Methods and Metrics for Cold-Start
 * Recommendations" (Penn)
 * Li et al. 2010 "A Contextual-Bandit Approach to Personalized
 * News Article Recommendation" (Yahoo! Today Module) */

void rec_cold_start_popularity(const RatingMatrix *rm, Recommendation *results, uint32_t k) {
    if (!rm || !results) return;
    ItemMetadata *items = (ItemMetadata *)calloc(rm->item_count, sizeof(ItemMetadata));
    /* Compute popularity per item */
    for (uint32_t r = 0; r < rm->rating_count; r++) {
        uint32_t it = rm->ratings[r].item_id;
        if (it < rm->item_count) {
            items[it].rating_count++;
            items[it].avg_rating += rm->ratings[r].rating;
        }
    }
    for (uint32_t i = 0; i < rm->item_count; i++) {
        if (items[i].rating_count > 0) {
            items[i].avg_rating /= (double)items[i].rating_count;
            /* Bayesian average: weight by prior (global mean + min_ratings) */
            double global = rec_global_mean(rm);
            uint32_t min_ratings = 5;
            items[i].popularity_score = ((double)items[i].rating_count / (double)(items[i].rating_count + min_ratings))
                * items[i].avg_rating + ((double)min_ratings / (double)(items[i].rating_count + min_ratings)) * global;
        }
    }
    /* Select top-k by popularity */
    for (uint32_t i = 0; i < k && i < rm->item_count; i++) {
        uint32_t best = 0; double best_score = -1.0;
        for (uint32_t j = 0; j < rm->item_count; j++) {
            /* Avoid duplicates */
            bool already = false;
            for (uint32_t p = 0; p < i; p++)
                if (results[p].item_id == j) { already = true; break; }
            if (!already && items[j].popularity_score > best_score) {
                best_score = items[j].popularity_score; best = j;
            }
        }
        results[i].user_id = 0; results[i].item_id = best;
        results[i].predicted_rating = items[best].popularity_score;
    }
    free(items);
}

/* L8: Implicit Feedback — Alternating Least Squares (ALS)
 *
 * Implicit feedback (clicks, views, purchases) is abundant but noisy.
 * ALS-WR (Hu, Koren, Volinsky 2008) formulation:
 *
 * Confidence: c_ui = 1 + alpha * r_ui (alpha ~ 40 for binary feedback)
 * Preference: p_ui = 1 if r_ui > 0, else 0
 * Objective: min Sigma c_ui(p_ui - x_u^T y_i)^2 + lambda(||x_u||^2 + ||y_i||^2)
 *
 * ALS optimization: alternate between fixing X (user factors) and
 * solving for Y (item factors), and vice versa. Each step is a
 * least-squares problem with closed-form solution.
 *
 * L4: ALS converges monotonically (each step reduces objective).
 * Time: O(users * items * latent_dim) per iteration (dense case)
 * Time: O(ratings * latent_dim^2 + (users+items)*latent_dim^3) (sparse)
 *
 * Reference: Hu, Koren, Volinsky 2008 "Collaborative Filtering
 * for Implicit Feedback Datasets" (Yahoo! Research) */

void rec_als_init(ImplicitALS *als, uint32_t users, uint32_t items, uint32_t dim, double alpha, double lambda) {
    if (!als) return;
    memset(als, 0, sizeof(*als));
    als->num_users = users; als->num_items = items; als->latent_dim = dim;
    als->alpha = alpha; als->lambda = lambda; als->iterations = 15;
    als->X = (double **)malloc(users * sizeof(double *));
    als->Y = (double **)malloc(items * sizeof(double *));
    for (uint32_t u = 0; u < users; u++) {
        als->X[u] = (double *)calloc(dim, sizeof(double));
        for (uint32_t d = 0; d < dim; d++) als->X[u][d] = (double)rand() / (double)RAND_MAX * 0.01;
    }
    for (uint32_t i = 0; i < items; i++) {
        als->Y[i] = (double *)calloc(dim, sizeof(double));
        for (uint32_t d = 0; d < dim; d++) als->Y[i][d] = (double)rand() / (double)RAND_MAX * 0.01;
    }
}

void rec_als_train(ImplicitALS *als, const RatingMatrix *rm) {
    if (!als || !rm) return;
    double *YtY = (double *)calloc((size_t)als->latent_dim * als->latent_dim, sizeof(double));
    for (uint32_t iter = 0; iter < als->iterations; iter++) {
        /* Fix Y, solve for X_u = (Y^T C^u Y + lambda I)^{-1} Y^T C^u p(u)
         * Simplified: gradient descent approximation */
        for (uint32_t u = 0; u < als->num_users; u++) {
            for (uint32_t d = 0; d < als->latent_dim; d++) {
                double grad = 0.0;
                for (uint32_t i = 0; i < als->num_items && i < rm->item_count; i++) {
                    if (rm->user_item_matrix[u][i] > 0.0) {
                        double c_ui = 1.0 + als->alpha * rm->user_item_matrix[u][i];
                        double pred = 0.0;
                        for (uint32_t k = 0; k < als->latent_dim; k++) pred += als->X[u][k] * als->Y[i][k];
                        grad += c_ui * (1.0 - pred) * als->Y[i][d];
                    }
                }
                als->X[u][d] += 0.01 * (grad - als->lambda * als->X[u][d]);
            }
        }
        /* Fix X, solve for Y_i similarly */
        for (uint32_t i = 0; i < als->num_items && i < rm->item_count; i++) {
            for (uint32_t d = 0; d < als->latent_dim; d++) {
                double grad = 0.0;
                for (uint32_t u = 0; u < als->num_users; u++) {
                    if (rm->user_item_matrix[u][i] > 0.0) {
                        double c_ui = 1.0 + als->alpha * rm->user_item_matrix[u][i];
                        double pred = 0.0;
                        for (uint32_t k = 0; k < als->latent_dim; k++) pred += als->X[u][k] * als->Y[i][k];
                        grad += c_ui * (1.0 - pred) * als->X[u][d];
                    }
                }
                als->Y[i][d] += 0.01 * (grad - als->lambda * als->Y[i][d]);
            }
        }
    }
    free(YtY);
}

double rec_als_predict(const ImplicitALS *als, uint32_t uid, uint32_t iid) {
    if (!als || uid >= als->num_users || iid >= als->num_items) return 0.0;
    double dot = 0.0;
    for (uint32_t d = 0; d < als->latent_dim; d++) dot += als->X[uid][d] * als->Y[iid][d];
    return dot;
}

/* L9: Diversity and Novelty in Recommendations
 * Beyond accuracy: diverse recommendations increase user satisfaction.
 * Intra-list diversity (ILD): average pairwise distance in top-K list.
 * Novelty: -log2(popularity(item)) — penalize obvious recommendations.
 * Serendipity: unexpected + useful (hardest to measure). */

double rec_intra_list_diversity(const Recommendation *recs, uint32_t k, const RatingMatrix *rm) {
    if (!recs || !rm || k < 2) return 0.0;
    double total_dist = 0.0; uint32_t pairs = 0;
    for (uint32_t i = 0; i < k; i++) {
        for (uint32_t j = i + 1; j < k; j++) {
            /* Item dissimilarity = 1 - cosine of item rating vectors */
            double dot = 0.0, ni = 0.0, nj = 0.0;
            for (uint32_t u = 0; u < rm->user_count; u++) {
                double ri = (recs[i].item_id < rm->item_count) ? rm->user_item_matrix[u][recs[i].item_id] : 0.0;
                double rj = (recs[j].item_id < rm->item_count) ? rm->user_item_matrix[u][recs[j].item_id] : 0.0;
                dot += ri * rj; ni += ri * ri; nj += rj * rj;
            }
            double sim = (ni > 1e-9 && nj > 1e-9) ? dot / sqrt(ni * nj) : 0.0;
            total_dist += 1.0 - sim; pairs++;
        }
    }
    return pairs > 0 ? total_dist / (double)pairs : 0.0;
}

double rec_novelty(const Recommendation *rec, const RatingMatrix *rm) {
    if (!rec || !rm) return 0.0;
    /* Self-information: -log2(p(item))
     * Higher = more novel (less popular / more surprising) */
    uint32_t total = rm->rating_count;
    uint32_t item_ratings = 0;
    for (uint32_t r = 0; r < rm->rating_count; r++)
        if (rm->ratings[r].item_id == rec->item_id) item_ratings++;
    if (item_ratings == 0 || total == 0) return 10.0;
    double p = (double)item_ratings / (double)total;
    return -log2(p);
}

/* L7: Train/Test Split for Recommendation Datasets
 * Stratified temporal split preserves per-user rating distribution.
 * Common ratios: 80/20 (small), 90/10 (large), leave-last-out (production).
 *
 * Temporal split (recommended for production):
 * Train: all ratings before cutoff_time
 * Test: all ratings after cutoff_time
 * Prevents future information leakage. */

void rec_split_temporal(const RatingMatrix *rm, RecTrainTestSplit *split, double train_ratio)
{
    if (!rm || !split) return;
    memset(split, 0, sizeof(*split));
    split->train_ratio = train_ratio;

    /* Sort ratings by timestamp (simple: use cutoff based on count) */
    uint32_t train_count = (uint32_t)((double)rm->rating_count * train_ratio);
    split->cutoff_timestamp = (train_count < rm->rating_count) ?
        rm->ratings[train_count].timestamp : rm->ratings[rm->rating_count - 1].timestamp;

    /* Initialize split matrices */
    rec_rating_matrix_init(&split->train, rm->user_count, rm->item_count, train_count);
    rec_rating_matrix_init(&split->test, rm->user_count, rm->item_count, rm->rating_count - train_count);

    for (uint32_t r = 0; r < rm->rating_count; r++) {
        RatingRecord *rr = &rm->ratings[r];
        if (r < train_count)
            rec_add_rating(&split->train, rr->user_id, rr->item_id, rr->rating);
        else
            rec_add_rating(&split->test, rr->user_id, rr->item_id, rr->rating);
    }
}

/* L9: Two-Tower Neural Collaborative Filtering (Conceptual Model)
 *
 * YouTube DNN (Covington et al. 2016):
 * - Candidate generation: retrieve hundreds from millions
 * - Ranking: score and order top candidates
 *
 * Two-tower architecture (Yi et al. 2019):
 * - User tower: user features → FC layers → embedding u ∈ R^d
 * - Item tower: item features → FC layers → embedding v ∈ R^d
 * - Prediction: σ(u^T v) for click probability
 * - Training: sampled softmax with in-batch negatives
 * - Serving: ANN (FAISS/ScaNN) for top-K retrieval
 *
 * L4: Annoy (Spotify) — random projection trees for approximate NN.
 * FAISS (Facebook) — product quantization + IVF for billion-scale.
 * ScaNN (Google) — anisotropic vector quantization, SOTA on ann-benchmarks.
 *
 * Key trade-offs:
 * - Recall vs. QPS: higher recall needs more probes
 * - Memory vs. accuracy: lower-bit quantization saves RAM
 * - Build time vs. query time: offline index construction
 *
 * Stanford CS 246 / CMU 10-605: Mining Massive Datasets
 *
 * Reference: Covington et al. 2016 "Deep Neural Networks for YouTube
 * Recommendations" (Google)
 * Yi et al. 2019 "Sampling-Bias-Corrected Neural Modeling for
 * Large Corpus Item Recommendations" (Google) */
