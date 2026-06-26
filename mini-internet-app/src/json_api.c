#include "json_api.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

MiniAppJsonNode *mini_app_json_create_null(void)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node) node->type = MINI_APP_JSON_NULL;
    return node;
}

MiniAppJsonNode *mini_app_json_create_bool(bool value)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node) { node->type = MINI_APP_JSON_BOOL; node->value.bool_val = value; }
    return node;
}

MiniAppJsonNode *mini_app_json_create_number(double value)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node) { node->type = MINI_APP_JSON_NUMBER; node->value.num_val = value; }
    return node;
}

MiniAppJsonNode *mini_app_json_create_string(const char *value)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node && value) {
        node->type = MINI_APP_JSON_STRING;
        node->value.str_val = strdup(value);
    }
    return node;
}

MiniAppJsonNode *mini_app_json_create_array(void)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node) { node->type = MINI_APP_JSON_ARRAY; node->value.array.capacity = 8; }
    return node;
}

MiniAppJsonNode *mini_app_json_create_object(void)
{
    MiniAppJsonNode *node = (MiniAppJsonNode *)calloc(1, sizeof(MiniAppJsonNode));
    if (node) { node->type = MINI_APP_JSON_OBJECT; node->value.object.capacity = 8; }
    return node;
}

void mini_app_json_array_add(MiniAppJsonNode *arr, MiniAppJsonNode *item)
{
    if (!arr || arr->type != MINI_APP_JSON_ARRAY || !item) return;
    if (arr->value.array.count >= arr->value.array.capacity) {
        int new_cap = arr->value.array.capacity * 2;
        MiniAppJsonNode **new_items = (MiniAppJsonNode **)realloc(
            arr->value.array.items, (size_t)new_cap * sizeof(MiniAppJsonNode *));
        if (!new_items) return;
        arr->value.array.items = new_items;
        arr->value.array.capacity = new_cap;
    }
    if (!arr->value.array.items) {
        arr->value.array.items = (MiniAppJsonNode **)calloc(
            (size_t)arr->value.array.capacity, sizeof(MiniAppJsonNode *));
    }
    arr->value.array.items[arr->value.array.count++] = item;
}

void mini_app_json_object_set(MiniAppJsonNode *obj, const char *key, MiniAppJsonNode *val)
{
    if (!obj || obj->type != MINI_APP_JSON_OBJECT || !key || !val) return;
    val->key = strdup(key);

    /* Check for existing key */
    for (int i = 0; i < obj->value.object.count; i++) {
        if (obj->value.object.members[i]->key &&
            strcmp(obj->value.object.members[i]->key, key) == 0) {
            mini_app_json_free(obj->value.object.members[i]);
            obj->value.object.members[i] = val;
            return;
        }
    }

    if (obj->value.object.count >= obj->value.object.capacity) {
        int new_cap = obj->value.object.capacity * 2;
        MiniAppJsonNode **new_members = (MiniAppJsonNode **)realloc(
            obj->value.object.members, (size_t)new_cap * sizeof(MiniAppJsonNode *));
        if (!new_members) return;
        obj->value.object.members = new_members;
        obj->value.object.capacity = new_cap;
    }
    if (!obj->value.object.members) {
        obj->value.object.members = (MiniAppJsonNode **)calloc(
            (size_t)obj->value.object.capacity, sizeof(MiniAppJsonNode *));
    }
    obj->value.object.members[obj->value.object.count++] = val;
}

MiniAppJsonNode *mini_app_json_object_get(MiniAppJsonNode *obj, const char *key)
{
    if (!obj || obj->type != MINI_APP_JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < obj->value.object.count; i++) {
        if (obj->value.object.members[i]->key &&
            strcmp(obj->value.object.members[i]->key, key) == 0) {
            return obj->value.object.members[i];
        }
    }
    return NULL;
}

