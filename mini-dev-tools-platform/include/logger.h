#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_LOG_MSG_LEN    1024
#define MAX_LOG_FILE_PATH   512
#define MAX_LOG_MODULE_NAME  64
#define MAX_LOG_SINKS        8

typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO  = 2,
    LOG_WARN  = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5
} LogLevel;

typedef enum {
    SINK_CONSOLE = 0,
    SINK_FILE    = 1,
    SINK_SYSLOG  = 2,
    SINK_RINGBUF = 3,
    SINK_UDP     = 4,
    SINK_JSON    = 5
} SinkType;

typedef struct {
    time_t    timestamp;
    LogLevel  level;
    char      module[MAX_LOG_MODULE_NAME];
    char      file[128];
    int       line;
    char      message[MAX_LOG_MSG_LEN];
    uint64_t  sequence_id;
} LogEntry;

typedef struct {
    SinkType type;
    char     path[MAX_LOG_FILE_PATH];
    char     host[64];
    int      port;
    bool     enabled;
    LogLevel min_level;
    bool     colorized;
    int      entry_count;
    LogEntry ring_buffer[256];
    int      ring_pos;
} LogSink;

typedef struct {
    LogSink  sinks[MAX_LOG_SINKS];
    int      sink_count;
    LogLevel global_level;
    uint64_t sequence;
    bool     initialized;
    bool     async_mode;
} Logger;

Logger*    logger_create(void);
void       logger_destroy(Logger *l);
int        logger_add_sink(Logger *l, SinkType type, const char *path, LogLevel min_lvl);
int        logger_set_level(Logger *l, LogLevel lvl);
void       logger_log(Logger *l, LogLevel lvl, const char *module, const char *file, int line, const char *fmt, ...);
uint64_t   logger_entry_count(const Logger *l);
int        logger_flush(Logger *l);
int        logger_rotate(Logger *l, const char *sink_path);
const char* log_level_str(LogLevel l);
const char* sink_type_str(SinkType s);
void       logger_dump(const Logger *l, char *buf, int buf_sz);
int        logger_set_color(Logger *l, bool enabled);
int        logger_get_ring_buffer(Logger *l, const char *sink_path, LogEntry *out, int max_entries);
int        logger_set_async(Logger *l, bool async);
bool       logger_would_log(const Logger *l, LogLevel lvl);
int        logger_clear_ring_buffer(Logger *l, const char *sink_path);
int        logger_sink_count_by_type(const Logger *l, SinkType type);
int        logger_load_config(Logger *l, const char *config_file);
int        logger_format_entry(const LogEntry *e, char *buf, int buf_sz);
LogLevel   logger_parse_level(const char *str);
void       logger_log_with_errno(Logger *l, LogLevel lvl, const char *module, const char *file, int line, const char *fmt, ...);

#define LOG_TRACE_F(l, mod, fmt, ...) logger_log(l, LOG_TRACE, mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_F(l, mod, fmt, ...) logger_log(l, LOG_DEBUG, mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO_F(l, mod, fmt, ...)  logger_log(l, LOG_INFO,  mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN_F(l, mod, fmt, ...)  logger_log(l, LOG_WARN,  mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR_F(l, mod, fmt, ...) logger_log(l, LOG_ERROR, mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL_F(l, mod, fmt, ...) logger_log(l, LOG_FATAL, mod, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
