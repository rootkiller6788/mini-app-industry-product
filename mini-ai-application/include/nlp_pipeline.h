#ifndef NLP_PIPELINE_H
#define NLP_PIPELINE_H
#include <stdbool.h>
#include <stdint.h>

#define NLP_MAX_TOKENS    512
#define NLP_VOCAB_SIZE   10000
#define NLP_MAX_SEQ_LEN   256
#define NLP_EMBED_DIM      64

typedef enum { TOK_UNK, TOK_WORD, TOK_NUM, TOK_PUNCT, TOK_SPECIAL } TokenType;

typedef struct { char text[64]; uint32_t id; TokenType type; uint64_t freq; } VocabEntry;
typedef struct { uint32_t vocab_size; VocabEntry *entries; } Vocabulary;

typedef struct { uint32_t *token_ids; uint32_t length; } TokenSequence;

typedef enum { TASK_CLASSIFY, TASK_NER, TASK_SENTIMENT, TASK_SUMMARIZE } NLPTask;

typedef struct {
    NLPTask task;
    Vocabulary *vocab;
    float **embeddings;
    uint32_t embed_dim, vocab_sz;
} NLPPipeline;

/* L1 API */
void nlp_vocab_init(Vocabulary *v);
void nlp_vocab_add(Vocabulary *v, const char *word);
uint32_t nlp_vocab_lookup(const Vocabulary *v, const char *word);

/* L2: BPE Tokenization (Sennrich et al. 2016) */
void nlp_bpe_tokenize(const char *text, TokenSequence *tokens, const Vocabulary *vocab);
void nlp_whitespace_tokenize(const char *text, TokenSequence *tokens, const Vocabulary *vocab);

/* L2: Embedding lookup and similarity */
void nlp_embedding_init(NLPPipeline *nlp);
float nlp_embedding_similarity(const NLPPipeline *nlp, uint32_t w1, uint32_t w2);
void nlp_sentence_embedding(const NLPPipeline *nlp, const TokenSequence *tokens, float *out);

/* L5: Naive Bayes text classification */
typedef struct { uint32_t num_classes; double *priors; double **likelihoods; uint32_t vocab_sz; } NaiveBayes;
void nlp_nb_init(NaiveBayes *nb, uint32_t classes, uint32_t vocab_sz);
void nlp_nb_train(NaiveBayes *nb, const TokenSequence *docs, const uint32_t *labels, uint32_t count);
uint32_t nlp_nb_predict(const NaiveBayes *nb, const TokenSequence *doc);

/* L5: Sentiment analysis — lexicon-based */
double nlp_sentiment_score(const TokenSequence *tokens, const char **pos_words, uint32_t pos_cnt, const char **neg_words, uint32_t neg_cnt);

/* L6: TF-IDF document vector */
typedef struct { uint32_t doc_count; double *idf; uint32_t vocab_sz; } TFIDFModel;
void nlp_tfidf_fit(TFIDFModel *m, const TokenSequence *docs, uint32_t count);
void nlp_tfidf_vectorize(const TFIDFModel *m, const TokenSequence *doc, double *vec);

void nlp_pipeline_init(NLPPipeline *nlp, NLPTask task);
void nlp_vocab_print(const Vocabulary *v);

/* L8: Bigram Language Model with Kneser-Ney Smoothing */
#define NGRAM_MAX_VOCAB 200
#define NGRAM_MAX_ORDER 3
typedef struct { uint32_t counts[NGRAM_MAX_VOCAB][NGRAM_MAX_VOCAB]; uint32_t unigram_counts[NGRAM_MAX_VOCAB]; uint32_t total_bigrams; uint32_t vocab_size; double discount; } BigramLM;
void nlp_bigram_init(BigramLM *lm, uint32_t vocab_size);
void nlp_bigram_train(BigramLM *lm, const uint32_t *token_ids, uint32_t len);
double nlp_bigram_probability(const BigramLM *lm, uint32_t prev, uint32_t curr);

/* L8: Beam Search Decoding */
#define BEAM_WIDTH 5
#define BEAM_MAX_LEN 64
typedef struct { uint32_t tokens[BEAM_MAX_LEN]; uint32_t length; double log_prob; bool finished; } BeamHypothesis;
typedef struct { BeamHypothesis beams[BEAM_WIDTH]; uint32_t active_beams; uint32_t eos_token_id; uint32_t vocab_size; } BeamSearch;
void nlp_beam_init(BeamSearch *bs, uint32_t vocab_size, uint32_t eos_id);
void nlp_beam_step(BeamSearch *bs, double (*score_fn)(uint32_t prev, uint32_t next, void *ctx), void *ctx);

/* L9: Tokenizer Comparison */
typedef struct { uint32_t vocab_size; uint32_t num_segments; double compression_ratio; double avg_tokens_per_word; char algorithm[32]; } TokenizerStats;
void nlp_compare_tokenizers(const char *text, TokenizerStats *results, uint32_t n);

/* L7: Named Entity Recognition */
#define NER_MAX_ENTITIES 64
#define NER_MAX_TEXT_LEN 2048
typedef enum { ENT_PER, ENT_ORG, ENT_LOC, ENT_DATE, ENT_MONEY, ENT_PCT, ENT_MISC } EntityType;
typedef struct { char text[128]; EntityType type; uint32_t start_pos; uint32_t end_pos; double confidence; } NamedEntity;
typedef struct { NamedEntity entities[NER_MAX_ENTITIES]; uint32_t count; char source_text[NER_MAX_TEXT_LEN]; } NERResult;
void nlp_ner_extract(const char *text, NERResult *result);
void nlp_ner_print(const NERResult *result);
const char *nlp_ner_entity_type_name(EntityType t);

/* L7: TextRank Summarization */
#define TEXTRANK_MAX_SENT 128
#define TEXTRANK_MAX_ITER 50
typedef struct { char text[512]; double tfidf[128]; double score; uint32_t rank; } TextRankSentence;
typedef struct { TextRankSentence sentences[TEXTRANK_MAX_SENT]; uint32_t count; double damping; } TextRankSummarizer;
void nlp_textrank_init(TextRankSummarizer *tr);
void nlp_textrank_add_sentence(TextRankSummarizer *tr, const char *sentence);
void nlp_textrank_compute(TextRankSummarizer *tr);
void nlp_textrank_summarize(const TextRankSummarizer *tr, uint32_t k, char *summary, size_t max_len);
#endif