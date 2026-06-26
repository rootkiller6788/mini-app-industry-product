#include "ecommerce_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ================================================================
 * L2: Full-text search engine for e-commerce products
 * L3: Inverted index — core data structure (term → doc list)
 * L4: TF-IDF theorem (Salton, 1970s)
 * L5: BM25 ranking, Porter Stemmer, Levenshtein distance, pagination
 * L7: Product discovery with filtering, faceting, and sorting
 * ================================================================ */

const char* sort_order_name(SearchSortOrder s) {
    switch (s) {
        case SORT_RELEVANCE:  return "Relevance";
        case SORT_PRICE_ASC:  return "Price Low-High";
        case SORT_PRICE_DESC: return "Price High-Low";
        case SORT_RATING:     return "Rating";
        case SORT_NEWEST:     return "Newest";
        default:              return "Unknown";
    }
}

/* --- L2: Lifecycle --- */

InvertedIndex* index_create(void) {
    InvertedIndex* idx = (InvertedIndex*)calloc(1, sizeof(InvertedIndex));
    return idx;
}

void index_destroy(InvertedIndex* idx) { free(idx); }

/* --- L5: Text Processing Utilities --- */

char* to_lowercase_str(char* str) {
    if (!str) return str;
    for (char* p = str; *p; p++) *p = (char)tolower((unsigned char)*p);
    return str;
}

/* L5: Tokenization — splits text into lowercase tokens by whitespace/punctuation
 * Removes empty tokens and duplicates within a single document.
 */
int tokenize(const char* text, char tokens[][SEARCH_TERM_MAX_LEN], int max_tokens) {
    if (!text || !tokens || max_tokens <= 0) return 0;
    int count = 0;
    const char* p = text;
    char token[SEARCH_TERM_MAX_LEN];

    while (*p && count < max_tokens) {
        /* Skip non-alphanumeric characters (except for search) */
        while (*p && !isalnum((unsigned char)*p) && *p != '\'') p++;
        if (!*p) break;

        int len = 0;
        while (*p && (isalnum((unsigned char)*p) || *p == '\'')
               && len < SEARCH_TERM_MAX_LEN - 1) {
            token[len++] = (char)tolower((unsigned char)*p);
            p++;
        }
        token[len] = '\0';

        if (len > 0) {
            /* Check for duplicate in current token list */
            bool dup = false;
            for (int i = 0; i < count; i++) {
                if (strcmp(tokens[i], token) == 0) { dup = true; break; }
            }
            if (!dup) {
                strncpy(tokens[count], token, SEARCH_TERM_MAX_LEN - 1);
                tokens[count][SEARCH_TERM_MAX_LEN - 1] = '\0';
                count++;
            }
        }
    }
    return count;
}

/* L5: Stop word removal — filters common English words that don't add meaning */
static bool is_stopword(const char* word) {
    static const char* stops[] = {
        "a","an","the","and","or","but","in","on","at","to","for",
        "of","with","by","from","is","are","was","were","be","been",
        "being","have","has","had","do","does","did","will","would",
        "shall","should","can","could","may","might","must","it",
        "its","this","that","these","those","not","no","nor","so",
        "if","then","else","when","where","who","whom","which","what",
        "how","all","each","every","both","few","more","most","other",
        "some","such","only","own","same","too","very","just","about",
        "into","over","after","before","between","under","above","below",
        "up","down","out","off","new","old","good","great","big","small",
        NULL
    };
    for (int i = 0; stops[i]; i++)
        if (strcmp(word, stops[i]) == 0) return true;
    return false;
}

void remove_stopwords(char tokens[][SEARCH_TERM_MAX_LEN], int* count) {
    if (!tokens || !count) return;
    int j = 0;
    for (int i = 0; i < *count; i++) {
        if (!is_stopword(tokens[i])) {
            if (i != j) strcpy(tokens[j], tokens[i]);
            j++;
        }
    }
    *count = j;
}

