#ifndef MINI_APP_CONTENT_CACHE_H
#define MINI_APP_CONTENT_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* LRU (Least Recently Used) content cache for web server responses.
 * Reduces backend load by caching frequently accessed content.
 *
 * L4: Cache Replacement Policy Theorem
 * LRU is optimal among online algorithms when the access pattern
 * follows temporal locality. Competitive ratio: k where k = cache size.
 * Belady's OPT algorithm (offline) provides the theoretical lower bound. */

#define MINI_APP_CACHE_MAX_ENTRIES 256
#define MINI_APP_CACHE_KEY_MAX     256

typedef struct MiniAppCacheEntry {
    char     key[MINI_APP_CACHE_KEY_MAX];
    uint8_t *data;
    size_t   data_len;
    uint64_t created_at;
    uint64_t expires_at;
    uint64_t last_access;
    uint64_t stale_if_error;
    int      access_count;
    char     etag[64];
    char     content_type[128];
    char     cache_tag[128];
    bool     valid;
} MiniAppCacheEntry;

typedef struct {
    MiniAppCacheEntry entries[MINI_APP_CACHE_MAX_ENTRIES];
    int    entry_count;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    size_t  max_memory;
    size_t  used_memory;
} MiniAppContentCache;

void  mini_app_cache_init(MiniAppContentCache *cache, size_t max_memory);
bool  mini_app_cache_put(MiniAppContentCache *cache, const char *key,
                          const uint8_t *data, size_t data_len,
                          uint64_t ttl_seconds, const char *content_type);
bool  mini_app_cache_get(MiniAppContentCache *cache, const char *key,
                          uint8_t **data, size_t *data_len,
                          char *content_type, size_t ct_size);
bool  mini_app_cache_has(MiniAppContentCache *cache, const char *key);
void  mini_app_cache_invalidate(MiniAppContentCache *cache, const char *key);
void  mini_app_cache_clear(MiniAppContentCache *cache);
void  mini_app_cache_get_stats(MiniAppContentCache *cache, uint64_t *hits,
                                uint64_t *misses, uint64_t *evictions,
                                double *hit_ratio);
void  mini_app_cache_destroy(MiniAppContentCache *cache);

/* L8: Advanced cache invalidation */
typedef enum {
    CACHE_CHECK_FRESH,
    CACHE_CHECK_STALE,
    CACHE_CHECK_REVALIDATE,
    CACHE_CHECK_NOT_FOUND
} CacheCheckResult;

CacheCheckResult mini_app_cache_check(MiniAppContentCache *cache,
                                        const char *key,
                                        const char *if_none_match,
                                        const char *if_modified_since);
int  mini_app_cache_purge_by_tag(MiniAppContentCache *cache, const char *tag);
uint64_t mini_app_cache_estimate_ttl(uint64_t last_modified, uint64_t date);
int  mini_app_cache_parse_cc(const char *cc_header,
                              uint64_t *max_age,
                              bool *no_cache, bool *no_store,
                              bool *public_cache, bool *must_revalidate);

#endif
