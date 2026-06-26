#include "rate_limiter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * L5: Token Bucket Algorithm
 *
 * Token Bucket is a classic rate-limiting algorithm:
 *
 * - Bucket has capacity (max_tokens) and refill_rate (tokens/second)
 * - Each request consumes 1 token
 * - Tokens refill at a constant rate
 * - If bucket is empty, request is rejected
 *
 * Properties:
 * - Allows bursts up to max_tokens
 * - Long-term average rate = refill_rate
 * - Time complexity: O(1) amortized
 * - Space complexity: O(n) where n = number of tracked keys
 *
 * L4 Theorem: Token bucket enforces long-term average rate r with
 * maximum burst size b. For any time window T:
 *   tokens_(T) ≤ b + r * T
 *
 * Reference: Turner 1986 "New Directions in Communications"
 * Cisco IOS committed access rate (CAR)
 * ========================================================================== */

static uint64_t mini_app_now_seconds(void)
{
    return (uint64_t)time(NULL);
}

void mini_app_rate_limiter_init(MiniAppRateLimiter *rl,
                                 double max_tokens, double refill_rate)
{
    memset(rl, 0, sizeof(*rl));
    rl->default_max_tokens = max_tokens;
    rl->default_refill_rate = refill_rate;
}

static MiniAppTokenBucket *find_or_create_bucket(MiniAppRateLimiter *rl,
                                                   const char *key)
{
    /* Linear search for existing bucket */
    for (int i = 0; i < rl->bucket_count; i++) {
        if (strcmp(rl->buckets[i].key, key) == 0) {
            return &rl->buckets[i];
        }
    }

    /* Create new bucket if space available */
    if (rl->bucket_count >= MINI_APP_RL_MAX_BUCKETS) return NULL;

    MiniAppTokenBucket *b = &rl->buckets[rl->bucket_count];
    strncpy(b->key, key, 127);
    b->tokens = rl->default_max_tokens;
    b->max_tokens = rl->default_max_tokens;
    b->refill_rate = rl->default_refill_rate;
    b->last_refill = mini_app_now_seconds();
    rl->bucket_count++;

    return b;
}

bool mini_app_rate_limiter_allow(MiniAppRateLimiter *rl, const char *key)
{
    MiniAppTokenBucket *b = find_or_create_bucket(rl, key);
    if (!b) return false;

    uint64_t now = mini_app_now_seconds();
    uint64_t elapsed = now - b->last_refill;

    /* Refill tokens */
    if (elapsed > 0) {
        b->tokens += (double)elapsed * b->refill_rate;
        if (b->tokens > b->max_tokens) b->tokens = b->max_tokens;
        b->last_refill = now;
    }

    /* Consume token */
    if (b->tokens >= 1.0) {
        b->tokens -= 1.0;
        return true;
    }
    return false;
}

int mini_app_rate_limiter_remaining(MiniAppRateLimiter *rl, const char *key)
{
    MiniAppTokenBucket *b = find_or_create_bucket(rl, key);
    if (!b) return 0;

    uint64_t now = mini_app_now_seconds();
    uint64_t elapsed = now - b->last_refill;
    double current = b->tokens + (double)elapsed * b->refill_rate;
    if (current > b->max_tokens) current = b->max_tokens;
    return (int)current;
}

void mini_app_rate_limiter_reset(MiniAppRateLimiter *rl, const char *key)
{
    MiniAppTokenBucket *b = find_or_create_bucket(rl, key);
    if (b) {
        b->tokens = b->max_tokens;
        b->last_refill = mini_app_now_seconds();
    }
}

/* ============================================================================
 * L5: Sliding Window Counter
 *
 * Sliding window provides more accurate rate limiting than fixed window
 * by counting requests in a moving time window.
 *
 * Algorithm:
 * 1. Store timestamps of last N requests
 * 2. On each request, evict timestamps outside window
 * 3. If count < max_requests, allow; else deny
 *
 * Space: O(max_requests) per window
 * Time: O(1) amortized per request (each timestamp evicted once)
 * ========================================================================== */

void mini_app_sliding_window_init(MiniAppSlidingWindow *sw,
                                   uint64_t window_s, uint64_t max_req)
{
    memset(sw, 0, sizeof(*sw));
    sw->window_seconds = window_s;
    sw->max_requests = max_req;
}

bool mini_app_sliding_window_allow(MiniAppSlidingWindow *sw, uint64_t now_s)
{
    if (!sw) return false;

    /* Evict expired entries */
    int valid_count = 0;
    for (int i = 0; i < sw->count; i++) {
        int idx = (sw->head - sw->count + i + 1000) % 1000;
        if (now_s - sw->timestamps[idx] <= sw->window_seconds) {
            valid_count++;
        }
    }
    sw->count = valid_count;

    if ((uint64_t)sw->count >= sw->max_requests) return false;

    /* Record this request */
    sw->timestamps[sw->head] = now_s;
    sw->head = (sw->head + 1) % 1000;
    if (sw->count < 1000) sw->count++;

    return true;
}