/* L5: Porter Stemming Algorithm (simplified Step 1a)
 * Reduces words to their root form for better search matching.
 * Step 1a handles: -sses→-ss, -ies→-i, -ss→-ss, -s→
 * Reference: Porter, M.F. (1980). "An algorithm for suffix stripping."
 */
void stem_porter(char* word) {
    if (!word) return;
    int len = (int)strlen(word);
    if (len < 3) return;

    /* Step 1a: Plural suffixes */
    if (len > 4 && strcmp(word + len - 4, "sses") == 0) {
        word[len - 4] = 's'; word[len - 3] = 's'; word[len - 2] = '\0';
    } else if (len > 3 && strcmp(word + len - 3, "ies") == 0) {
        word[len - 3] = 'i'; word[len - 2] = '\0';
    } else if (len > 2 && strcmp(word + len - 2, "ss") == 0) {
        /* keep "ss" as-is */
    } else if (len > 1 && word[len - 1] == 's' && word[len - 2] != 's') {
        word[len - 1] = '\0';
    }

    /* Step 1b: -ed, -ing */
    len = (int)strlen(word);
    if (len > 3 && strcmp(word + len - 2, "ed") == 0) {
        word[len - 2] = '\0';
    } else if (len > 4 && strcmp(word + len - 3, "ing") == 0) {
        word[len - 3] = '\0';
    }

    /* Common suffixes */
    len = (int)strlen(word);
    if (len > 4 && strcmp(word + len - 4, "ness") == 0) word[len - 4] = '\0';
    else if (len > 3 && strcmp(word + len - 3, "ful") == 0) word[len - 3] = '\0';
    else if (len > 3 && strcmp(word + len - 3, "est") == 0) word[len - 3] = '\0';
    else if (len > 2 && strcmp(word + len - 2, "er") == 0) word[len - 2] = '\0';
    else if (len > 2 && strcmp(word + len - 2, "ly") == 0) word[len - 2] = '\0';
}

/* --- L4: TF-IDF Computation ---
 * TF(t,d) = freq(t,d) / max_freq(d)    — term frequency normalized
 * IDF(t) = log10(N / df(t))            — inverse document frequency
 * TF-IDF(t,d) = TF(t,d) * IDF(t)       — combined weight
 *
 * Theorem: Words that appear frequently in few documents are most
 *   discriminative for relevance ranking (Salton & Buckley, 1988).
 */

double compute_tf(int term_freq, int doc_len) {
    if (doc_len <= 0) return 0.0;
    return (double)term_freq / (double)doc_len;
}

double compute_idf(int total_docs, int doc_freq) {
    if (total_docs <= 0 || doc_freq <= 0) return 0.0;
    /* Use log10(N/df) — standard IDF formulation */
    double ratio = (double)total_docs / (double)doc_freq;
    return log10(ratio > 1.0 ? ratio : 1.0);
}

double compute_tfidf(int term_freq, int doc_len, int total_docs, int doc_freq) {
    double tf  = compute_tf(term_freq, doc_len);
    double idf = compute_idf(total_docs, doc_freq);
    return tf * idf;
}

/* --- L5: BM25 Ranking Function ---
 * BM25(D,Q) = sum over query terms qi:
 *   IDF(qi) * (tf*(k1+1)) / (tf + k1*(1-b + b*|D|/avgdl))
 *
 * Parameters: k1=1.5 (term frequency saturation), b=0.75 (length normalization)
 * Reference: Robertson & Zaragoza (2009). "The Probabilistic Relevance Framework."
 */
double compute_bm25(int term_freq, int doc_len, double avg_dl,
                    int total_docs, int doc_freq, double k1, double b) {
    double idf = compute_idf(total_docs, doc_freq);
    if (doc_freq <= 0 || avg_dl <= 0.0) return 0.0;

    double tf_norm = (double)term_freq * (k1 + 1.0);
    double len_norm = 1.0 - b + b * ((double)doc_len / avg_dl);
    double denom = (double)term_freq + k1 * len_norm;

    return idf * (tf_norm / denom);
}

/* --- L3: Inverted Index Building --- */

