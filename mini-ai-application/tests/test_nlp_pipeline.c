#include "nlp_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

void nlp_bigram_init(BigramLM *lm, uint32_t vocab_size);
void nlp_bigram_train(BigramLM *lm, const uint32_t *token_ids, uint32_t len);
double nlp_bigram_probability(const BigramLM *lm, uint32_t prev, uint32_t curr);
void nlp_beam_init(BeamSearch *bs, uint32_t vocab_size, uint32_t eos_id);
void nlp_beam_step(BeamSearch *bs, double (*score_fn)(uint32_t prev, uint32_t next, void *ctx), void *ctx);
void nlp_compare_tokenizers(const char *text, TokenizerStats *results, uint32_t n);
void nlp_ner_extract(const char *text, NERResult *result);
const char *nlp_ner_entity_type_name(EntityType t);
void nlp_textrank_init(TextRankSummarizer *tr);
void nlp_textrank_add_sentence(TextRankSummarizer *tr, const char *sentence);
void nlp_textrank_compute(TextRankSummarizer *tr);
void nlp_textrank_summarize(const TextRankSummarizer *tr, uint32_t k, char *summary, size_t max_len);

static int run = 0, pass = 0;
#define T(n) do { run++; printf("  TEST: %s ... ", n); } while(0)
#define OK do { printf("PASS\n"); pass++; } while(0)

static void t_vocab_init(void) {
    T("vocab_init");
    Vocabulary v; nlp_vocab_init(&v);
    assert(v.vocab_size == 0);
    assert(v.entries != NULL);
    free(v.entries);
    OK;
}

static void t_vocab_add(void) {
    T("vocab_add/lookup");
    Vocabulary v; nlp_vocab_init(&v);
    nlp_vocab_add(&v, "hello");
    nlp_vocab_add(&v, "world");
    assert(v.vocab_size == 2);
    assert(nlp_vocab_lookup(&v, "hello") == 0);
    assert(nlp_vocab_lookup(&v, "world") == 1);
    assert(nlp_vocab_lookup(&v, "unknown") == (uint32_t)-1);
    nlp_vocab_add(&v, "hello");
    assert(v.vocab_size == 2);
    free(v.entries);
    OK;
}

static void t_whitespace_tokenize(void) {
    T("whitespace_tokenize");
    Vocabulary v; nlp_vocab_init(&v);
    nlp_vocab_add(&v, "hello"); nlp_vocab_add(&v, "world");
    TokenSequence ts;
    ts.token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    nlp_whitespace_tokenize("hello world", &ts, &v);
    assert(ts.length == 2);
    free(ts.token_ids); free(v.entries);
    OK;
}

static void t_bpe_tokenize(void) {
    T("bpe_tokenize");
    Vocabulary v; nlp_vocab_init(&v);
    nlp_vocab_add(&v, "test");
    TokenSequence ts;
    ts.token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    nlp_bpe_tokenize("test case", &ts, &v);
    assert(ts.length > 0);
    free(ts.token_ids); free(v.entries);
    OK;
}

static void t_embedding_sim(void) {
    T("embedding_similarity");
    NLPPipeline nlp; nlp_pipeline_init(&nlp, TASK_CLASSIFY);
    Vocabulary v; nlp_vocab_init(&v);
    nlp_vocab_add(&v, "king"); nlp_vocab_add(&v, "queen");
    nlp.vocab = &v; nlp_embedding_init(&nlp);
    float sim_self = nlp_embedding_similarity(&nlp, 0, 0);
    assert(fabsf(sim_self - 1.0f) < 0.01f);
    for (uint32_t i = 0; i < nlp.vocab_sz; i++) free(nlp.embeddings[i]);
    free(nlp.embeddings); free(v.entries);
    OK;
}

static void t_nb(void) {
    T("NaiveBayes");
    NaiveBayes nb; nlp_nb_init(&nb, 2, 100);
    TokenSequence docs[4];
    uint32_t labels[4] = {0,0,1,1};
    for (int i = 0; i < 4; i++) {
        docs[i].token_ids = (uint32_t*)calloc(10, sizeof(uint32_t));
        docs[i].length = 3;
        docs[i].token_ids[0] = (uint32_t)i;
        docs[i].token_ids[1] = 10+(uint32_t)i;
        docs[i].token_ids[2] = 20+(uint32_t)i;
    }
    nlp_nb_train(&nb, docs, labels, 4);
    uint32_t pred = nlp_nb_predict(&nb, &docs[0]);
    assert(pred < 2);
    for (int i = 0; i < 4; i++) free(docs[i].token_ids);
    for (uint32_t c = 0; c < nb.num_classes; c++) free(nb.likelihoods[c]);
    free(nb.likelihoods); free(nb.priors);
    OK;
}

