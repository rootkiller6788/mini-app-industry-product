#include "profiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* profile_type_str(ProfileType t) {
    switch (t) {
    case PROF_CPU:    return "cpu";
    case PROF_MEMORY: return "memory";
    case PROF_ALLOC:  return "alloc";
    case PROF_IO:     return "io";
    default:          return "???";
    }
}

Profiler* profiler_create(void) {
    Profiler *p = (Profiler*)calloc(1, sizeof(Profiler));
    if (!p) return NULL;
    p->zone_count = 0;
    p->active_zone_count = 0;
    p->total_cpu_ms = 0.0;
    p->total_wall_ms = 0.0;
    memset(&p->memory, 0, sizeof(MemoryStats));
    p->started_at = time(NULL);
    p->running = false;
    return p;
}

void profiler_destroy(Profiler *p) { free(p); }

int profiler_begin_zone(Profiler *p, const char *name, ProfileType t) {
    if (!p || !name || p->zone_count >= MAX_PROFILE_ZONES) return -1;
    ProfileZone *z = &p->zones[p->zone_count];
    memset(z, 0, sizeof(ProfileZone));
    snprintf(z->name, sizeof(z->name), "%s", name);
    z->type = t;
    z->start_time = time(NULL);
    z->active = true;
    z->call_count = 0;
    p->active_zone_count++;
    p->zone_count++;
    return p->zone_count - 1;
}

int profiler_end_zone(Profiler *p, int zone_id) {
    if (!p || zone_id < 0 || zone_id >= p->zone_count) return -1;
    ProfileZone *z = &p->zones[zone_id];
    if (!z->active) return -2;
    z->end_time = time(NULL);
    z->duration_ms = difftime(z->end_time, z->start_time) * 1000.0;
    z->call_count++;
    z->active = false;
    p->active_zone_count--;
    if (z->type == PROF_CPU)
        p->total_cpu_ms += z->duration_ms;
    p->total_wall_ms += z->duration_ms;
    return 0;
}

int profiler_record_allocation(Profiler *p, uint64_t bytes) {
    if (!p) return -1;
    p->memory.total_allocated += bytes;
    p->memory.current_usage += bytes;
    p->memory.allocation_count++;
    if (p->memory.current_usage > p->memory.peak_usage)
        p->memory.peak_usage = p->memory.current_usage;
    return 0;
}

int profiler_record_free(Profiler *p, uint64_t bytes) {
    if (!p) return -1;
    p->memory.total_freed += bytes;
    if (p->memory.current_usage >= bytes)
        p->memory.current_usage -= bytes;
    else
        p->memory.current_usage = 0;
    p->memory.free_count++;
    return 0;
}

int profiler_start(Profiler *p) {
    if (!p) return -1;
    p->running = true;
    p->started_at = time(NULL);
    return 0;
}

int profiler_stop(Profiler *p) {
    if (!p) return -1;
    p->running = false;
    for (int i = 0; i < p->zone_count; i++)
        if (p->zones[i].active) profiler_end_zone(p, i);
    return 0;
}

int profiler_reset(Profiler *p) {
    if (!p) return -1;
    p->zone_count = 0;
    p->active_zone_count = 0;
    p->total_cpu_ms = 0.0;
    p->total_wall_ms = 0.0;
    memset(&p->memory, 0, sizeof(MemoryStats));
    p->running = false;
    return 0;
}

double profiler_zone_duration_ms(const Profiler *p, int zone_id) {
    if (!p || zone_id < 0 || zone_id >= p->zone_count) return 0.0;
    return p->zones[zone_id].duration_ms;
}

int profiler_zone_call_count(const Profiler *p, int zone_id) {
    if (!p || zone_id < 0 || zone_id >= p->zone_count) return 0;
    return p->zones[zone_id].call_count;
}

MemoryStats profiler_memory_stats(const Profiler *p) {
    MemoryStats zero = {0};
    return p ? p->memory : zero;
}

double profiler_cpu_utilization(const Profiler *p) {
    if (!p || p->total_wall_ms <= 0) return 0.0;
    return (p->total_cpu_ms / p->total_wall_ms) * 100.0;
}