static int index_find_term(const InvertedIndex* idx, const char* term) {
    for (int i = 0; i < idx->term_count; i++) {
        if (strcmp(idx->terms[i].term, term) == 0) return i;
    }
    return -1;
}

int index_add_document(InvertedIndex* idx, int product_id,
                        const char* name, const char* category,
                        const char* description) {
    if (!idx || idx->term_count >= SEARCH_MAX_TERMS) return -1;

    /* Combine all text fields */
    char full_text[1024] = {0};
    if (name) { strncat(full_text, name, 255); strncat(full_text, " ", 2); }
    if (category) { strncat(full_text, category, 255); strncat(full_text, " ", 2); }
    if (description) strncat(full_text, description, 512);

    char tokens[128][SEARCH_TERM_MAX_LEN];
    int token_count = tokenize(full_text, tokens, 128);
    remove_stopwords(tokens, &token_count);

    /* Stem and add to index */
    int doc_len = token_count;
    if (product_id < SEARCH_MAX_DOCS) idx->doc_lengths[product_id] = doc_len;

    for (int t = 0; t < token_count; t++) {
        stem_porter(tokens[t]);
        int term_idx = index_find_term(idx, tokens[t]);

        if (term_idx < 0) {
            /* New term */
            if (idx->term_count >= SEARCH_MAX_TERMS) continue;
            term_idx = idx->term_count;
            strncpy(idx->terms[term_idx].term, tokens[t], SEARCH_TERM_MAX_LEN - 1);
            idx->terms[term_idx].term[SEARCH_TERM_MAX_LEN - 1] = '\0';
            idx->terms[term_idx].posting_count = 0;
            idx->terms[term_idx].document_frequency = 0;
            idx->term_count++;
        }

        TermEntry* entry = &idx->terms[term_idx];

        /* Check if product already posted for this term */
        int posting_idx = -1;
        for (int p = 0; p < entry->posting_count; p++) {
            if (entry->postings[p].product_id == product_id) {
                posting_idx = p;
                break;
            }
        }

        if (posting_idx >= 0) {
            entry->postings[posting_idx].term_frequency++;
        } else {
            if (entry->posting_count >= SEARCH_RESULTS_MAX) continue;
            posting_idx = entry->posting_count;
            entry->postings[posting_idx].product_id = product_id;
            entry->postings[posting_idx].term_frequency = 1;
            entry->postings[posting_idx].position_count = 0;
            entry->postings[posting_idx].weight = 0.0;
            entry->posting_count++;
            entry->document_frequency++;
        }
    }

    idx->total_documents++;
    /* Update average document length */
    idx->avg_doc_length = 0.0;
    int doc_count = 0;
    for (int i = 0; i < SEARCH_MAX_DOCS; i++) {
        if (idx->doc_lengths[i] > 0) {
            idx->avg_doc_length += idx->doc_lengths[i];
            doc_count++;
        }
    }
    if (doc_count > 0) idx->avg_doc_length /= (double)doc_count;

    return 0;
}

int index_build_from_inventory(InvertedIndex* idx, const Inventory* inv,
                                const char* descriptions[]) {
    if (!idx || !inv) return -1;
    for (int i = 0; i < inv->product_count; i++) {
        const char* desc = (descriptions && descriptions[i]) ? descriptions[i] : "";
        index_add_document(idx, inv->products[i].id,
                          inv->products[i].name,
                          inv->products[i].category,
                          desc);
    }
    return 0;
}

/* --- L5: Levenshtein Edit Distance ---
 * Dynamic programming: O(m*n) time, O(n) space (optimized).
 * Recurrence:
 *   d[i][0] = i      (insert all)
 *   d[0][j] = j      (delete all)
 *   d[i][j] = min(d[i-1][j]+1, d[i][j-1]+1, d[i-1][j-1]+(a[i]!=b[j]))
 *
 * Reference: Levenshtein, V.I. (1966). "Binary codes capable of correcting
 *   deletions, insertions, and reversals."
 */