static int mini_app_json_serialize_internal(const MiniAppJsonNode *node,
                                              char *buf, size_t buf_size)
{
    if (!node || !buf || buf_size == 0) return 0;

    switch (node->type) {
    case MINI_APP_JSON_NULL:
        return snprintf(buf, buf_size, "null");
    case MINI_APP_JSON_BOOL:
        return snprintf(buf, buf_size, "%s", node->value.bool_val ? "true" : "false");
    case MINI_APP_JSON_NUMBER:
        if (floor(node->value.num_val) == node->value.num_val) {
            return snprintf(buf, buf_size, "%.0f", node->value.num_val);
        }
        return snprintf(buf, buf_size, "%g", node->value.num_val);
    case MINI_APP_JSON_STRING: {
        int written = snprintf(buf, buf_size, "\"");
        if (written < 0) return 0;
        char *p = buf + written;
        size_t rem = buf_size - (size_t)written;
        const char *s = node->value.str_val ? node->value.str_val : "";
        while (*s && rem > 2) {
            if (*s == '"' || *s == '\\') { *p++ = '\\'; *p++ = *s; rem -= 2; written += 2; }
            else if (*s == '\n') { *p++ = '\\'; *p++ = 'n'; rem -= 2; written += 2; }
            else if (*s == '\t') { *p++ = '\\'; *p++ = 't'; rem -= 2; written += 2; }
            else { *p++ = *s; rem--; written++; }
            s++;
        }
        if (rem > 1) { *p++ = '"'; *p = '\0'; written++; }
        return written;
    }
    case MINI_APP_JSON_ARRAY: {
        int written = snprintf(buf, buf_size, "[");
        if (written < 0) return 0;
        char *p = buf + written;
        size_t rem = buf_size - (size_t)written;
        for (int i = 0; i < node->value.array.count; i++) {
            if (i > 0 && rem > 1) { *p++ = ','; rem--; written++; }
            int n = mini_app_json_serialize_internal(
                node->value.array.items[i], p, rem);
            if (n <= 0) return written;
            p += n; rem -= (size_t)n; written += n;
        }
        if (rem > 1) { *p++ = ']'; *p = '\0'; written++; }
        return written;
    }
    case MINI_APP_JSON_OBJECT: {
        int written = snprintf(buf, buf_size, "{");
        if (written < 0) return 0;
        char *p = buf + written;
        size_t rem = buf_size - (size_t)written;
        for (int i = 0; i < node->value.object.count; i++) {
            if (i > 0 && rem > 1) { *p++ = ','; rem--; written++; }
            MiniAppJsonNode *m = node->value.object.members[i];
            int n = snprintf(p, rem, "\"%s\":", m->key ? m->key : "");
            if (n < 0) return written;
            p += n; rem -= (size_t)n; written += n;
            n = mini_app_json_serialize_internal(m, p, rem);
            if (n <= 0) return written;
            p += n; rem -= (size_t)n; written += n;
        }
        if (rem > 1) { *p++ = '}'; *p = '\0'; written++; }
        return written;
    }
    }
    return 0;
}

int mini_app_json_serialize(const MiniAppJsonNode *node, char *buf, size_t buf_size)
{
    int n = mini_app_json_serialize_internal(node, buf, buf_size);
    if (n > 0 && (size_t)n < buf_size) buf[n] = '\0';
    return n;
}

/* Simple JSON parser (recursive descent) */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static MiniAppJsonNode *parse_value(const char **json);

static MiniAppJsonNode *parse_string(const char **json)
{
    const char *p = *json;
    if (*p != '"') return NULL;
    p++;
    char buf[4096];
    int idx = 0;
    while (*p && *p != '"' && idx < 4095) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
            case '"':  buf[idx++] = '"';  break;
            case '\\': buf[idx++] = '\\'; break;
            case 'n':  buf[idx++] = '\n'; break;
            case 't':  buf[idx++] = '\t'; break;
            case '/':  buf[idx++] = '/';  break;
            default:   buf[idx++] = *p;   break;
            }
        } else {
            buf[idx++] = *p;
        }
        p++;
    }
    buf[idx] = '\0';
    if (*p == '"') p++;
    *json = p;
    return mini_app_json_create_string(buf);
}

