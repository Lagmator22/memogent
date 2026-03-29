// SPDX-License-Identifier: MIT
#include "memogent_c.h"

#include <memogent/memogent.hpp>

#include <cstring>
#include <memory>
#include <mutex>
#include <string>

namespace {

struct Config {
    mem::Config cfg;
};

struct Orchestrator {
    std::unique_ptr<mem::Orchestrator> inner;
    std::string kpis_json_cache;
    std::mutex mu;
};

mem_status_t to_status(const mem::Error& e) {
    switch (e.code) {
        case mem::ErrorCode::Ok: return MEM_OK;
        case mem::ErrorCode::InvalidArgument: return MEM_ERR_INVALID_ARGUMENT;
        case mem::ErrorCode::NotFound: return MEM_ERR_NOT_FOUND;
        case mem::ErrorCode::AlreadyExists: return MEM_ERR_ALREADY_EXISTS;
        case mem::ErrorCode::FailedPrecondition: return MEM_ERR_FAILED_PRECONDITION;
        case mem::ErrorCode::OutOfRange: return MEM_ERR_OUT_OF_RANGE;
        case mem::ErrorCode::Unimplemented: return MEM_ERR_UNIMPLEMENTED;
        case mem::ErrorCode::Internal: return MEM_ERR_INTERNAL;
        case mem::ErrorCode::Unavailable: return MEM_ERR_UNAVAILABLE;
        case mem::ErrorCode::DataCorrupted: return MEM_ERR_DATA_CORRUPTED;
        case mem::ErrorCode::IoError: return MEM_ERR_IO;
    }
    return MEM_ERR_INTERNAL;
}

}  // namespace

