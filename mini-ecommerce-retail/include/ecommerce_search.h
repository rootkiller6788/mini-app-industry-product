#ifndef ECOMMERCE_SEARCH_H
#define ECOMMERCE_SEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include "ecommerce_core.h"

/* ================================================================
 * L2: Full-text product search engine
 * L3: Inverted index data structure
 * L4: TF-IDF theorem (Salton, 1970s)
 * L5: BM25 ranking, Levenshtein fuzzy matching, pagination
 * L7: E-commerce product discovery
 * ================================================================ */

#define SEARCH_MAX_TERMS      1024
#define SEARCH_MAX_DOCS       ECOM_MAX_PRODUCTS
#define SEARCH_TERM_MAX_LEN   64
#define SEARCH_RESULTS_MAX    128
#define SEARCH_PAGE_SIZE      20
#define SEARCH_MAX_POSTINGS   8192

/* L1: Posting — records a term's occurrence in a document */
typedef struct {
    int    product_id;
    int    term_frequency;    /* TF: how many times term appears in this product */
    double weight;            /* precomputed weight for fast retrieval */
    int    positions[16];     /* term positions within document */
    int    position_count;
} Posting;

/* L1: Inverted index entry — term → list of documents */
typedef struct {
    char    term[SEARCH_TERM_MAX_LEN];
    Posting postings[SEARCH_RESULTS_MAX];
    int     posting_count;
    int     document_frequency; /* DF: how many documents contain this term */
} TermEntry;

/* L3: Inverted index — the core data structure for full-text search */
typedef struct {
    TermEntry terms[SEARCH_MAX_TERMS];
    int       term_count;
    int       total_documents;
    double    avg_doc_length;
    int       doc_lengths[SEARCH_MAX_DOCS];
} InvertedIndex;

/* L1: Search query with filters */
typedef enum {
    SORT_RELEVANCE,
    SORT_PRICE_ASC,
    SORT_PRICE_DESC,
    SORT_RATING,
    SORT_NEWEST,
    SORT_COUNT
} SearchSortOrder;

typedef struct {
    char           keywords[256];
    double         min_price;
    double         max_price;
    char           category[64];
    double         min_rating;
    SearchSortOrder sort;
    int            page;
    int            page_size;
} SearchQuery;

/* L1: Ranked search result */
typedef struct {
    int    product_id;
    double score;
    double price;
    double rating;
    char   name[ECOM_MAX_NAME];
    char   sku[32];
} SearchResult;

typedef struct {
    SearchResult results[SEARCH_RESULTS_MAX];
    int          result_count;
    int          total_hits;
    int          page;
    int          page_size;
    int          total_pages;
    double       search_time_ms;
} SearchResponse;

/* L5: Levenshtein (edit distance) — dynamic programming O(m*n)
 * Used for fuzzy search and typo correction
 * Theorem: Minimum number of single-character edits (insert, delete, substitute)
 *   to transform string a into string b.
 * Recurrence: d[i][j] = min(d[i-1][j]+1, d[i][j-1]+1, d[i-1][j-1]+cost)
 */
typedef struct {
    char    original[SEARCH_TERM_MAX_LEN];
    char    suggestion[SEARCH_TERM_MAX_LEN];
    int     distance;
    double  similarity;  /* 1.0 = identical, 0.0 = completely different */
} SpellCorrection;

/* --- Index Building (L3, L5) --- */
InvertedIndex* index_create(void);
int  index_add_document(InvertedIndex* idx, int product_id,
                        const char* name, const char* category, const char* description);
int  index_build_from_inventory(InvertedIndex* idx, const Inventory* inv,
                                 const char* descriptions[]);

/* --- Tokenization & Text Processing (L5) --- */
int  tokenize(const char* text, char tokens[][SEARCH_TERM_MAX_LEN], int max_tokens);
void stem_porter(char* word);             /* Porter stemming algorithm */
void remove_stopwords(char tokens[][SEARCH_TERM_MAX_LEN], int* count);
char* to_lowercase_str(char* str);

/* --- L4: TF-IDF Scoring ---
 * TF(t,d) = freq(t,d) / max_freq(d)
 * IDF(t) = log( N / df(t) )   where N = total docs
 * TF-IDF(t,d) = TF(t,d) * IDF(t)
 */
double compute_tf(int term_freq, int doc_len);
double compute_idf(int total_docs, int doc_freq);
double compute_tfidf(int term_freq, int doc_len, int total_docs, int doc_freq);

/* --- L5: BM25 (Okapi BM25) — probabilistic ranking function
 * BM25(D,Q) = sum( IDF(qi) * (f(qi,D)*(k1+1)) / (f(qi,D)+k1*(1-b+b*|D|/avgdl)) )
 * where k1∈[1.2,2.0], b=0.75 typically
 */
double compute_bm25(int term_freq, int doc_len, double avg_dl,
                    int total_docs, int doc_freq, double k1, double b);

/* --- Search Execution (L5, L7) --- */
SearchResponse* search_execute(const InvertedIndex* idx, const SearchQuery* query,
                                const Inventory* inv, SearchResponse* resp);
SearchResponse* search_bm25_rank(const InvertedIndex* idx, const SearchQuery* query,
                                  const Inventory* inv, SearchResponse* resp);

/* --- Fuzzy Search (L5: Levenshtein distance) --- */
int    levenshtein_distance(const char* a, const char* b);
double levenshtein_similarity(const char* a, const char* b);
int    fuzzy_search(const InvertedIndex* idx, const char* query, int max_distance,
                     SpellCorrection* corrections, int max_corrections);
int    did_you_mean(const InvertedIndex* idx, const char* query,
                     SpellCorrection* out);

/* --- Pagination (L5, L7) --- */
void   search_paginate(SearchResponse* resp, int page, int page_size);
int    search_pages_total(int total_hits, int page_size);

/* --- Filters & Facets (L7) --- */
typedef struct {
    char    category[64];
    int     count;
    double  min_price;
    double  max_price;
} SearchFacet;

typedef struct {
    SearchFacet facets[32];
    int         facet_count;
} FacetResult;

int    search_facets_compute(const SearchResponse* resp, const Inventory* inv,
                              FacetResult* facets);

/* --- Utility --- */
void   index_print_summary(const InvertedIndex* idx);
void   search_response_print(const SearchResponse* resp);
void   index_destroy(InvertedIndex* idx);
const char* sort_order_name(SearchSortOrder s);

#endif