static MiniAppJsonNode *parse_number(const char **json)
{
    const char *p = *json;
    double val = 0.0;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    while (isdigit((unsigned char)*p)) { val = val * 10.0 + (double)(*p - '0'); p++; }
    if (*p == '.') {
        p++;
        double frac = 0.1;
        while (isdigit((unsigned char)*p)) {
            val += (double)(*p - '0') * frac;
            frac *= 0.1; p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        int esign = 1;
        if (*p == '-') { esign = -1; p++; }
        else if (*p == '+') p++;
        int exp = 0;
        while (isdigit((unsigned char)*p)) { exp = exp * 10 + (*p - '0'); p++; }
        double pow10 = 1.0;
        for (int i = 0; i < exp; i++) pow10 *= 10.0;
        if (esign < 0) val /= pow10; else val *= pow10;
    }
    *json = p;
    return mini_app_json_create_number(val * (double)sign);
}

static MiniAppJsonNode *parse_array(const char **json)
{
    const char *p = *json;
    if (*p != '[') return NULL;
    p++;
    MiniAppJsonNode *arr = mini_app_json_create_array();
    p = skip_ws(p);
    if (*p == ']') { *json = p + 1; return arr; }
    while (*p) {
        MiniAppJsonNode *item = parse_value(&p);
        if (item) mini_app_json_array_add(arr, item);
        p = skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p == ',') p++;
        p = skip_ws(p);
    }
    *json = p;
    return arr;
}

static MiniAppJsonNode *parse_object(const char **json)
{
    const char *p = *json;
    if (*p != '{') return NULL;
    p++;
    MiniAppJsonNode *obj = mini_app_json_create_object();
    p = skip_ws(p);
    if (*p == '}') { *json = p + 1; return obj; }
    while (*p) {
        p = skip_ws(p);
        char key[256] = {0};
        if (*p == '"') {
            MiniAppJsonNode *k = parse_string(&p);
            if (k && k->value.str_val) {
                strncpy(key, k->value.str_val, 255);
            }
            mini_app_json_free(k);
        }
        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        MiniAppJsonNode *val = parse_value(&p);
        if (val && key[0]) mini_app_json_object_set(obj, key, val);
        else if (val) { mini_app_json_free(val); }
        p = skip_ws(p);
        if (*p == '}') { p++; break; }
        if (*p == ',') p++;
    }
    *json = p;
    return obj;
}

static MiniAppJsonNode *parse_value(const char **json)
{
    const char *p = skip_ws(*json);
    if (!*p) return NULL;
    if (*p == '"') return parse_string(json);
    if (*p == '{') return parse_object(json);
    if (*p == '[') return parse_array(json);
    if (strncmp(p, "true", 4) == 0)  { *json = p + 4; return mini_app_json_create_bool(true); }
    if (strncmp(p, "false", 5) == 0) { *json = p + 5; return mini_app_json_create_bool(false); }
    if (strncmp(p, "null", 4) == 0)  { *json = p + 4; return mini_app_json_create_null(); }
    return parse_number(json);
}

MiniAppJsonNode *mini_app_json_parse(const char *json)
{
    if (!json) return NULL;
    return parse_value(&json);
}

void mini_app_json_free(MiniAppJsonNode *node)
{
    if (!node) return;
    free(node->key);
    switch (node->type) {
    case MINI_APP_JSON_STRING:
        free(node->value.str_val);
        break;
    case MINI_APP_JSON_ARRAY:
        for (int i = 0; i < node->value.array.count; i++) {
            mini_app_json_free(node->value.array.items[i]);
        }
        free(node->value.array.items);
        break;
    case MINI_APP_JSON_OBJECT:
        for (int i = 0; i < node->value.object.count; i++) {
            mini_app_json_free(node->value.object.members[i]);
        }
        free(node->value.object.members);
        break;
    default:
        break;
    }
    free(node);
}