extern "C" {

const char* mem_version_string(void) { return mem::version_string; }
void        mem_version(int* major, int* minor, int* patch) {
    if (major) *major = mem::version_major;
    if (minor) *minor = mem::version_minor;
    if (patch) *patch = mem::version_patch;
}

mem_config_t* mem_config_create(void) { return reinterpret_cast<mem_config_t*>(new Config()); }

void mem_config_destroy(mem_config_t* cfg) {
    delete reinterpret_cast<Config*>(cfg);
}

mem_status_t mem_config_set_string(mem_config_t* h, const char* key, const char* value) {
    if (!h || !key || !value) return MEM_ERR_INVALID_ARGUMENT;
    auto& c = reinterpret_cast<Config*>(h)->cfg;
    std::string k{key}, v{value};
    if (k == "predictor") c.predictor = v;
    else if (k == "predictor_model_path") c.predictor_model_path = v;
    else if (k == "cache_policy") c.cache_policy = v;
    else if (k == "arbiter") c.arbiter = v;
    else if (k == "llm_backend") c.llm_backend = v;
    else if (k == "llm_model_path") c.llm_model_path = v;
    else if (k == "data_dir") c.data_dir = v;
    else if (k == "session_id") c.session_id = v;
    else return MEM_ERR_INVALID_ARGUMENT;
    return MEM_OK;
}

mem_status_t mem_config_set_uint(mem_config_t* h, const char* key, uint64_t value) {
    if (!h || !key) return MEM_ERR_INVALID_ARGUMENT;
    auto& c = reinterpret_cast<Config*>(h)->cfg;
    std::string k{key};
    if (k == "cache_capacity_app") c.cache_capacity_app = value;
    else if (k == "cache_capacity_model") c.cache_capacity_model = value;
    else if (k == "kv_cache_token_budget") c.kv_cache_token_budget = value;
    else if (k == "prediction_top_k") c.prediction_top_k = static_cast<std::uint32_t>(value);
    else if (k == "preload_top_k") c.preload_top_k = static_cast<std::uint32_t>(value);
    else if (k == "ram_budget_mb") c.ram_budget_mb = value;
    else return MEM_ERR_INVALID_ARGUMENT;
    return MEM_OK;
}

mem_status_t mem_config_set_float(mem_config_t* h, const char* key, double value) {
    if (!h || !key) return MEM_ERR_INVALID_ARGUMENT;
    auto& c = reinterpret_cast<Config*>(h)->cfg;
    std::string k{key};
    if (k == "qos_thermal_reduced") c.qos_thermal_reduced = static_cast<float>(value);
    else if (k == "qos_thermal_minimal") c.qos_thermal_minimal = static_cast<float>(value);
    else if (k == "qos_thermal_paused") c.qos_thermal_paused = static_cast<float>(value);
    else if (k == "qos_battery_reduced") c.qos_battery_reduced = static_cast<float>(value);
    else if (k == "qos_battery_minimal") c.qos_battery_minimal = static_cast<float>(value);
    else if (k == "qos_battery_paused") c.qos_battery_paused = static_cast<float>(value);
    else return MEM_ERR_INVALID_ARGUMENT;
    return MEM_OK;
}

mem_status_t mem_orchestrator_create(const mem_config_t* cfg_handle,
                                     mem_orchestrator_t** out) {
    if (!out) return MEM_ERR_INVALID_ARGUMENT;
    mem::Config cfg;
    if (cfg_handle) {
        cfg = reinterpret_cast<const Config*>(cfg_handle)->cfg;
    }
    auto r = mem::Orchestrator::create(std::move(cfg));
    if (!r) return to_status(r.error());
    auto* wrapped = new Orchestrator();
    wrapped->inner = std::move(r).value();
    *out = reinterpret_cast<mem_orchestrator_t*>(wrapped);
    return MEM_OK;
}

void mem_orchestrator_destroy(mem_orchestrator_t* orch) {
    delete reinterpret_cast<Orchestrator*>(orch);
}

static mem::AppEvent translate(const mem_app_event_t& e) {
    mem::AppEvent out;
    out.type = static_cast<mem::EventType>(e.type);
    if (e.app_id) out.app_id = e.app_id;
    if (e.model_id) out.model_id = e.model_id;
    out.timestamp = e.timestamp;
    if (e.hour_of_day >= 0) out.hour_of_day = e.hour_of_day;
    out.battery_pct = e.battery_pct;
    out.thermal = e.thermal;
    if (e.screen_state) out.screen_state = e.screen_state;
    if (e.location) out.location = e.location;
    return out;
}

mem_status_t mem_orchestrator_record_event(mem_orchestrator_t* orch, const mem_app_event_t* ev) {
    if (!orch || !ev) return MEM_ERR_INVALID_ARGUMENT;
    auto* W = reinterpret_cast<Orchestrator*>(orch);
    W->inner->record_event(translate(*ev));
    return MEM_OK;
}

mem_status_t mem_orchestrator_record_app_open(mem_orchestrator_t* orch, const char* app_id) {
    if (!orch || !app_id) return MEM_ERR_INVALID_ARGUMENT;
    reinterpret_cast<Orchestrator*>(orch)->inner->record_app_open(app_id);
    return MEM_OK;
}

mem_status_t mem_orchestrator_record_app_close(mem_orchestrator_t* orch, const char* app_id) {
    if (!orch || !app_id) return MEM_ERR_INVALID_ARGUMENT;
    reinterpret_cast<Orchestrator*>(orch)->inner->record_app_close(app_id);
    return MEM_OK;
}

mem_status_t mem_orchestrator_record_cache_hit(mem_orchestrator_t* orch, mem_pool_t p) {
    if (!orch) return MEM_ERR_INVALID_ARGUMENT;
    reinterpret_cast<Orchestrator*>(orch)->inner->record_cache_hit(
        static_cast<mem::ResourcePool>(p));
    return MEM_OK;
}

mem_status_t mem_orchestrator_record_cache_miss(mem_orchestrator_t* orch, mem_pool_t p) {
    if (!orch) return MEM_ERR_INVALID_ARGUMENT;
    reinterpret_cast<Orchestrator*>(orch)->inner->record_cache_miss(
        static_cast<mem::ResourcePool>(p));
    return MEM_OK;
}

mem_status_t mem_orchestrator_update_device_state(mem_orchestrator_t* orch,
                                                  const mem_device_state_t* s) {
    if (!orch || !s) return MEM_ERR_INVALID_ARGUMENT;
    mem::DeviceState d;
    d.battery_pct = s->battery_pct;
    d.thermal = s->thermal;
    d.ram_free_mb = s->ram_free_mb;
    d.charging = s->charging != 0;
    d.low_power_mode = s->low_power_mode != 0;
    d.timestamp = s->timestamp;
    reinterpret_cast<Orchestrator*>(orch)->inner->update_device_state(d);
    return MEM_OK;
}

size_t mem_orchestrator_predict_next(mem_orchestrator_t* orch, mem_prediction_t* out, size_t cap) {
    if (!orch || !out || cap == 0) return 0;
    auto* W = reinterpret_cast<Orchestrator*>(orch);
    auto ps = W->inner->predict_next(cap);
    std::lock_guard g(W->mu);
    const size_t n = std::min(cap, ps.size());
    // stash strings inside the wrapper so the caller's pointers stay valid.
    static thread_local std::vector<std::string> apps_cache;
    static thread_local std::vector<std::string> reasons_cache;
    apps_cache.resize(n); reasons_cache.resize(n);
    for (size_t i = 0; i < n; ++i) {
        apps_cache[i] = ps[i].app_id;
        reasons_cache[i] = ps[i].reason;
        out[i].app_id = apps_cache[i].c_str();
        out[i].score  = ps[i].score;
        out[i].reason = reasons_cache[i].c_str();
    }
    return n;
}

mem_power_mode_t mem_orchestrator_power_mode(mem_orchestrator_t* orch) {
    if (!orch) return MEM_POWER_FULL;
    return static_cast<mem_power_mode_t>(
        reinterpret_cast<Orchestrator*>(orch)->inner->current_power_mode());
}

mem_status_t mem_orchestrator_tick(mem_orchestrator_t* orch) {
    if (!orch) return MEM_ERR_INVALID_ARGUMENT;
    (void)reinterpret_cast<Orchestrator*>(orch)->inner->tick();
    return MEM_OK;
}

mem_kpi_snapshot_t mem_orchestrator_kpis(mem_orchestrator_t* orch) {
    mem_kpi_snapshot_t k{};
    if (!orch) return k;
    const auto s = reinterpret_cast<Orchestrator*>(orch)->inner->kpis();
    k.app_load_time_improvement_pct = s.app_load_time_improvement_pct;
    k.launch_time_improvement_pct = s.launch_time_improvement_pct;
    k.thrashing_reduction_pct = s.thrashing_reduction_pct;
    k.stability_issues = s.stability_issues;
    k.prediction_accuracy_top1 = s.prediction_accuracy_top1;
    k.prediction_accuracy_top3 = s.prediction_accuracy_top3;
    k.cache_hit_rate = s.cache_hit_rate;
    k.memory_utilization_efficiency_pct = s.memory_utilization_efficiency_pct;
    k.p50_decision_latency_ms = s.p50_decision_latency_ms;
    k.p95_decision_latency_ms = s.p95_decision_latency_ms;
    k.p99_decision_latency_ms = s.p99_decision_latency_ms;
    return k;
}

const char* mem_orchestrator_kpis_json(mem_orchestrator_t* orch) {
    if (!orch) return "";
    auto* W = reinterpret_cast<Orchestrator*>(orch);
    std::lock_guard g(W->mu);
    W->kpis_json_cache = W->inner->kpis_json();
    return W->kpis_json_cache.c_str();
}

mem_status_t mem_orchestrator_reset(mem_orchestrator_t* orch) {
    if (!orch) return MEM_ERR_INVALID_ARGUMENT;
    reinterpret_cast<Orchestrator*>(orch)->inner->reset();
    return MEM_OK;
}

}  // extern "C"
