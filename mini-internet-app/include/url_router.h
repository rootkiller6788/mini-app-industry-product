#ifndef MINI_APP_URL_ROUTER_H
#define MINI_APP_URL_ROUTER_H

#include <stdbool.h>
#include <stddef.h>

/* URL Router with Trie-based path matching.
 * Supports: exact match, wildcard (*), parameter capture (:param), and ** glob. */

#define MINI_APP_ROUTER_MAX_SEGMENTS 32
#define MINI_APP_ROUTER_MAX_PARAMS   8
#define MINI_APP_ROUTER_MAX_CHILDREN 64

typedef struct {
    char key[64];
    char value[256];
} MiniAppUrlParam;

typedef struct MiniAppTrieNode {
    char segment[128];
    bool is_wildcard;
    bool is_param;
    bool is_terminal;
    int  handler_id;
    struct MiniAppTrieNode *children[MINI_APP_ROUTER_MAX_CHILDREN];
    int  child_count;
} MiniAppTrieNode;

typedef struct {
    MiniAppTrieNode *root;
    int node_count;
} MiniAppUrlRouter;

void mini_app_router_init(MiniAppUrlRouter *router);
void mini_app_router_free(MiniAppUrlRouter *router);
int  mini_app_router_add(MiniAppUrlRouter *router, const char *pattern, int handler_id);
int  mini_app_router_match(MiniAppUrlRouter *router, const char *url,
                            MiniAppUrlParam *params, int max_params);
bool mini_app_url_decode(const char *src, char *dst, size_t dst_size);
bool mini_app_url_encode(const char *src, char *dst, size_t dst_size);

#endif