static void t_sentiment(void) {
    T("sentiment_score");
    TokenSequence ts;
    ts.token_ids = (uint32_t*)calloc(10, sizeof(uint32_t));
    ts.length = 3;
    const char *pos[] = {"good","great","excellent"};
    const char *neg[] = {"bad","terrible","awful"};
    double s = nlp_sentiment_score(&ts, pos, 3, neg, 3);
    assert(s >= -1.0 && s <= 1.0);
    free(ts.token_ids);
    OK;
}

static void t_tfidf(void) {
    T("TF-IDF");
    TFIDFModel m;
    TokenSequence docs[2];
    for (int i = 0; i < 2; i++) {
        docs[i].token_ids = (uint32_t*)calloc(10, sizeof(uint32_t));
        docs[i].token_ids[0] = 0;
        docs[i].token_ids[1] = (uint32_t)(i+1);
        docs[i].length = 2;
    }
    nlp_tfidf_fit(&m, docs, 2);
    assert(m.doc_count == 2);
    double vec[10] = {0};
    nlp_tfidf_vectorize(&m, &docs[0], vec);
    free(m.idf);
    for (int i = 0; i < 2; i++) free(docs[i].token_ids);
    OK;
}

static void t_bigram(void) {
    T("bigram LM");
    BigramLM lm; nlp_bigram_init(&lm, 100);
    uint32_t tokens[5] = {0,1,2,1,3};
    nlp_bigram_train(&lm, tokens, 5);
    assert(lm.total_bigrams > 0);
    double p = nlp_bigram_probability(&lm, 0, 1);
    assert(p >= 0.0 && p <= 1.0);
    OK;
}

static double mock_score(uint32_t prev, uint32_t next, void *ctx) {
    (void)ctx; (void)prev; return (next == 1) ? 0.7 : 0.1;
}

static void t_beam(void) {
    T("beam_search");
    BeamSearch bs; nlp_beam_init(&bs, 5, 4);
    assert(bs.active_beams == 1);
    nlp_beam_step(&bs, mock_score, NULL);
    assert(bs.active_beams > 0);
    OK;
}

static void t_tok_compare(void) {
    T("tokenizer_compare");
    TokenizerStats stats[4];
    nlp_compare_tokenizers("Hello world test", stats, 4);
    assert(stats[0].vocab_size > 0);
    OK;
}

static void t_ner(void) {
    T("NER extract");
    NERResult result;
    nlp_ner_extract("John Smith works at Microsoft Corp.", &result);
    assert(result.count >= 1);
    OK;
}

static void t_ner_type(void) {
    T("NER entity type");
    assert(strcmp(nlp_ner_entity_type_name(ENT_PER), "PER") == 0);
    assert(strcmp(nlp_ner_entity_type_name(ENT_ORG), "ORG") == 0);
    OK;
}

static void t_textrank(void) {
    T("TextRank summarize");
    TextRankSummarizer tr; nlp_textrank_init(&tr);
    nlp_textrank_add_sentence(&tr, "Machine learning enables AI systems.");
    nlp_textrank_add_sentence(&tr, "Deep learning uses neural networks.");
    nlp_textrank_add_sentence(&tr, "AI research is accelerating fast.");
    assert(tr.count == 3);
    nlp_textrank_compute(&tr);
    char summary[512];
    nlp_textrank_summarize(&tr, 2, summary, sizeof(summary));
    assert(strlen(summary) > 0);
    OK;
}

static void t_pipeline_init(void) {
    T("pipeline_init");
    NLPPipeline nlp; nlp_pipeline_init(&nlp, TASK_NER);
    assert(nlp.task == TASK_NER);
    OK;
}

static void t_null(void) {
    T("null safety");
    nlp_vocab_init(NULL);
    nlp_vocab_add(NULL, NULL);
    nlp_vocab_lookup(NULL, NULL);
    nlp_bpe_tokenize(NULL, NULL, NULL);
    nlp_whitespace_tokenize(NULL, NULL, NULL);
    nlp_pipeline_init(NULL, 0);
    nlp_nb_train(NULL, NULL, NULL, 0);
    nlp_tfidf_fit(NULL, NULL, 0);
    nlp_bigram_init(NULL, 0);
    nlp_beam_init(NULL, 0, 0);
    nlp_textrank_init(NULL);
    nlp_textrank_compute(NULL);
    nlp_textrank_summarize(NULL, 0, NULL, 0);
    nlp_sentiment_score(NULL, NULL, 0, NULL, 0);
    OK;
}

int main(void) {
    printf("\n=== NLP Pipeline Tests ===\n\n");
    t_vocab_init(); t_vocab_add();
    t_whitespace_tokenize(); t_bpe_tokenize();
    t_embedding_sim();
    t_nb(); t_sentiment();
    t_tfidf(); t_bigram(); t_beam();
    t_tok_compare(); t_ner(); t_ner_type();
    t_textrank(); t_pipeline_init(); t_null();
    printf("\n=== Results: %d/%d tests passed ===\n", pass, run);
    return (pass == run) ? 0 : 1;
}
