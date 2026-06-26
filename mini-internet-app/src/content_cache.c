#include "content_cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * L5: LRU Cache Implementation
 *
 * LRU Cache evicts the least recently accessed entry when full.
 *
 * Data structure: Array with timestamp-based bookkeeping.
 * Find LRU: linear scan for smallest last_access timestamp.
 *
 * Time complexity:
 * - put: O(n) for eviction scan, O(1) for insert
 * - get: O(n) for linear key lookup
 * - has: O(n)
 *
 * For production: use doubly-linked list + hash map for O(1) operations.
 * This simple implementation is educationally focused.
 *
 * L4 Theorem: LRU page fault count ≤ k × OPT fault count + k
 * (Sleator & Tarjan 1985, competitive ratio = k)
 *
 * L6 Application: HTTP caching headers (Cache-Control, ETag, Expires).
 * RFC 7234: HTTP/1.1 Caching.
 * ========================================================================== */

static uint64_t cache_now_ms(void)
{
    return (uint64_t)time(NULL);
}

void mini_app_cache_init(MiniAppContentCache *cache, size_t max_memory)
{
    memset(cache, 0, sizeof(*cache));
    cache->max_memory = max_memory;
}

static int find_entry_idx(MiniAppContentCache *cache, const char *key)
{
    for (int i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].valid && strcmp(cache->entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_lru_idx(MiniAppContentCache *cache)
{
    int lru_idx = -1;
    uint64_t min_access = UINT64_MAX;

    for (int i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].valid && cache->entries[i].last_access < min_access) {
            min_access = cache->entries[i].last_access;
            lru_idx = i;
        }
    }
    /* Also check for expired entries */
    uint64_t now = cache_now_ms();
    for (int i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].valid && cache->entries[i].expires_at < now) {
            return i;
        }
    }
    return lru_idx;
}

static void free_entry(MiniAppContentCache *cache, int idx)
{
    if (idx < 0 || idx >= cache->entry_count) return;
    MiniAppCacheEntry *e = &cache->entries[idx];
    if (e->data) {
        cache->used_memory -= e->data_len;
        free(e->data);
    }
    memset(e, 0, sizeof(*e));
}