int levenshtein_distance(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return (int)strlen(b);
    if (!b) return (int)strlen(a);

    int m = (int)strlen(a);
    int n = (int)strlen(b);

    /* Space-optimized: use two rows */
    int* prev = (int*)calloc((size_t)(n + 1), sizeof(int));
    int* curr = (int*)calloc((size_t)(n + 1), sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return -1; }

    for (int j = 0; j <= n; j++) prev[j] = j;

    for (int i = 1; i <= m; i++) {
        curr[0] = i;
        for (int j = 1; j <= n; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int min_val = prev[j] + 1;           /* delete */
            if (curr[j - 1] + 1 < min_val)        /* insert */
                min_val = curr[j - 1] + 1;
            if (prev[j - 1] + cost < min_val)     /* substitute */
                min_val = prev[j - 1] + cost;
            curr[j] = min_val;
        }
        int* tmp = prev; prev = curr; curr = tmp;
    }

    int result = prev[n];
    free(prev); free(curr);
    return result;
}

double levenshtein_similarity(const char* a, const char* b) {
    if (!a || !b) return 0.0;
    int dist = levenshtein_distance(a, b);
    if (dist < 0) return 0.0;
    int max_len = (int)strlen(a) > (int)strlen(b) ? (int)strlen(a) : (int)strlen(b);
    if (max_len == 0) return 1.0;
    return 1.0 - (double)dist / (double)max_len;
}

int fuzzy_search(const InvertedIndex* idx, const char* query,
                  int max_distance, SpellCorrection* corrections,
                  int max_corrections) {
    if (!idx || !query || !corrections || max_corrections <= 0) return 0;
    char query_tokens[32][SEARCH_TERM_MAX_LEN];
    int qcount = tokenize(query, query_tokens, 32);
    int found = 0;

    for (int q = 0; q < qcount && found < max_corrections; q++) {
        for (int i = 0; i < idx->term_count && found < max_corrections; i++) {
            int dist = levenshtein_distance(query_tokens[q], idx->terms[i].term);
            if (dist > 0 && dist <= max_distance) {
                strncpy(corrections[found].original, query_tokens[q],
                        SEARCH_TERM_MAX_LEN - 1);
                strncpy(corrections[found].suggestion, idx->terms[i].term,
                        SEARCH_TERM_MAX_LEN - 1);
                corrections[found].distance = dist;
                corrections[found].similarity =
                    levenshtein_similarity(query_tokens[q], idx->terms[i].term);
                found++;
            }
        }
    }
    return found;
}

int did_you_mean(const InvertedIndex* idx, const char* query,
                  SpellCorrection* out) {
    if (!idx || !query || !out) return 0;
    SpellCorrection corrections[32];
    int found = fuzzy_search(idx, query, 3, corrections, 32);
    if (found == 0) return 0;

    /* Return the suggestion with highest similarity */
    double best = 0.0;
    int best_idx = 0;
    for (int i = 0; i < found; i++) {
        if (corrections[i].similarity > best) {
            best = corrections[i].similarity;
            best_idx = i;
        }
    }
    *out = corrections[best_idx];
    return 1;
}

/* --- L5: Search Execution --- */

/* L4: Compare function for qsort — sorts SearchResults by sort order */
static int compare_by_score_desc(const void* a, const void* b) {
    const SearchResult* ra = (const SearchResult*)a;
    const SearchResult* rb = (const SearchResult*)b;
    if (rb->score > ra->score) return 1;
    if (rb->score < ra->score) return -1;
    return 0;
}

static int compare_by_price_asc(const void* a, const void* b) {
    const SearchResult* ra = (const SearchResult*)a;
    const SearchResult* rb = (const SearchResult*)b;
    if (ra->price > rb->price) return 1;
    if (ra->price < rb->price) return -1;
    return 0;
}

static int compare_by_price_desc(const void* a, const void* b) {
    const SearchResult* ra = (const SearchResult*)a;
    const SearchResult* rb = (const SearchResult*)b;
    if (rb->price > ra->price) return 1;
    if (rb->price < ra->price) return -1;
    return 0;
}

