#ifndef MINI_APP_JSON_API_H
#define MINI_APP_JSON_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Lightweight JSON parser/builder for REST API responses */

typedef enum {
    MINI_APP_JSON_NULL    = 0,
    MINI_APP_JSON_BOOL    = 1,
    MINI_APP_JSON_NUMBER  = 2,
    MINI_APP_JSON_STRING  = 3,
    MINI_APP_JSON_ARRAY   = 4,
    MINI_APP_JSON_OBJECT  = 5,
} MiniAppJsonType;

typedef struct MiniAppJsonNode {
    MiniAppJsonType type;
    char   *key;
    union {
        bool   bool_val;
        double num_val;
        char   *str_val;
        struct {
            struct MiniAppJsonNode **items;
            int count;
            int capacity;
        } array;
        struct {
            struct MiniAppJsonNode **members;
            int count;
            int capacity;
        } object;
    } value;
} MiniAppJsonNode;

MiniAppJsonNode *mini_app_json_create_null(void);
MiniAppJsonNode *mini_app_json_create_bool(bool value);
MiniAppJsonNode *mini_app_json_create_number(double value);
MiniAppJsonNode *mini_app_json_create_string(const char *value);
MiniAppJsonNode *mini_app_json_create_array(void);
MiniAppJsonNode *mini_app_json_create_object(void);

void mini_app_json_array_add(MiniAppJsonNode *arr, MiniAppJsonNode *item);
void mini_app_json_object_set(MiniAppJsonNode *obj, const char *key, MiniAppJsonNode *val);
MiniAppJsonNode *mini_app_json_object_get(MiniAppJsonNode *obj, const char *key);

int  mini_app_json_serialize(const MiniAppJsonNode *node, char *buf, size_t buf_size);
MiniAppJsonNode *mini_app_json_parse(const char *json);

void mini_app_json_free(MiniAppJsonNode *node);

#endif
