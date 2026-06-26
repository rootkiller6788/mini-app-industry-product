#include "config_mgr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

const char* config_type_str(ConfigType t) {
    switch (t) {
    case CFG_STRING: return "string";
    case CFG_INT:    return "int";
    case CFG_FLOAT:  return "float";
    case CFG_BOOL:   return "bool";
    case CFG_JSON:   return "json";
    default:         return "???";
    }
}

const char* config_source_str(ConfigSource s) {
    switch (s) {
    case SOURCE_FILE:    return "file";
    case SOURCE_ENV:     return "env";
    case SOURCE_ETCD:    return "etcd";
    case SOURCE_CONSUL:  return "consul";
    case SOURCE_VAULT:   return "vault";
    case SOURCE_CMDLINE: return "cmdline";
    default:             return "???";
    }
}

ConfigManager* config_create(const char *path, bool hot_reload) {
    ConfigManager *cm = (ConfigManager*)calloc(1, sizeof(ConfigManager));
    if (!cm) return NULL;
    cm->entry_count = 0;
    cm->hot_reload = hot_reload;
    cm->poll_interval_seconds = 30;
    cm->version = 1;
    cm->initialized = true;
    if (path) snprintf(cm->file_path, sizeof(cm->file_path), "%s", path);
    return cm;
}

void config_destroy(ConfigManager *cm) { free(cm); }

static ConfigEntry* find_entry(ConfigManager *cm, const char *key) {
    for (int i = 0; i < cm->entry_count; i++)
        if (strcmp(cm->entries[i].key, key) == 0)
            return &cm->entries[i];
    return NULL;
}

int config_set(ConfigManager *cm, const char *key, const char *val,
               ConfigType t, ConfigSource src) {
    if (!cm || !key || !val) return -1;
    ConfigEntry *e = find_entry(cm, key);
    if (e) {
        snprintf(e->value, sizeof(e->value), "%s", val);
        e->type = t;
        e->source = src;
        e->dirty = true;
        e->modified_at = time(NULL);
        return 1;
    }
    if (cm->entry_count >= MAX_CONFIG_KEYS) return -1;
    e = &cm->entries[cm->entry_count];
    memset(e, 0, sizeof(ConfigEntry));
    snprintf(e->key, sizeof(e->key), "%s", key);
    snprintf(e->value, sizeof(e->value), "%s", val);
    e->type = t;
    e->source = src;
    e->loaded_at = time(NULL);
    e->modified_at = e->loaded_at;
    e->dirty = true;
    cm->entry_count++;
    cm->version++;
    return 0;
}

const char* config_get(const ConfigManager *cm, const char *key,
                        const char *default_val) {
    if (!cm || !key) return default_val;
    ConfigEntry *e = find_entry((ConfigManager*)cm, key);
    return e ? e->value : default_val;
}

int64_t config_get_int(const ConfigManager *cm, const char *key,
                         int64_t default_val) {
    const char *val = config_get(cm, key, NULL);
    if (!val) return default_val;
    char *end = NULL;
    int64_t result = (int64_t)strtoll(val, &end, 10);
    return (end && *end == '\0') ? result : default_val;
}

double config_get_float(const ConfigManager *cm, const char *key,
                          double default_val) {
    const char *val = config_get(cm, key, NULL);
    if (!val) return default_val;
    char *end = NULL;
    double result = strtod(val, &end);
    return (end && *end == '\0') ? result : default_val;
}

bool config_get_bool(const ConfigManager *cm, const char *key,
                       bool default_val) {
    const char *val = config_get(cm, key, NULL);
    if (!val) return default_val;
    if (strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0 ||
        strcasecmp(val, "1") == 0 || strcasecmp(val, "on") == 0)
        return true;
    if (strcasecmp(val, "false") == 0 || strcasecmp(val, "no") == 0 ||
        strcasecmp(val, "0") == 0 || strcasecmp(val, "off") == 0)
        return false;
    return default_val;
}

#if defined(__MINGW32__) || defined(_WIN32) || defined(_MSC_VER)
/* On Windows, environ is _environ (provided by stdlib.h) */
#define GET_ENVIRON() ((char**)_environ)
#else
extern char **environ;
#define GET_ENVIRON() environ
#endif

int config_load_env(ConfigManager *cm, const char *prefix) {
    if (!cm) return -1;
    char **env_ptr = GET_ENVIRON();
    if (!env_ptr) return 0;
    int loaded = 0;
    int prefix_len = prefix ? (int)strlen(prefix) : 0;
    for (char **env = env_ptr; *env != NULL; env++) {
        if (prefix && strncmp(*env, prefix, prefix_len) != 0) continue;
        char *eq = strchr(*env, '=');
        if (!eq) continue;
        int key_len = (int)(eq - *env);
        char key[MAX_CONFIG_KEY_LEN];
        int copy_len = key_len < MAX_CONFIG_KEY_LEN - 1 ? key_len : MAX_CONFIG_KEY_LEN - 1;
        memcpy(key, *env, copy_len);
        key[copy_len] = '\0';
        config_set(cm, key, eq + 1, CFG_STRING, SOURCE_ENV);
        loaded++;
    }
    return loaded;
}

bool config_has_key(const ConfigManager *cm, const char *key) {
    return cm && key && find_entry((ConfigManager*)cm, key) != NULL;
}

int config_reload(ConfigManager *cm) {
    if (!cm) return -1;
    config_load_file(cm, cm->file_path);
    cm->last_reload = time(NULL);
    cm->version++;
    return 0;
}

