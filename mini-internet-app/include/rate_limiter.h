#ifndef MINI_APP_RATE_LIMITER_H
#define MINI_APP_RATE_LIMITER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Token Bucket rate limiter for API protection */

#define MINI_APP_RL_MAX_BUCKETS 1024

typedef struct {
    char     key[128];          /* Client identifier (IP, user_id, etc.) */
    double   tokens;            /* Current token count */
    double   max_tokens;        /* Bucket capacity */
    double   refill_rate;       /* Tokens per second */
    uint64_t last_refill;       /* Last refill timestamp (seconds) */
} MiniAppTokenBucket;

typedef struct {
    MiniAppTokenBucket buckets[MINI_APP_RL_MAX_BUCKETS];
    int    bucket_count;
    double default_max_tokens;
    double default_refill_rate;
} MiniAppRateLimiter;

void mini_app_rate_limiter_init(MiniAppRateLimiter *rl,
                                 double max_tokens, double refill_rate);
bool mini_app_rate_limiter_allow(MiniAppRateLimiter *rl,
                                  const char *key);
int  mini_app_rate_limiter_remaining(MiniAppRateLimiter *rl,
                                      const char *key);
void mini_app_rate_limiter_reset(MiniAppRateLimiter *rl, const char *key);

/* Sliding window counter for more precise rate limiting */
typedef struct {
    uint64_t timestamps[1000];
    int      head;
    int      count;
    uint64_t window_seconds;
    uint64_t max_requests;
} MiniAppSlidingWindow;

void mini_app_sliding_window_init(MiniAppSlidingWindow *sw,
                                   uint64_t window_s, uint64_t max_req);
bool mini_app_sliding_window_allow(MiniAppSlidingWindow *sw, uint64_t now_s);
int  mini_app_sliding_window_count(MiniAppSlidingWindow *sw, uint64_t now_s);

#endif
