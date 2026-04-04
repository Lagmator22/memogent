// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/adaptive_cache.hpp>
#include <memogent/app_event.hpp>
#include <memogent/arbiter.hpp>
#include <memogent/config.hpp>
#include <memogent/device_state.hpp>
#include <memogent/kv_cache.hpp>
#include <memogent/model_swap.hpp>
#include <memogent/predictor.hpp>
#include <memogent/preloader.hpp>
#include <memogent/result.hpp>
#include <memogent/storage.hpp>
#include <memogent/telemetry.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mem {

// Public-facing single-object entry point. Thread-safe: concurrent reads
// allowed, writes serialized via an internal mutex. Host apps construct one
// Orchestrator per session / user and keep it alive.
class Orchestrator {
public:
    // Factory creates + wires every subsystem per cfg. Never throws.
    static Result<std::unique_ptr<Orchestrator>> create(Config cfg);

    ~Orchestrator();

    // Ingest one observation from the host OS.
    void record_event(const AppEvent& ev);

    // Short-cuts around record_event.
    void record_app_open(const AppId& id);
    void record_app_close(const AppId& id);
    void record_model_load(const ModelId& id, std::size_t bytes);
    void record_cache_hit(ResourcePool pool);
    void record_cache_miss(ResourcePool pool);

    // Update the device state. Bindings should call this ~1 Hz.
    void update_device_state(const DeviceState& state);

    // Predict the next-k apps given current history.
    std::vector<Prediction> predict_next(std::size_t k = 3);

    // Produce an allocation plan for this tick and apply it internally.
    ArbiterPlan tick();

    // Snapshot the KPI accumulator.
    KpiSnapshot kpis() const;
    std::string kpis_json() const;

    // Diagnostics.
    CacheStats app_cache_stats() const;
    CacheStats model_cache_stats() const;
    PowerMode current_power_mode() const;

    // Lifecycle.
    void reset();  // wipe everything in a single transaction.

    // Accessors for advanced use cases (e.g., custom dashboards).
    Predictor& predictor();
    AdaptiveCache& app_cache();
    AdaptiveCache& model_cache();
    KVCacheManager& kv_cache();
    ModelSwapManager& model_swap();
    Preloader& preloader();
    Arbiter& arbiter();
    Telemetry& telemetry();
    Storage& storage();
    const Config& config() const;

private:
    explicit Orchestrator(Config cfg);
    Result<bool> init();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mem