bool mini_app_cache_put(MiniAppContentCache *cache, const char *key,
                         const uint8_t *data, size_t data_len,
                         uint64_t ttl_seconds, const char *content_type)
{
    if (!cache || !key || !data || data_len == 0) return false;

    /* Check for duplicate key */
    int existing = find_entry_idx(cache, key);
    if (existing >= 0) {
        free_entry(cache, existing);
    }

    /* Evict if needed */
    while (cache->used_memory + data_len > cache->max_memory &&
           cache->entry_count > 0) {
        int lru = find_lru_idx(cache);
        if (lru < 0) break;
        cache->evictions++;
        free_entry(cache, lru);
        /* Compact if at end */
        if (lru == cache->entry_count - 1) {
            cache->entry_count--;
        }
    }

    /* Find a free slot */
    int slot = -1;
    for (int i = 0; i < MINI_APP_CACHE_MAX_ENTRIES; i++) {
        if (!cache->entries[i].valid) {
            if (i >= cache->entry_count) {
                slot = cache->entry_count;
                break;
            }
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        /* Evict LRU */
        int lru = find_lru_idx(cache);
        if (lru < 0) return false;
        free_entry(cache, lru);
        slot = lru;
    }

    MiniAppCacheEntry *e = &cache->entries[slot];
    memset(e, 0, sizeof(*e));

    strncpy(e->key, key, MINI_APP_CACHE_KEY_MAX - 1);
    e->data = (uint8_t *)malloc(data_len);
    if (!e->data) return false;

    memcpy(e->data, data, data_len);
    e->data_len = data_len;
    e->created_at = cache_now_ms();
    e->expires_at = e->created_at + ttl_seconds;
    e->last_access = e->created_at;
    e->access_count = 0;
    e->valid = true;
    if (content_type) strncpy(e->content_type, content_type, 127);

    /* Generate simple ETag from hash of data */
    uint64_t hash = 0;
    for (size_t i = 0; i < data_len; i++) {
        hash = hash * 31 + data[i];
    }
    snprintf(e->etag, sizeof(e->etag), "\"%llx\"", (unsigned long long)hash);

    cache->used_memory += data_len;
    if (slot >= cache->entry_count) cache->entry_count = slot + 1;
    return true;
}

bool mini_app_cache_get(MiniAppContentCache *cache, const char *key,
                         uint8_t **data, size_t *data_len,
                         char *content_type, size_t ct_size)
{
    if (!cache || !key || !data || !data_len) return false;

    int idx = find_entry_idx(cache, key);
    if (idx < 0) {
        cache->misses++;
        return false;
    }

    MiniAppCacheEntry *e = &cache->entries[idx];
    uint64_t now = cache_now_ms();

    /* Check expiry */
    if (now > e->expires_at) {
        free_entry(cache, idx);
        cache->misses++;
        return false;
    }

    /* Return data (caller does NOT own the pointer) */
    *data = e->data;
    *data_len = e->data_len;
    if (content_type && ct_size > 0 && e->content_type[0]) {
        strncpy(content_type, e->content_type, ct_size - 1);
        content_type[ct_size - 1] = '\0';
    }
    e->last_access = now;
    e->access_count++;
    cache->hits++;
    return true;
}

bool mini_app_cache_has(MiniAppContentCache *cache, const char *key)
{
    if (!cache || !key) return false;
    int idx = find_entry_idx(cache, key);
    if (idx < 0) return false;
    uint64_t now = cache_now_ms();
    if (now > cache->entries[idx].expires_at) {
        free_entry(cache, idx);
        return false;
    }
    return true;
}

void mini_app_cache_invalidate(MiniAppContentCache *cache, const char *key)
{
    if (!cache || !key) return;
    int idx = find_entry_idx(cache, key);
    if (idx >= 0) free_entry(cache, idx);
}

void mini_app_cache_clear(MiniAppContentCache *cache)
{
    if (!cache) return;
    for (int i = 0; i < cache->entry_count; i++) {
        free_entry(cache, i);
    }
    cache->entry_count = 0;
    cache->used_memory = 0;
}

void mini_app_cache_get_stats(MiniAppContentCache *cache, uint64_t *hits,
                                uint64_t *misses, uint64_t *evictions,
                                double *hit_ratio)
{
    if (!cache) return;
    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
    if (evictions) *evictions = cache->evictions;
    if (hit_ratio) {
        uint64_t total = cache->hits + cache->misses;
        *hit_ratio = total > 0 ? (double)cache->hits / (double)total : 0.0;
    }
}

/* ============================================================================
 * L8: Cache Invalidation Strategies
 *
 * Cache invalidation is one of the "two hard problems in CS"
 * (along with naming and off-by-one errors).
 *
 * Strategies (RFC 7234 §4.4):
 * 1. Time-based (TTL/Expires): simplest, suitable for static assets
 * 2. ETag validation (If-None-Match): conditional GET, no re-download
 * 3. Last-Modified (If-Modified-Since): time-based conditional
 * 4. Cache-Control directives: no-cache, no-store, must-revalidate
 * 5. Purge (active invalidation): CDN API-driven removal
 * 6. Surrogate keys (tag-based): group invalidation by content tags
 *
 * L4: Cache coherence — strong consistency requires invalidation
 * on every write (write-invalidate), eventual consistency allows
 * TTL-based staleness (trade-off per PACELC theorem).
 *
 * L9: CDN edge caching — distributed cache with hierarchical
 * topology (edge → regional → origin). Purge propagation
 * latency ≤ 5 seconds (Fastly), ≤ 15 seconds (CloudFront).
 *
 * Reference: RFC 7234, RFC 5861 (stale-if-error)
 *            NYU CS-GY 6913: Web Architecture and Protocols
 * ========================================================================== */

/* Check if cached response can be served based on request headers */
CacheCheckResult mini_app_cache_check(MiniAppContentCache *cache,
                                        const char *key,
                                        const char *if_none_match,
                                        const char *if_modified_since)
{
    if (!cache || !key) return CACHE_CHECK_NOT_FOUND;

    int idx = find_entry_idx(cache, key);
    if (idx < 0) return CACHE_CHECK_NOT_FOUND;

    MiniAppCacheEntry *e = &cache->entries[idx];
    uint64_t now = cache_now_ms();

    /* Check expiry */
    if (now > e->expires_at) {
        if (e->stale_if_error > 0 && now <= e->expires_at + e->stale_if_error) {
            return CACHE_CHECK_STALE;
        }
        return CACHE_CHECK_REVALIDATE;
    }

    /* ETag validation (RFC 7232 §2.3) */
    if (if_none_match && if_none_match[0] && e->etag[0]) {
        if (strcmp(if_none_match, e->etag) == 0 ||
            strcmp(if_none_match, "*") == 0) {
            return CACHE_CHECK_FRESH; /* 304 Not Modified */
        }
    }

    /* If-Modified-Since validation (RFC 7232 §2.2) — simplified */
    (void)if_modified_since;
    return CACHE_CHECK_FRESH;
}

/* CDN-style purge by surrogate key (tag) */
int mini_app_cache_purge_by_tag(MiniAppContentCache *cache, const char *tag)
{
    if (!cache || !tag) return 0;
    int purged = 0;
    for (int i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].valid &&
            strstr(cache->entries[i].key, tag)) {
            free_entry(cache, i);
            purged++;
        }
    }
    return purged;
}

/* TTL estimation based on content type heuristics (RFC 7234 §4.2.3)
 * Use 10% of (Date - Last-Modified) as heuristic freshness lifetime */
uint64_t mini_app_cache_estimate_ttl(uint64_t last_modified, uint64_t date)
{
    if (date <= last_modified) return 3600; /* Default: 1 hour */
    uint64_t age = date - last_modified;
    uint64_t ttl = age / 10;
    return ttl < 60 ? 60 : (ttl > 86400 ? 86400 : ttl); /* Clamp [1min, 24h] */
}

/* Cache-Control header parser */
int mini_app_cache_parse_cc(const char *cc_header,
                              uint64_t *max_age,
                              bool *no_cache, bool *no_store,
                              bool *public_cache, bool *must_revalidate)
{
    if (!cc_header) return -1;
    *max_age = 0; *no_cache = false; *no_store = false;
    *public_cache = false; *must_revalidate = false;

    if (strstr(cc_header, "no-store")) *no_store = true;
    if (strstr(cc_header, "no-cache")) *no_cache = true;
    if (strstr(cc_header, "public"))   *public_cache = true;
    if (strstr(cc_header, "must-revalidate")) *must_revalidate = true;
    if (strstr(cc_header, "private"))  *public_cache = false;

    const char *ma = strstr(cc_header, "max-age=");
    if (ma) {
        *max_age = (uint64_t)strtoull(ma + 8, NULL, 10);
    }
    return 0;
}
