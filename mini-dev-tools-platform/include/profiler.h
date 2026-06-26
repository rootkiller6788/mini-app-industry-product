#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_PROFILE_ZONES    128
#define MAX_ZONE_NAME_LEN     64
#define MAX_THREAD_NAME_LEN   32
#define MAX_TRACE_EVENTS     4096

typedef enum {
    PROF_CPU     = 0,
    PROF_MEMORY  = 1,
    PROF_ALLOC   = 2,
    PROF_IO      = 3
} ProfileType;

typedef struct {
    char   name[MAX_ZONE_NAME_LEN];
    char   thread[MAX_THREAD_NAME_LEN];
    time_t start_time;
    time_t end_time;
    double duration_ms;
    uint64_t memory_allocated_bytes;
    uint64_t memory_freed_bytes;
    uint64_t io_bytes_read;
    uint64_t io_bytes_written;
    int     call_count;
    ProfileType type;
    bool    active;
} ProfileZone;

typedef struct {
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t current_usage;
    uint64_t peak_usage;
    int      allocation_count;
    int      free_count;
} MemoryStats;

typedef struct {
    ProfileZone zones[MAX_PROFILE_ZONES];
    int     zone_count;
    int     active_zone_count;
    double  total_cpu_ms;
    double  total_wall_ms;
    MemoryStats memory;
    time_t  started_at;
    bool    running;
} Profiler;

Profiler*   profiler_create(void);
void        profiler_destroy(Profiler *p);
int         profiler_begin_zone(Profiler *p, const char *name, ProfileType t);
int         profiler_end_zone(Profiler *p, int zone_id);
int         profiler_record_allocation(Profiler *p, uint64_t bytes);
int         profiler_record_free(Profiler *p, uint64_t bytes);
int         profiler_start(Profiler *p);
int         profiler_stop(Profiler *p);
int         profiler_reset(Profiler *p);
double      profiler_zone_duration_ms(const Profiler *p, int zone_id);
int         profiler_zone_call_count(const Profiler *p, int zone_id);
MemoryStats profiler_memory_stats(const Profiler *p);
double      profiler_cpu_utilization(const Profiler *p);
double      profiler_memory_usage_mb(const Profiler *p);
int         profiler_compare_zones(const Profiler *p, int a_id, int b_id);
int         profiler_find_hottest_zone(const Profiler *p);
double      profiler_total_allocated_mb(const Profiler *p);
double      profiler_memory_leak_mb(const Profiler *p);
int         profiler_zone_count_by_type(const Profiler *p, ProfileType t);
int         profiler_get_top_zones(const Profiler *p, int top_n, int *zone_ids, int max_ids);
void        profiler_generate_flamegraph(const Profiler *p, char *buf, int buf_sz);
const char* profile_type_str(ProfileType t);
void        profiler_dump(const Profiler *p, char *buf, int buf_sz);

#endif
