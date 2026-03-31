/*
 * memogent_c.h — SwiftPM-local copy of bindings/c/memogent_c.h.
 *
 * Keep this file in sync with bindings/c/memogent_c.h. The canonical source
 * of truth is the top-level copy; this is duplicated here only so the Swift
 * Package is self-contained for CI builds.
 */
#ifndef MEMOGENT_C_H_
#define MEMOGENT_C_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_ABI_VERSION_MAJOR 0
#define MEM_ABI_VERSION_MINOR 1
#define MEM_ABI_VERSION_PATCH 0

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef MEM_BUILDING_SHARED
    #define MEM_API __declspec(dllexport)
  #else
    #define MEM_API __declspec(dllimport)
  #endif
#else
  #define MEM_API __attribute__((visibility("default")))
#endif

typedef enum {
    MEM_OK = 0,
    MEM_ERR_INVALID_ARGUMENT = 1,
    MEM_ERR_NOT_FOUND = 2,
    MEM_ERR_ALREADY_EXISTS = 3,
    MEM_ERR_FAILED_PRECONDITION = 4,
    MEM_ERR_OUT_OF_RANGE = 5,
    MEM_ERR_UNIMPLEMENTED = 6,
    MEM_ERR_INTERNAL = 7,
    MEM_ERR_UNAVAILABLE = 8,
    MEM_ERR_DATA_CORRUPTED = 9,
    MEM_ERR_IO = 10,
} mem_status_t;

typedef enum {
    MEM_POWER_FULL = 0,
    MEM_POWER_REDUCED = 1,
    MEM_POWER_MINIMAL = 2,
    MEM_POWER_PAUSED = 3,
} mem_power_mode_t;

typedef enum {
    MEM_EV_APP_OPEN = 0,
    MEM_EV_APP_CLOSE = 1,
    MEM_EV_APP_BACKGROUND = 2,
    MEM_EV_APP_FOREGROUND = 3,
    MEM_EV_USER_INTERACTION = 4,
    MEM_EV_MODEL_LOAD = 5,
    MEM_EV_MODEL_UNLOAD = 6,
    MEM_EV_MODEL_INVOKE = 7,
    MEM_EV_CACHE_HIT = 8,
    MEM_EV_CACHE_MISS = 9,
    MEM_EV_PRELOAD = 10,
    MEM_EV_EVICT = 11,
} mem_event_type_t;

typedef enum {
    MEM_POOL_APP_STATE = 0,
    MEM_POOL_MODEL_WEIGHTS = 1,
    MEM_POOL_KV_CACHE = 2,
    MEM_POOL_EMBEDDING = 3,
    MEM_POOL_OTHER = 255,
} mem_pool_t;

typedef struct mem_config mem_config_t;
typedef struct mem_orchestrator mem_orchestrator_t;

typedef struct {
    mem_event_type_t type;
    const char* app_id;
    const char* model_id;
    double timestamp;
    int hour_of_day;
    float battery_pct;
    float thermal;
    const char* screen_state;
    const char* location;
} mem_app_event_t;

typedef struct {
    float battery_pct;
    float thermal;
    float ram_free_mb;
    int charging;
    int low_power_mode;
    double timestamp;
} mem_device_state_t;

typedef struct {
    const char* app_id;
    float score;
    const char* reason;
} mem_prediction_t;

typedef struct {
    double app_load_time_improvement_pct;
    double launch_time_improvement_pct;
    double thrashing_reduction_pct;
    uint64_t stability_issues;
    double prediction_accuracy_top1;
    double prediction_accuracy_top3;
    double cache_hit_rate;
    double memory_utilization_efficiency_pct;
    double p50_decision_latency_ms;
    double p95_decision_latency_ms;
    double p99_decision_latency_ms;
} mem_kpi_snapshot_t;

MEM_API const char* mem_version_string(void);
MEM_API void        mem_version(int* major, int* minor, int* patch);

MEM_API mem_config_t* mem_config_create(void);
MEM_API void          mem_config_destroy(mem_config_t* cfg);
MEM_API mem_status_t  mem_config_set_string(mem_config_t* cfg, const char* key, const char* value);
MEM_API mem_status_t  mem_config_set_uint  (mem_config_t* cfg, const char* key, uint64_t value);
MEM_API mem_status_t  mem_config_set_float (mem_config_t* cfg, const char* key, double value);

MEM_API mem_status_t  mem_orchestrator_create(const mem_config_t* cfg, mem_orchestrator_t** out);
MEM_API void          mem_orchestrator_destroy(mem_orchestrator_t* orch);

MEM_API mem_status_t  mem_orchestrator_record_event(mem_orchestrator_t* orch, const mem_app_event_t* ev);
MEM_API mem_status_t  mem_orchestrator_record_app_open (mem_orchestrator_t* orch, const char* app_id);
MEM_API mem_status_t  mem_orchestrator_record_app_close(mem_orchestrator_t* orch, const char* app_id);
MEM_API mem_status_t  mem_orchestrator_record_cache_hit (mem_orchestrator_t* orch, mem_pool_t p);
MEM_API mem_status_t  mem_orchestrator_record_cache_miss(mem_orchestrator_t* orch, mem_pool_t p);
MEM_API mem_status_t  mem_orchestrator_update_device_state(mem_orchestrator_t* orch, const mem_device_state_t* state);
MEM_API size_t        mem_orchestrator_predict_next(mem_orchestrator_t* orch, mem_prediction_t* out, size_t cap);
MEM_API mem_power_mode_t mem_orchestrator_power_mode(mem_orchestrator_t* orch);
MEM_API mem_status_t  mem_orchestrator_tick(mem_orchestrator_t* orch);
MEM_API mem_kpi_snapshot_t mem_orchestrator_kpis(mem_orchestrator_t* orch);
MEM_API const char*   mem_orchestrator_kpis_json(mem_orchestrator_t* orch);
MEM_API mem_status_t  mem_orchestrator_reset(mem_orchestrator_t* orch);

#ifdef __cplusplus
}
#endif

#endif