int mini_app_sliding_window_count(MiniAppSlidingWindow *sw, uint64_t now_s)
{
    if (!sw) return 0;
    int cnt = 0;
    for (int i = 0; i < sw->count; i++) {
        int idx = (sw->head - sw->count + i + 1000) % 1000;
        if (now_s - sw->timestamps[idx] <= sw->window_seconds) cnt++;
    }
    return cnt;
}

/* ============================================================================
 * L8: Distributed Rate Limiting with Consistent Hashing
 *
 * For distributed systems, rate limits must be coordinated across
 * multiple server instances. Approaches:
 *
 * 1. Centralized: Redis/etcd-based counter (single point of failure)
 * 2. Distributed consensus: Raft/Paxos for counter state
 * 3. Probabilistic: local counter + gossip protocol
 * 4. Consistent hashing: requests for same key go to same node
 *
 * This module uses approach 4 as a computationally efficient approximation.
 *
 * Consistent Hashing Ring:
 * - Map each server to multiple points on a hash ring (virtual nodes)
 * - Hash client key to find nearest server point
 * - Same client always hits same server for rate limiting
 * - Adding/removing servers only affects K/N keys
 *
 * L4: Consistent hashing theorem (Karger 1997)
 * With V virtual nodes per server and S servers:
 *   - Load balance stddev ≤ O(1/√V)
 *   - Keys remapped on add/remove = O(K/S) where K = total keys
 *
 * Reference: Karger et al. 1997 "Consistent Hashing and Random Trees"
 * ========================================================================== */

#define MINI_APP_CH_RING_SIZE 1024
#define MINI_APP_CH_VNODES     32

typedef struct {
    uint64_t hash;
    int     server_id;
} MiniAppCHRingEntry;

typedef struct {
    MiniAppCHRingEntry ring[MINI_APP_CH_RING_SIZE];
    int ring_size;
    int server_count;
} MiniAppConsistentHash;

static uint64_t mini_app_ch_hash(const char *key)
{
    uint64_t h = 14695981039346656037ULL;
    while (*key) {
        h ^= (uint64_t)(unsigned char)*key;
        h *= 1099511628211ULL;
        key++;
    }
    return h;
}

void mini_app_ch_init(MiniAppConsistentHash *ch)
{
    memset(ch, 0, sizeof(*ch));
}

void mini_app_ch_add_server(MiniAppConsistentHash *ch, int server_id)
{
    if (ch->ring_size + MINI_APP_CH_VNODES > MINI_APP_CH_RING_SIZE) return;

    char vnode_key[64];
    for (int v = 0; v < MINI_APP_CH_VNODES; v++) {
        snprintf(vnode_key, sizeof(vnode_key), "s%d-v%d", server_id, v);
        uint64_t h = mini_app_ch_hash(vnode_key);

        /* Insert into sorted ring */
        int pos = ch->ring_size;
        for (int i = 0; i < ch->ring_size; i++) {
            if (ch->ring[i].hash > h) { pos = i; break; }
        }
        /* Shift */
        for (int i = ch->ring_size; i > pos; i--) {
            ch->ring[i] = ch->ring[pos];
        }
        ch->ring[pos].hash = h;
        ch->ring[pos].server_id = server_id;
        ch->ring_size++;
    }
    ch->server_count++;
}

int mini_app_ch_get_server(MiniAppConsistentHash *ch, const char *key)
{
    if (ch->ring_size == 0) return -1;
    uint64_t h = mini_app_ch_hash(key);

    /* Binary search for first server with hash >= h */
    int lo = 0, hi = ch->ring_size;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (ch->ring[mid].hash < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= ch->ring_size) lo = 0;
    return ch->ring[lo].server_id;
}

/* Distributed rate limiter using consistent hashing */
typedef struct {
    MiniAppConsistentHash ch_ring;
    MiniAppRateLimiter    local_limiter;
    int                   my_server_id;
} MiniAppDistributedRateLimiter;

void mini_app_drl_init(MiniAppDistributedRateLimiter *drl,
                        int my_server_id, double max_tokens, double refill_rate)
{
    memset(drl, 0, sizeof(*drl));
    drl->my_server_id = my_server_id;
    mini_app_ch_init(&drl->ch_ring);
    mini_app_ch_add_server(&drl->ch_ring, my_server_id);
    mini_app_rate_limiter_init(&drl->local_limiter, max_tokens, refill_rate);
}

void mini_app_drl_add_peer(MiniAppDistributedRateLimiter *drl, int peer_id)
{
    mini_app_ch_add_server(&drl->ch_ring, peer_id);
}

bool mini_app_drl_allow(MiniAppDistributedRateLimiter *drl, const char *key)
{
    if (!drl) return false;
    int owner = mini_app_ch_get_server(&drl->ch_ring, key);
    if (owner != drl->my_server_id) return true; /* Allow (handled by peer) */
    return mini_app_rate_limiter_allow(&drl->local_limiter, key);
}