static int compare_by_rating_desc(const void* a, const void* b) {
    const SearchResult* ra = (const SearchResult*)a;
    const SearchResult* rb = (const SearchResult*)b;
    if (rb->rating > ra->rating) return 1;
    if (rb->rating < ra->rating) return -1;
    return 0;
}

SearchResponse* search_execute(const InvertedIndex* idx, const SearchQuery* query,
                                const Inventory* inv, SearchResponse* resp) {
    if (!idx || !query || !inv || !resp) return NULL;
    memset(resp, 0, sizeof(SearchResponse));

    /* Tokenize query */
    char query_tokens[32][SEARCH_TERM_MAX_LEN];
    int qcount = tokenize(query->keywords, query_tokens, 32);
    if (qcount == 0) {
        /* Empty query: return all products sorted appropriately */
        for (int i = 0; i < inv->product_count && i < SEARCH_RESULTS_MAX; i++) {
            const Product* p = &inv->products[i];
            /* Apply filters */
            if (query->min_price > 0 && p->price < query->min_price) continue;
            if (query->max_price > 0 && p->price > query->max_price) continue;
            if (query->min_rating > 0 && p->rating < query->min_rating) continue;
            if (query->category[0] && !strstr(p->category, query->category)) continue;

            resp->results[resp->result_count].product_id = p->id;
            resp->results[resp->result_count].score = 1.0;
            resp->results[resp->result_count].price = p->price;
            resp->results[resp->result_count].rating = p->rating;
            strncpy(resp->results[resp->result_count].name, p->name, ECOM_MAX_NAME - 1);
            strncpy(resp->results[resp->result_count].sku, p->sku, 31);
            resp->result_count++;
        }
        resp->total_hits = resp->result_count;
        resp->page = query->page > 0 ? query->page : 1;
        resp->page_size = query->page_size > 0 ? query->page_size : SEARCH_PAGE_SIZE;
        resp->total_pages = (resp->total_hits + resp->page_size - 1) / resp->page_size;
        return resp;
    }

    /* Remove stopwords and stem query terms */
    remove_stopwords(query_tokens, &qcount);
    for (int i = 0; i < qcount; i++) stem_porter(query_tokens[i]);

    /* Accumulate TF-IDF scores across query terms */
    typedef struct { double score; int prod_id; } ScoreEntry;
    ScoreEntry scores[SEARCH_MAX_DOCS];
    int score_count = 0;
    int prod_to_idx[SEARCH_MAX_DOCS];
    memset(prod_to_idx, -1, sizeof(prod_to_idx));

    for (int t = 0; t < qcount; t++) {
        int term_idx = index_find_term(idx, query_tokens[t]);
        if (term_idx < 0) continue;
        const TermEntry* entry = &idx->terms[term_idx];

        for (int p = 0; p < entry->posting_count; p++) {
            int pid = entry->postings[p].product_id;
            if (pid >= SEARCH_MAX_DOCS) continue;

            double tfidf = compute_tfidf(
                entry->postings[p].term_frequency,
                idx->doc_lengths[pid] > 0 ? idx->doc_lengths[pid] : 1,
                idx->total_documents,
                entry->document_frequency);

            if (prod_to_idx[pid] < 0) {
                if (score_count >= SEARCH_MAX_DOCS) continue;
                prod_to_idx[pid] = score_count;
                scores[score_count].prod_id = pid;
                scores[score_count].score = tfidf;
                score_count++;
            } else {
                scores[prod_to_idx[pid]].score += tfidf;
            }
        }
    }

    /* Sort by score descending */
    for (int i = 0; i < score_count - 1; i++) {
        for (int j = i + 1; j < score_count; j++) {
            if (scores[j].score > scores[i].score) {
                ScoreEntry tmp = scores[i];
                scores[i] = scores[j];
                scores[j] = tmp;
            }
        }
    }

    /* Fill search response with filter application */
    for (int i = 0; i < score_count && resp->result_count < SEARCH_RESULTS_MAX; i++) {
        Product* p = inv_find_by_id((Inventory*)inv, scores[i].prod_id);
        if (!p) continue;

        if (query->min_price > 0 && p->price < query->min_price) continue;
        if (query->max_price > 0 && p->price > query->max_price) continue;
        if (query->min_rating > 0 && p->rating < query->min_rating) continue;
        if (query->category[0] && !strstr(p->category, query->category)) continue;

        SearchResult* r = &resp->results[resp->result_count];
        r->product_id = p->id;
        r->score = scores[i].score;
        r->price = p->price;
        r->rating = p->rating;
        strncpy(r->name, p->name, ECOM_MAX_NAME - 1);
        strncpy(r->sku, p->sku, 31);
        resp->result_count++;
    }

    resp->total_hits = resp->result_count;
    resp->page = query->page > 0 ? query->page : 1;
    resp->page_size = query->page_size > 0 ? query->page_size : SEARCH_PAGE_SIZE;
    resp->total_pages = (resp->total_hits + resp->page_size - 1) / resp->page_size;

    /* Apply sorting */
    switch (query->sort) {
        case SORT_PRICE_ASC:
            qsort(resp->results, (size_t)resp->result_count, sizeof(SearchResult),
                  compare_by_price_asc);
            break;
        case SORT_PRICE_DESC:
            qsort(resp->results, (size_t)resp->result_count, sizeof(SearchResult),
                  compare_by_price_desc);
            break;
        case SORT_RATING:
            qsort(resp->results, (size_t)resp->result_count, sizeof(SearchResult),
                  compare_by_rating_desc);
            break;
        case SORT_RELEVANCE:
            qsort(resp->results, (size_t)resp->result_count, sizeof(SearchResult),
                  compare_by_score_desc);
            break;
        default: break;
    }

    return resp;
}