int config_save(const ConfigManager *cm) {
    if (!cm || !cm->file_path[0]) return -1;
    FILE *fp = fopen(cm->file_path, "w");
    if (!fp) return -1;
    for (int i = 0; i < cm->entry_count; i++)
        fprintf(fp, "%s = %s\n", cm->entries[i].key, cm->entries[i].value);
    fclose(fp);
    return 0;
}

int config_load_file(ConfigManager *cm, const char *path) {
    if (!cm || !path) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[MAX_CONFIG_VAL_LEN + MAX_CONFIG_KEY_LEN + 4];
    int loaded = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        while (*key == ' ' || *key == '	') key++;
        char *ke = key + strlen(key) - 1;
        while (ke > key && (*ke == ' ' || *ke == '	')) *ke-- = '\0';
        char *val = eq + 1;
        while (*val == ' ' || *val == '	') val++;
        char *ve = val + strlen(val) - 1;
        while (ve > val && (*ve == '\n' || *ve == '\n' || *ve == ' ' || *ve == '	'))
            *ve-- = '\0';
        if (*key) { config_set(cm, key, val, CFG_STRING, SOURCE_FILE); loaded++; }
    }
    fclose(fp);
    return loaded;
}

int config_diff(const ConfigManager *a, const ConfigManager *b,
                char *buf, int buf_sz) {
    if (!a || !b || !buf) return -1;
    int off = 0;
    for (int i = 0; i < a->entry_count; i++) {
        const char *bv = config_get(b, a->entries[i].key, NULL);
        if (!bv || strcmp(a->entries[i].value, bv) != 0) {
            off += snprintf(buf + off, buf_sz - off, "%s: %s -> %s\n",
                           a->entries[i].key, a->entries[i].value,
                           bv ? bv : "<missing>");
        }
    }
    if (off < buf_sz) buf[off] = '\0';
    return off;
}

int config_watch(ConfigManager *cm, int interval_s) {
    if (!cm) return -1;
    cm->poll_interval_seconds = interval_s;
    cm->hot_reload = true;
    return 0;
}

void config_dump(const ConfigManager *cm, char *buf, int buf_sz) {
    if (!cm || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Config Manager ===\n");
    off += snprintf(buf + off, buf_sz - off, "File: %s  Version: %d  Entries: %d\n",
                    cm->file_path, cm->version, cm->entry_count);
    off += snprintf(buf + off, buf_sz - off, "Hot-reload: %s  Interval: %ds\n",
                    cm->hot_reload ? "on" : "off", cm->poll_interval_seconds);
    for (int i = 0; i < cm->entry_count && i < 20; i++) {
        const ConfigEntry *e = &cm->entries[i];
        off += snprintf(buf + off, buf_sz - off, "  %s = %s%s (%s/%s)\n",
                        e->key,
                        e->secret ? "***" : e->value,
                        e->dirty ? " [dirty]" : "",
                        config_type_str(e->type),
                        config_source_str(e->source));
    }
    if (off < buf_sz) buf[off] = '\0';
}


int config_merge(ConfigManager *dst, const ConfigManager *src) {
    if (!dst || !src) return -1;
    int merged = 0;
    for (int i = 0; i < src->entry_count; i++) {
        if (!config_has_key(dst, src->entries[i].key)) {
            config_set(dst, src->entries[i].key, src->entries[i].value,
                       src->entries[i].type, src->entries[i].source);
            merged++;
        }
    }
    return merged;
}

int config_get_keys(const ConfigManager *cm, char keys[][MAX_CONFIG_KEY_LEN], int max_keys) {
    if (!cm || !keys || max_keys <= 0) return 0;
    int count = cm->entry_count < max_keys ? cm->entry_count : max_keys;
    for (int i = 0; i < count; i++)
        snprintf(keys[i], MAX_CONFIG_KEY_LEN, "%s", cm->entries[i].key);
    return count;
}

int config_count_by_source(const ConfigManager *cm, ConfigSource src) {
    if (!cm) return 0;
    int count = 0;
    for (int i = 0; i < cm->entry_count; i++)
        if (cm->entries[i].source == src) count++;
    return count;
}

int config_set_secret(ConfigManager *cm, const char *key, bool secret) {
    if (!cm || !key) return -1;
    ConfigEntry *e = find_entry(cm, key);
    if (!e) return -1;
    e->secret = secret;
    return 0;
}

int config_get_version(const ConfigManager *cm) {
    return cm ? cm->version : -1;
}

bool config_is_dirty(const ConfigManager *cm) {
    if (!cm) return false;
    for (int i = 0; i < cm->entry_count; i++)
        if (cm->entries[i].dirty) return true;
    return false;
}

int config_clear_dirty(ConfigManager *cm) {
    if (!cm) return -1;
    for (int i = 0; i < cm->entry_count; i++)
        cm->entries[i].dirty = false;
    return 0;
}

int config_export_json(const ConfigManager *cm, char *buf, int buf_sz) {
    if (!cm || !buf) return -1;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "{\n");
    for (int i = 0; i < cm->entry_count; i++) {
        const ConfigEntry *e = &cm->entries[i];
        off += snprintf(buf + off, buf_sz - off, "  \"%s\": \"%s\"%s\n",
                        e->key, e->secret ? "***" : e->value,
                        i < cm->entry_count - 1 ? "," : "");
    }
    off += snprintf(buf + off, buf_sz - off, "%s", "}\n");
    if (off < buf_sz) buf[off] = '\0';
    return off;
}
