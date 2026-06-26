#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

const char* log_level_str(LogLevel l) {
    switch (l) {
    case LOG_TRACE: return "TRACE";
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    case LOG_FATAL: return "FATAL";
    default:        return "???";
    }
}

const char* sink_type_str(SinkType s) {
    switch (s) {
    case SINK_CONSOLE: return "console";
    case SINK_FILE:    return "file";
    case SINK_SYSLOG:  return "syslog";
    case SINK_RINGBUF: return "ringbuf";
    case SINK_UDP:     return "udp";
    case SINK_JSON:    return "json";
    default:           return "???";
    }
}

Logger* logger_create(void) {
    Logger *l = (Logger*)calloc(1, sizeof(Logger));
    if (!l) return NULL;
    l->sink_count = 0;
    l->global_level = LOG_INFO;
    l->sequence = 0;
    l->initialized = true;
    l->async_mode = false;
    return l;
}

void logger_destroy(Logger *l) {
    if (!l) return;
    for (int i = 0; i < l->sink_count; i++) {
        if (l->sinks[i].type == SINK_FILE) logger_flush(l);
    }
    free(l);
}

int logger_add_sink(Logger *l, SinkType type, const char *path, LogLevel min_lvl) {
    if (!l || l->sink_count >= MAX_LOG_SINKS) return -1;
    LogSink *s = &l->sinks[l->sink_count];
    memset(s, 0, sizeof(LogSink));
    s->type = type;
    s->enabled = true;
    s->min_level = min_lvl;
    s->colorized = (type == SINK_CONSOLE);
    s->entry_count = 0;
    s->ring_pos = 0;
    if (path) snprintf(s->path, sizeof(s->path), "%s", path);
    l->sink_count++;
    return l->sink_count - 1;
}

int logger_set_level(Logger *l, LogLevel lvl) {
    if (!l) return -1;
    l->global_level = lvl;
    return 0;
}

static const char* level_colors[] = {"","","","\33[33m","\33[31m","\33[35m"};
static const char* color_reset = "\33[0m";

static void write_to_sink(LogSink *s, const LogEntry *e) {
    if (!s->enabled || e->level < s->min_level) return;
    if (s->type == SINK_CONSOLE) {
        const char *color = "";
        const char *reset = "";
        if (s->colorized && e->level >= LOG_WARN && e->level <= LOG_FATAL) {
            color = level_colors[e->level];
            reset = color_reset;
        }
        printf("%s[%s] %s %s:%d] %s%s\n",
               color, log_level_str(e->level), e->module, e->file,
               e->line, e->message, reset);
    } else if (s->type == SINK_RINGBUF) {
        s->ring_buffer[s->ring_pos % 256] = *e;
        s->ring_pos++;
        s->entry_count++;
    } else if (s->type == SINK_JSON) {
        printf("{\"level\":\"%s\",\"module\":\"%s\",\"msg\":\"%s\"}\n",
               log_level_str(e->level), e->module, e->message);
    }
}

void logger_log(Logger *l, LogLevel lvl, const char *module, const char *file,
                int line, const char *fmt, ...) {
    if (!l || lvl < l->global_level) return;
    LogEntry e;
    memset(&e, 0, sizeof(LogEntry));
    e.timestamp = time(NULL);
    e.level = lvl;
    e.line = line;
    e.sequence_id = l->sequence++;
    if (module) snprintf(e.module, sizeof(e.module), "%s", module);
    if (file)   snprintf(e.file, sizeof(e.file), "%s", file);
    va_list args;
    va_start(args, fmt);
    vsnprintf(e.message, sizeof(e.message), fmt, args);
    va_end(args);
    for (int i = 0; i < l->sink_count; i++) write_to_sink(&l->sinks[i], &e);
}

uint64_t logger_entry_count(const Logger *l) {
    return l ? l->sequence : 0;
}

int logger_flush(Logger *l) {
    if (!l) return -1;
    for (int i = 0; i < l->sink_count; i++) {
        if (l->sinks[i].type == SINK_FILE) l->sinks[i].entry_count = 0;
    }
    return 0;
}

int logger_rotate(Logger *l, const char *sink_path) {
    if (!l || !sink_path) return -1;
    for (int i = 0; i < l->sink_count; i++) {
        if (strcmp(l->sinks[i].path, sink_path) == 0) {
            l->sinks[i].entry_count = 0;
            l->sinks[i].ring_pos = 0;
            return 0;
        }
    }
    return -1;
}