/* L5: BM25-based ranking variant (more advanced than vanilla TF-IDF) */
SearchResponse* search_bm25_rank(const InvertedIndex* idx, const SearchQuery* query,
                                  const Inventory* inv, SearchResponse* resp) {
    if (!idx || !query || !inv || !resp) return NULL;
    memset(resp, 0, sizeof(SearchResponse));

    char query_tokens[32][SEARCH_TERM_MAX_LEN];
    int qcount = tokenize(query->keywords, query_tokens, 32);
    remove_stopwords(query_tokens, &qcount);
    for (int i = 0; i < qcount; i++) stem_porter(query_tokens[i]);

    if (qcount == 0) return search_execute(idx, query, inv, resp);

    typedef struct { double score; int prod_id; } ScoreEntry;
    ScoreEntry scores[SEARCH_MAX_DOCS];
    int score_count = 0;
    int prod_to_idx[SEARCH_MAX_DOCS];
    memset(prod_to_idx, -1, sizeof(prod_to_idx));

    for (int t = 0; t < qcount; t++) {
        int term_idx = index_find_term(idx, query_tokens[t]);
        if (term_idx < 0) continue;
        const TermEntry* entry = &idx->terms[term_idx];

        for (int p = 0; p < entry->posting_count; p++) {
            int pid = entry->postings[p].product_id;
            if (pid >= SEARCH_MAX_DOCS) continue;

            double bm25 = compute_bm25(
                entry->postings[p].term_frequency,
                idx->doc_lengths[pid] > 0 ? idx->doc_lengths[pid] : 1,
                idx->avg_doc_length > 0 ? idx->avg_doc_length : 1.0,
                idx->total_documents,
                entry->document_frequency,
                1.5, 0.75);

            if (prod_to_idx[pid] < 0) {
                if (score_count >= SEARCH_MAX_DOCS) continue;
                prod_to_idx[pid] = score_count;
                scores[score_count].prod_id = pid;
                scores[score_count].score = bm25;
                score_count++;
            } else {
                scores[prod_to_idx[pid]].score += bm25;
            }
        }
    }

    for (int i = 0; i < score_count - 1; i++) {
        for (int j = i + 1; j < score_count; j++) {
            if (scores[j].score > scores[i].score) {
                ScoreEntry tmp = scores[i];
                scores[i] = scores[j];
                scores[j] = tmp;
            }
        }
    }

    for (int i = 0; i < score_count && resp->result_count < SEARCH_RESULTS_MAX; i++) {
        Product* p = inv_find_by_id((Inventory*)inv, scores[i].prod_id);
        if (!p) continue;
        if (query->min_price > 0 && p->price < query->min_price) continue;
        if (query->max_price > 0 && p->price > query->max_price) continue;
        if (query->category[0] && !strstr(p->category, query->category)) continue;

        SearchResult* r = &resp->results[resp->result_count];
        r->product_id = p->id;
        r->score = scores[i].score;
        r->price = p->price;
        r->rating = p->rating;
        strncpy(r->name, p->name, ECOM_MAX_NAME - 1);
        strncpy(r->sku, p->sku, 31);
        resp->result_count++;
    }

    resp->total_hits = resp->result_count;
    resp->page = query->page > 0 ? query->page : 1;
    resp->page_size = query->page_size > 0 ? query->page_size : SEARCH_PAGE_SIZE;
    resp->total_pages = (resp->total_hits + resp->page_size - 1) / resp->page_size;

    return resp;
}

