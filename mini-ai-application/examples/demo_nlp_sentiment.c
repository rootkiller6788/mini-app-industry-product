#include "nlp_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void nlp_vocab_print(const Vocabulary *v);
void nlp_ner_print(const NERResult *result);
void nlp_textrank_init(TextRankSummarizer *tr);
void nlp_textrank_add_sentence(TextRankSummarizer *tr, const char *sentence);
void nlp_textrank_compute(TextRankSummarizer *tr);
void nlp_textrank_summarize(const TextRankSummarizer *tr, uint32_t k, char *summary, size_t max_len);
const char *nlp_ner_entity_type_name(EntityType t);

int main(void) {
    printf("\n============================================================\n");
    printf("  NLP Pipeline Demo\n");
    printf("============================================================\n\n");

    printf("[1] Building vocabulary from sample corpus...\n");
    Vocabulary v;
    nlp_vocab_init(&v);
    const char *corpus[] = {
        "machine", "learning", "deep", "neural", "network",
        "artificial", "intelligence", "data", "model", "training",
        "excellent", "great", "good", "amazing", "wonderful",
        "terrible", "bad", "awful", "poor", "disappointing"
    };
    for (int i = 0; i < 20; i++) nlp_vocab_add(&v, corpus[i]);
    printf("    Vocabulary: %u words\n", v.vocab_size);

    printf("\n[2] Tokenization demo:\n");
    TokenSequence ts;
    ts.token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    nlp_whitespace_tokenize("machine learning is excellent and amazing", &ts, &v);
    printf("    Input: 'machine learning is excellent and amazing'\n");
    printf("    Tokens produced: %u\n", ts.length);

    printf("\n[3] Word embedding similarity:\n");
    NLPPipeline nlp;
    nlp_pipeline_init(&nlp, TASK_CLASSIFY);
    nlp.vocab = &v;
    nlp_embedding_init(&nlp);
    float sim_ml_ai = nlp_embedding_similarity(&nlp, 0, 6);
    float sim_ml_bad = nlp_embedding_similarity(&nlp, 0, 16);
    printf("    sim(machine, intelligence) = %.4f\n", sim_ml_ai);
    printf("    sim(machine, terrible) = %.4f\n", sim_ml_bad);

    printf("\n[4] Sentiment analysis:\n");
    const char *pos[] = {"excellent","great","good","amazing","wonderful"};
    const char *neg[] = {"terrible","bad","awful","poor","disappointing"};
    double sent = nlp_sentiment_score(&ts, pos, 5, neg, 5);
    printf("    Positive sentence score: %.4f\n", sent);

    TokenSequence ts_neg;
    ts_neg.token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    nlp_whitespace_tokenize("this is terrible and disappointing", &ts_neg, &v);
    double sent_neg = nlp_sentiment_score(&ts_neg, pos, 5, neg, 5);
    printf("    Negative sentence score: %.4f\n", sent_neg);
    free(ts_neg.token_ids);

    printf("\n[5] TF-IDF document vectorization:\n");
    TFIDFModel tfidf;
    TokenSequence docs[2];
    docs[0].token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    docs[1].token_ids = (uint32_t*)calloc(NLP_MAX_TOKENS, sizeof(uint32_t));
    nlp_whitespace_tokenize("machine learning deep neural", docs, &v);
    nlp_whitespace_tokenize("artificial intelligence data model", &docs[1], &v);
    nlp_tfidf_fit(&tfidf, docs, 2);
    printf("    Vocab: %u, Docs: %u\n", tfidf.vocab_sz, tfidf.doc_count);
    free(tfidf.idf);
    free(docs[0].token_ids); free(docs[1].token_ids);

    printf("\n[6] TextRank extractive summarization:\n");
    TextRankSummarizer tr;
    nlp_textrank_init(&tr);
    nlp_textrank_add_sentence(&tr, "Artificial intelligence has transformed modern technology.");
    nlp_textrank_add_sentence(&tr, "Machine learning enables computers to learn from data.");
    nlp_textrank_add_sentence(&tr, "Deep learning uses multi-layer neural networks.");
    nlp_textrank_add_sentence(&tr, "Natural language processing helps understand human language.");
    nlp_textrank_compute(&tr);
    char summary[1024];
    nlp_textrank_summarize(&tr, 2, summary, sizeof(summary));
    printf("    Top-2 summary: %s\n", summary);

    printf("\n[7] Named Entity Recognition:\n");
    NERResult ner;
    nlp_ner_extract("Dr. Jane Smith from Stanford University and Microsoft Corp announced GPT-5.", &ner);
    nlp_ner_print(&ner);

    printf("\n============================================================\n");
    printf("  NLP Pipeline Demo Complete!\n");
    printf("============================================================\n");
    return 0;
}
