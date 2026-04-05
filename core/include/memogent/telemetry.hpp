// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mem {

// Samsung's 7 KPIs plus a few engineering metrics, tracked continuously.
struct KpiSnapshot {
    double app_load_time_ms_baseline = 0.0;
    double app_load_time_ms_current = 0.0;
    double app_load_time_improvement_pct = 0.0;   // >= 20 target
    double launch_time_ms_baseline = 0.0;
    double launch_time_ms_current = 0.0;
    double launch_time_improvement_pct = 0.0;      // >= 10 target
    double thrashing_events_baseline = 0.0;
    double thrashing_events_current = 0.0;
    double thrashing_reduction_pct = 0.0;           // >= 50 target
    std::uint64_t stability_issues = 0;             // == 0 target
    double prediction_accuracy_top1 = 0.0;
    double prediction_accuracy_top3 = 0.0;          // >= 0.75 target
    double cache_hit_rate = 0.0;                    // >= 0.85 target
    double memory_utilization_efficiency_pct = 0.0; // >= +30 target
    double p50_decision_latency_ms = 0.0;
    double p95_decision_latency_ms = 0.0;
    double p99_decision_latency_ms = 0.0;
};

class Telemetry {
public:
    virtual ~Telemetry() = default;

    virtual void record_event(std::string_view kind,
                              const std::unordered_map<std::string, std::string>& attrs) = 0;
    virtual void record_latency(std::string_view kind, double ms) = 0;
    virtual void record_prediction(bool top1_correct, bool top3_correct) = 0;
    virtual void record_cache(std::string_view pool, std::size_t hits,
                              std::size_t misses) = 0;
    virtual void record_thrash(std::size_t events) = 0;
    virtual void record_stability_issue(std::string_view reason) = 0;

    virtual KpiSnapshot snapshot() const = 0;
    virtual std::string to_json() const = 0;
};

Result<std::unique_ptr<Telemetry>> make_telemetry(const Config& cfg);

}  // namespace mem