/* --- L5: Pagination --- */

void search_paginate(SearchResponse* resp, int page, int page_size) {
    if (!resp || page < 1 || page_size < 1) return;
    resp->page = page;
    resp->page_size = page_size;
    resp->total_hits = resp->result_count;
    resp->total_pages = (resp->total_hits + page_size - 1) / page_size;
}

int search_pages_total(int total_hits, int page_size) {
    if (page_size <= 0) return 0;
    return (total_hits + page_size - 1) / page_size;
}

/* --- L7: Faceted Search --- */

int search_facets_compute(const SearchResponse* resp, const Inventory* inv,
                           FacetResult* facets) {
    if (!resp || !inv || !facets) return 0;
    memset(facets, 0, sizeof(FacetResult));

    for (int i = 0; i < resp->result_count && facets->facet_count < 32; i++) {
        Product* p = inv_find_by_id((Inventory*)inv, resp->results[i].product_id);
        if (!p) continue;

        /* Check if category already in facets */
        bool found = false;
        for (int f = 0; f < facets->facet_count; f++) {
            if (strcmp(facets->facets[f].category, p->category) == 0) {
                facets->facets[f].count++;
                if (p->price < facets->facets[f].min_price || facets->facets[f].count == 1)
                    facets->facets[f].min_price = p->price;
                if (p->price > facets->facets[f].max_price || facets->facets[f].count == 1)
                    facets->facets[f].max_price = p->price;
                found = true;
                break;
            }
        }

        if (!found) {
            SearchFacet* sf = &facets->facets[facets->facet_count];
            strncpy(sf->category, p->category, 63);
            sf->count = 1;
            sf->min_price = p->price;
            sf->max_price = p->price;
            facets->facet_count++;
        }
    }
    return facets->facet_count;
}

/* --- Utility --- */

void index_print_summary(const InvertedIndex* idx) {
    if (!idx) return;
    printf("Inverted Index: %d terms, %d documents, avg_dl=%.1f\n",
           idx->term_count, idx->total_documents, idx->avg_doc_length);
    printf("Top 10 terms by document frequency:\n");
    for (int i = 0; i < idx->term_count && i < 10; i++) {
        printf("  %-20s df=%d postings=%d\n",
               idx->terms[i].term,
               idx->terms[i].document_frequency,
               idx->terms[i].posting_count);
    }
}

void search_response_print(const SearchResponse* resp) {
    if (!resp) return;
    printf("Search Results: %d hits (page %d/%d)\n",
           resp->total_hits, resp->page, resp->total_pages);
    for (int i = 0; i < resp->result_count; i++) {
        const SearchResult* r = &resp->results[i];
        printf("  [%d] %-30s $%8.2f score=%.4f rating=%.1f\n",
               r->product_id, r->name, r->price, r->score, r->rating);
    }
}