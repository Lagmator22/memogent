// SPDX-License-Identifier: MIT
#include <memogent/orchestrator.hpp>

#include <memogent/logger.hpp>

#include <chrono>
#include <mutex>

namespace mem {

struct Orchestrator::Impl {
    Config cfg;
    std::unique_ptr<Predictor> predictor;
    std::unique_ptr<AdaptiveCache> app_cache;
    std::unique_ptr<AdaptiveCache> model_cache;
    std::unique_ptr<KVCacheManager> kv;
    std::unique_ptr<ModelSwapManager> swap;
    std::unique_ptr<Preloader> preloader;
    std::unique_ptr<Arbiter> arbiter;
    std::unique_ptr<Telemetry> telemetry;
    std::unique_ptr<Storage> storage;
    SystemClock clock;
    DeviceState device{};
    PowerMode mode = PowerMode::Full;
    std::mutex mu;
};

Result<std::unique_ptr<Orchestrator>> Orchestrator::create(Config cfg) {
    std::unique_ptr<Orchestrator> self(new Orchestrator(std::move(cfg)));
    if (auto r = self->init(); !r) return r.error();
    return std::move(self);
}

Orchestrator::Orchestrator(Config cfg) : impl_(std::make_unique<Impl>()) {
    impl_->cfg = std::move(cfg);
}

Orchestrator::~Orchestrator() = default;

Result<bool> Orchestrator::init() {
    auto& I = *impl_;
    log::init();

    if (auto r = make_predictor(I.cfg); r) I.predictor = std::move(r).value();
    else return r.error();

    if (auto r = make_cache(I.cfg, I.cfg.cache_capacity_app); r) I.app_cache = std::move(r).value();
    else return r.error();

    if (auto r = make_cache(I.cfg, I.cfg.cache_capacity_model); r) I.model_cache = std::move(r).value();
    else return r.error();

    if (auto r = make_kv_cache_manager(I.cfg); r) I.kv = std::move(r).value();
    else return r.error();

    if (auto r = make_model_swap_manager(I.cfg); r) I.swap = std::move(r).value();
    else return r.error();

    if (auto r = make_preloader(I.cfg); r) I.preloader = std::move(r).value();
    else return r.error();

    if (auto r = make_arbiter(I.cfg); r) I.arbiter = std::move(r).value();
    else return r.error();

    if (auto r = make_telemetry(I.cfg); r) I.telemetry = std::move(r).value();
    else return r.error();

    if (auto r = open_storage(I.cfg); r) I.storage = std::move(r).value();
    else return r.error();

    return true;
}

void Orchestrator::record_event(const AppEvent& ev) {
    std::lock_guard g(impl_->mu);
    impl_->predictor->observe(ev);
    impl_->storage->append_event(ev);
}

void Orchestrator::record_app_open(const AppId& id) {
    AppEvent e; e.type = EventType::AppOpen; e.app_id = id;
    e.timestamp = impl_->clock.now();
    record_event(e);
}

void Orchestrator::record_app_close(const AppId& id) {
    AppEvent e; e.type = EventType::AppClose; e.app_id = id;
    e.timestamp = impl_->clock.now();
    record_event(e);
}

void Orchestrator::record_model_load(const ModelId& id, std::size_t bytes) {
    std::lock_guard g(impl_->mu);
    ModelSlot s{id, bytes, true, impl_->clock.now(), 0};
    impl_->swap->register_model(s);
}

void Orchestrator::record_cache_hit(ResourcePool pool) {
    impl_->telemetry->record_event("cache_hit", {{"pool", resource_pool_name(pool)}});
}

void Orchestrator::record_cache_miss(ResourcePool pool) {
    impl_->telemetry->record_event("cache_miss", {{"pool", resource_pool_name(pool)}});
}

void Orchestrator::update_device_state(const DeviceState& state) {
    std::lock_guard g(impl_->mu);
    impl_->device = state;
    impl_->mode = classify(state, impl_->cfg);
}

std::vector<Prediction> Orchestrator::predict_next(std::size_t k) {
    std::lock_guard g(impl_->mu);
    return impl_->predictor->predict(k);
}

ArbiterPlan Orchestrator::tick() {
    using clk = std::chrono::steady_clock;
    const auto t0 = clk::now();
    std::lock_guard g(impl_->mu);

    ArbiterInputs in{};
    in.device = impl_->device;
    in.mode = impl_->mode;
    in.predictions = impl_->predictor->predict(impl_->cfg.prediction_top_k);
    in.apps_resident = impl_->app_cache->stats().size;
    in.models_resident = impl_->model_cache->stats().size;
    in.kv_tokens = impl_->kv->total_tokens();
    in.app_cache = impl_->app_cache->stats();
    in.model_cache = impl_->model_cache->stats();

    auto plan = impl_->arbiter->decide(in);
    impl_->app_cache->resize(plan.app_cache_capacity);
    impl_->model_cache->resize(plan.model_cache_capacity);

    if (plan.allow_preload) {
        impl_->app_cache->hint_future(in.predictions);
        const auto p = impl_->preloader->plan(in.predictions, in.mode,
                                              impl_->cfg.ram_budget_mb);
        impl_->preloader->execute(p, *impl_->app_cache);
    }

    auto kv_plan = impl_->kv->plan(plan.kv_cache_token_budget);
    for (auto id : kv_plan.evict) impl_->kv->release(id);

    const auto t1 = clk::now();
    const auto ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    impl_->telemetry->record_latency("decision", ms);
    impl_->telemetry->record_cache("app",
                                   impl_->app_cache->stats().hits,
                                   impl_->app_cache->stats().misses);
    impl_->telemetry->record_cache("model",
                                   impl_->model_cache->stats().hits,
                                   impl_->model_cache->stats().misses);
    return plan;
}

KpiSnapshot Orchestrator::kpis() const { return impl_->telemetry->snapshot(); }
std::string Orchestrator::kpis_json() const { return impl_->telemetry->to_json(); }
CacheStats Orchestrator::app_cache_stats() const { return impl_->app_cache->stats(); }
CacheStats Orchestrator::model_cache_stats() const { return impl_->model_cache->stats(); }
PowerMode Orchestrator::current_power_mode() const { return impl_->mode; }

void Orchestrator::reset() {
    std::lock_guard g(impl_->mu);
    impl_->predictor->reset();
    impl_->app_cache->clear();
    impl_->model_cache->clear();
    impl_->kv->reset();
    impl_->storage->reset();
}

Predictor& Orchestrator::predictor() { return *impl_->predictor; }
AdaptiveCache& Orchestrator::app_cache() { return *impl_->app_cache; }
AdaptiveCache& Orchestrator::model_cache() { return *impl_->model_cache; }
KVCacheManager& Orchestrator::kv_cache() { return *impl_->kv; }
ModelSwapManager& Orchestrator::model_swap() { return *impl_->swap; }
Preloader& Orchestrator::preloader() { return *impl_->preloader; }
Arbiter& Orchestrator::arbiter() { return *impl_->arbiter; }
Telemetry& Orchestrator::telemetry() { return *impl_->telemetry; }
Storage& Orchestrator::storage() { return *impl_->storage; }
const Config& Orchestrator::config() const { return impl_->cfg; }

}  // namespace mem