double profiler_memory_usage_mb(const Profiler *p) {
    if (!p) return 0.0;
    return p->memory.current_usage / (1024.0 * 1024.0);
}

void profiler_generate_flamegraph(const Profiler *p, char *buf, int buf_sz) {
    if (!p || !buf) return;
    int off = 0;
    for (int i = 0; i < p->zone_count; i++) {
        const ProfileZone *z = &p->zones[i];
        if (z->duration_ms > 0) {
            int bar_len = (int)(z->duration_ms / 10.0);
            if (bar_len > 80) bar_len = 80;
            if (bar_len < 1) bar_len = 1;
            off += snprintf(buf + off, buf_sz - off, "%-20s [%s] ", z->name, profile_type_str(z->type));
            for (int j = 0; j < bar_len; j++)
                off += snprintf(buf + off, buf_sz - off, "=");
            off += snprintf(buf + off, buf_sz - off, " %.2fms (%d calls)%s",
                           z->duration_ms, z->call_count, "\n");
        }
    }
    if (off < buf_sz) buf[off] = '\0';
}

void profiler_dump(const Profiler *p, char *buf, int buf_sz) {
    if (!p || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Profiler ===\n");
    off += snprintf(buf + off, buf_sz - off, "Running: %s  Zones: %d/%d\n",
                    p->running ? "yes" : "no", p->active_zone_count, p->zone_count);
    off += snprintf(buf + off, buf_sz - off, "CPU: %.2fms  Wall: %.2fms  Util: %.1f%%\n",
                    p->total_cpu_ms, p->total_wall_ms, profiler_cpu_utilization(p));
    off += snprintf(buf + off, buf_sz - off,
                    "Memory: %.2fMB (peak: %.2fMB)  Allocs: %d  Frees: %d\n",
                    profiler_memory_usage_mb(p),
                    p->memory.peak_usage / (1024.0 * 1024.0),
                    p->memory.allocation_count, p->memory.free_count);
    profiler_generate_flamegraph(p, buf + off, buf_sz - off);
}


int profiler_compare_zones(const Profiler *p, int a_id, int b_id) {
    if (!p || a_id < 0 || a_id >= p->zone_count || b_id < 0 || b_id >= p->zone_count)
        return 0;
    double diff = p->zones[a_id].duration_ms - p->zones[b_id].duration_ms;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

int profiler_find_hottest_zone(const Profiler *p) {
    if (!p || p->zone_count == 0) return -1;
    int hottest = 0;
    for (int i = 1; i < p->zone_count; i++)
        if (p->zones[i].duration_ms > p->zones[hottest].duration_ms)
            hottest = i;
    return hottest;
}

double profiler_total_allocated_mb(const Profiler *p) {
    if (!p) return 0.0;
    return p->memory.total_allocated / (1024.0 * 1024.0);
}

double profiler_memory_leak_mb(const Profiler *p) {
    if (!p) return 0.0;
    int64_t leaked = (int64_t)p->memory.total_allocated - (int64_t)p->memory.total_freed;
    return leaked > 0 ? leaked / (1024.0 * 1024.0) : 0.0;
}

int profiler_zone_count_by_type(const Profiler *p, ProfileType t) {
    if (!p) return 0;
    int count = 0;
    for (int i = 0; i < p->zone_count; i++)
        if (p->zones[i].type == t) count++;
    return count;
}

int profiler_get_top_zones(const Profiler *p, int top_n, int *zone_ids, int max_ids) {
    if (!p || !zone_ids || top_n <= 0 || max_ids <= 0) return 0;
    int count = 0;
    typedef struct { int id; double dur; } ZSort;
    ZSort sorted[MAX_PROFILE_ZONES];
    for (int i = 0; i < p->zone_count; i++) {
        sorted[i].id = i;
        sorted[i].dur = p->zones[i].duration_ms;
    }
    for (int i = 0; i < p->zone_count - 1; i++)
        for (int j = i + 1; j < p->zone_count; j++)
            if (sorted[j].dur > sorted[i].dur) {
                ZSort tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
    for (int i = 0; i < top_n && i < p->zone_count && i < max_ids; i++)
        zone_ids[count++] = sorted[i].id;
    return count;
}
