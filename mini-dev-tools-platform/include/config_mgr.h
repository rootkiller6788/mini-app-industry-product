#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CONFIG_KEYS     256
#define MAX_CONFIG_KEY_LEN   64
#define MAX_CONFIG_VAL_LEN  512
#define MAX_CONFIG_FILE_PATH 512
#define MAX_CONFIG_SOURCES   8

typedef enum {
    CFG_STRING  = 0,
    CFG_INT     = 1,
    CFG_FLOAT   = 2,
    CFG_BOOL    = 3,
    CFG_JSON    = 4
} ConfigType;

typedef enum {
    SOURCE_FILE    = 0,
    SOURCE_ENV     = 1,
    SOURCE_ETCD    = 2,
    SOURCE_CONSUL  = 3,
    SOURCE_VAULT   = 4,
    SOURCE_CMDLINE = 5
} ConfigSource;

typedef struct {
    char key[MAX_CONFIG_KEY_LEN];
    char value[MAX_CONFIG_VAL_LEN];
    ConfigType type;
    ConfigSource source;
    bool secret;
    bool dirty;
    time_t loaded_at;
    time_t modified_at;
} ConfigEntry;

typedef struct {
    ConfigEntry entries[MAX_CONFIG_KEYS];
    int  entry_count;
    char file_path[MAX_CONFIG_FILE_PATH];
    bool hot_reload;
    int  poll_interval_seconds;
    time_t last_reload;
    int  version;
    bool initialized;
} ConfigManager;

ConfigManager* config_create(const char *path, bool hot_reload);
void           config_destroy(ConfigManager *cm);
int            config_load_file(ConfigManager *cm, const char *path);
int            config_load_env(ConfigManager *cm, const char *prefix);
int            config_set(ConfigManager *cm, const char *key, const char *val, ConfigType t, ConfigSource src);
const char*    config_get(const ConfigManager *cm, const char *key, const char *default_val);
int64_t        config_get_int(const ConfigManager *cm, const char *key, int64_t default_val);
double         config_get_float(const ConfigManager *cm, const char *key, double default_val);
bool           config_get_bool(const ConfigManager *cm, const char *key, bool default_val);
int            config_reload(ConfigManager *cm);
int            config_save(const ConfigManager *cm);
bool           config_has_key(const ConfigManager *cm, const char *key);
int            config_watch(ConfigManager *cm, int interval_s);
int            config_diff(const ConfigManager *a, const ConfigManager *b, char *buf, int buf_sz);
int            config_merge(ConfigManager *dst, const ConfigManager *src);
int            config_get_keys(const ConfigManager *cm, char keys[][MAX_CONFIG_KEY_LEN], int max_keys);
int            config_count_by_source(const ConfigManager *cm, ConfigSource src);
int            config_set_secret(ConfigManager *cm, const char *key, bool secret);
int            config_get_version(const ConfigManager *cm);
bool           config_is_dirty(const ConfigManager *cm);
int            config_clear_dirty(ConfigManager *cm);
int            config_export_json(const ConfigManager *cm, char *buf, int buf_sz);
const char*    config_type_str(ConfigType t);
const char*    config_source_str(ConfigSource s);
void           config_dump(const ConfigManager *cm, char *buf, int buf_sz);

#endif