void logger_dump(const Logger *l, char *buf, int buf_sz) {
    if (!l || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Logger ===\n");
    off += snprintf(buf + off, buf_sz - off,
                    "Level: %s  Sinks: %d  Seq: %lu\n",
                    log_level_str(l->global_level), l->sink_count,
                    (unsigned long)l->sequence);
    for (int i = 0; i < l->sink_count; i++)
        off += snprintf(buf + off, buf_sz - off,
                        "  [%d] %s -> %s (%s)\n",
                        i, sink_type_str(l->sinks[i].type),
                        l->sinks[i].path,
                        l->sinks[i].enabled ? "on" : "off");
    if (off < buf_sz) buf[off] = 0;
}


int logger_set_color(Logger *l, bool enabled) {
    if (!l) return -1;
    for (int i = 0; i < l->sink_count; i++)
        l->sinks[i].colorized = enabled;
    return 0;
}

int logger_get_ring_buffer(Logger *l, const char *sink_path, LogEntry *out, int max_entries) {
    if (!l || !out || max_entries <= 0) return 0;
    for (int i = 0; i < l->sink_count; i++) {
        if (l->sinks[i].type == SINK_RINGBUF && strcmp(l->sinks[i].path, sink_path) == 0) {
            int count = l->sinks[i].entry_count;
            if (count > max_entries) count = max_entries;
            int start = l->sinks[i].ring_pos - count;
            if (start < 0) start = 0;
            for (int j = 0; j < count; j++)
                out[j] = l->sinks[i].ring_buffer[(start + j) % 256];
            return count;
        }
    }
    return 0;
}

int logger_set_async(Logger *l, bool async) {
    if (!l) return -1;
    l->async_mode = async;
    return 0;
}

bool logger_would_log(const Logger *l, LogLevel lvl) {
    return l && lvl >= l->global_level && l->sink_count > 0;
}

int logger_clear_ring_buffer(Logger *l, const char *sink_path) {
    if (!l) return -1;
    for (int i = 0; i < l->sink_count; i++) {
        if (l->sinks[i].type == SINK_RINGBUF &&
            (!sink_path || strcmp(l->sinks[i].path, sink_path) == 0)) {
            l->sinks[i].entry_count = 0;
            l->sinks[i].ring_pos = 0;
        }
    }
    return 0;
}

int logger_sink_count_by_type(const Logger *l, SinkType type) {
    if (!l) return 0;
    int count = 0;
    for (int i = 0; i < l->sink_count; i++)
        if (l->sinks[i].type == type) count++;
    return count;
}

LogLevel logger_parse_level(const char *str) {
    if (!str) return LOG_INFO;
    if (strcasecmp(str, "trace") == 0) return LOG_TRACE;
    if (strcasecmp(str, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(str, "info") == 0)  return LOG_INFO;
    if (strcasecmp(str, "warn") == 0)  return LOG_WARN;
    if (strcasecmp(str, "error") == 0) return LOG_ERROR;
    if (strcasecmp(str, "fatal") == 0) return LOG_FATAL;
    return LOG_INFO;
}

int logger_load_config(Logger *l, const char *config_file) {
    if (!l || !config_file) return -1;
    FILE *fp = fopen(config_file, "r");
    if (!fp) return -1;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "level") == 0) {
            LogLevel lvl = logger_parse_level(eq + 1);
            logger_set_level(l, lvl);
        } else if (strcmp(line, "sink") == 0) {
            char *type = eq + 1;
            char *path = strchr(type, ',');
            if (path) { *path = '\0'; path++; }
            SinkType st = SINK_CONSOLE;
            if (strcmp(type, "file") == 0) st = SINK_FILE;
            else if (strcmp(type, "json") == 0) st = SINK_JSON;
            else if (strcmp(type, "ringbuf") == 0) st = SINK_RINGBUF;
            logger_add_sink(l, st, path ? path : "", LOG_INFO);
        }
    }
    fclose(fp);
    return 0;
}

int logger_format_entry(const LogEntry *e, char *buf, int buf_sz) {
    if (!e || !buf) return -1;
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", localtime(&e->timestamp));
    return snprintf(buf, buf_sz, "[%s] [%s] [%s:%d] %s",
                    timestr, log_level_str(e->level), e->module, e->line, e->message);
}

void logger_log_with_errno(Logger *l, LogLevel lvl, const char *module,
                            const char *file, int line, const char *fmt, ...) {
    if (!l || lvl < l->global_level) return;
    char buf[MAX_LOG_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    int saved_errno = errno;
    char full_msg[MAX_LOG_MSG_LEN + 256];
    snprintf(full_msg, sizeof(full_msg), "%s (errno=%d: %s)", buf, saved_errno, strerror(saved_errno));
    logger_log(l, lvl, module, file, line, "%s", full_msg);
}
