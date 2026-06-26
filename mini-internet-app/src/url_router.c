#include "url_router.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

void mini_app_router_init(MiniAppUrlRouter *router)
{
    if (!router) return;
    router->root = (MiniAppTrieNode *)calloc(1, sizeof(MiniAppTrieNode));
    router->node_count = 1;
    if (router->root) {
        strcpy(router->root->segment, "/");
    }
}

void mini_app_router_free_node(MiniAppTrieNode *node)
{
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        mini_app_router_free_node(node->children[i]);
    }
    free(node);
}

void mini_app_router_free(MiniAppUrlRouter *router)
{
    if (!router) return;
    mini_app_router_free_node(router->root);
    router->root = NULL;
    router->node_count = 0;
}

static MiniAppTrieNode *find_or_create_child(MiniAppTrieNode *parent,
                                              const char *segment, bool wild, bool param)
{
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->segment, segment) == 0) {
            return parent->children[i];
        }
    }
    if (parent->child_count >= MINI_APP_ROUTER_MAX_CHILDREN) return NULL;

    MiniAppTrieNode *child = (MiniAppTrieNode *)calloc(1, sizeof(MiniAppTrieNode));
    if (!child) return NULL;
    strncpy(child->segment, segment, 127);
    child->is_wildcard = wild;
    child->is_param = param;

    parent->children[parent->child_count++] = child;
    return child;
}

int mini_app_router_add(MiniAppUrlRouter *router, const char *pattern, int handler_id)
{
    if (!router || !router->root || !pattern) return -1;

    /* Parse pattern into segments */
    char work[512];
    strncpy(work, pattern, 511);
    work[511] = '\0';

    MiniAppTrieNode *node = router->root;
    char *saveptr = NULL;
    char *segment = strtok_r(work, "/", &saveptr);

    while (segment) {
        bool is_wild = (strcmp(segment, "*") == 0);
        bool is_param = (segment[0] == ':');

        char node_name[128];
        if (is_param) {
            strncpy(node_name, segment + 1, 127); /* Store param name without ':' */
        } else {
            strncpy(node_name, segment, 127);
        }

        node = find_or_create_child(node, node_name, is_wild, is_param);
        if (!node) return -1;

        segment = strtok_r(NULL, "/", &saveptr);
    }

    node->is_terminal = true;
    node->handler_id = handler_id;
    return 0;
}

int mini_app_router_match(MiniAppUrlRouter *router, const char *url,
                           MiniAppUrlParam *params, int max_params)
{
    if (!router || !router->root || !url) return -1;

    char work[512];
    strncpy(work, url, 511);
    work[511] = '\0';

    MiniAppTrieNode *node = router->root;
    int param_count = 0;
    char *saveptr = NULL;
    char *segment = strtok_r(work, "/", &saveptr);

    while (segment && node) {
        /* Try exact match first */
        MiniAppTrieNode *matched = NULL;
        MiniAppTrieNode *param_node = NULL;
        MiniAppTrieNode *wild_node = NULL;

        for (int i = 0; i < node->child_count; i++) {
            MiniAppTrieNode *child = node->children[i];
            if (strcmp(child->segment, segment) == 0) {
                matched = child;
                break;
            }
            if (child->is_param && !param_node) param_node = child;
            if (child->is_wildcard && !wild_node) wild_node = child;
        }

        node = matched ? matched : (param_node ? param_node : wild_node);

        if (param_node == node && param_count < max_params) {
            strncpy(params[param_count].key, node->segment, 63);
            strncpy(params[param_count].value, segment, 255);
            param_count++;
        }
        if (wild_node == node) break; /* Wildcard consumes rest */

        segment = strtok_r(NULL, "/", &saveptr);
    }

    if (node && node->is_terminal) return node->handler_id;
    return -1;
}

bool mini_app_url_decode(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0) return false;
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            unsigned int c;
            sscanf(src + i + 1, "%2x", &c);
            dst[j++] = (char)c;
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
    return true;
}

bool mini_app_url_encode(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0) return false;
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            if (j + 1 < dst_size) dst[j++] = (char)c;
        } else if (c == ' ') {
            if (j + 1 < dst_size) dst[j++] = '+';
        } else {
            if (j + 3 < dst_size) {
                snprintf(dst + j, 4, "%%%02X", c);
                j += 3;
            }
        }
    }
    if (j < dst_size) dst[j] = '\0';
    return true;
}
