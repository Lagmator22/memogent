// SPDX-License-Identifier: MIT
#include <memogent/telemetry.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mem {

namespace {

using json = nlohmann::json;

class InMemoryTelemetry final : public Telemetry {
public:
    explicit InMemoryTelemetry(const Config& cfg) : cfg_(cfg) {}

    void record_event(std::string_view kind,
                      const std::unordered_map<std::string, std::string>& attrs) override {
        std::lock_guard g(mu_);
        json j = {{"kind", std::string{kind}}};
        for (const auto& [k, v] : attrs) j[k] = v;
        events_.push_back(j.dump());
        trim();
    }

    void record_latency(std::string_view kind, double ms) override {
        std::lock_guard g(mu_);
        latencies_[std::string{kind}].push_back(ms);
    }

    void record_prediction(bool top1, bool top3) override {
        std::lock_guard g(mu_);
        ++predictions_total_;
        if (top1) ++predictions_top1_;
        if (top3) ++predictions_top3_;
    }

    void record_cache(std::string_view pool, std::size_t hits, std::size_t misses) override {
        std::lock_guard g(mu_);
        auto& c = cache_[std::string{pool}];
        c.first = hits; c.second = misses;
    }

    void record_thrash(std::size_t events) override {
        std::lock_guard g(mu_); thrash_events_ += events;
    }

    void record_stability_issue(std::string_view reason) override {
        std::lock_guard g(mu_);
        ++stability_issues_;
        stability_reasons_.emplace_back(reason);
    }

    KpiSnapshot snapshot() const override {
        std::lock_guard g(mu_);
        KpiSnapshot k{};
        k.prediction_accuracy_top1 = predictions_total_ == 0
            ? 0.0 : static_cast<double>(predictions_top1_) / predictions_total_;
        k.prediction_accuracy_top3 = predictions_total_ == 0
            ? 0.0 : static_cast<double>(predictions_top3_) / predictions_total_;

        std::size_t total_h = 0, total_m = 0;
        for (const auto& [_, v] : cache_) { total_h += v.first; total_m += v.second; }
        k.cache_hit_rate = (total_h + total_m) == 0 ? 0.0
            : static_cast<double>(total_h) / (total_h + total_m);

        k.stability_issues = stability_issues_;
        k.thrashing_events_current = static_cast<double>(thrash_events_);
        k.thrashing_events_baseline = baseline_thrashing_;
        k.thrashing_reduction_pct = baseline_thrashing_ == 0 ? 0.0
            : (baseline_thrashing_ - thrash_events_) * 100.0 / baseline_thrashing_;

        auto percentile = [](std::vector<double> v, double p) {
            if (v.empty()) return 0.0;
            std::sort(v.begin(), v.end());
            const auto idx = std::min<std::size_t>(v.size() - 1,
                                                   static_cast<std::size_t>(v.size() * p));
            return v[idx];
        };
        std::vector<double> decisions;
        if (auto it = latencies_.find("decision"); it != latencies_.end())
            decisions = it->second;
        k.p50_decision_latency_ms = percentile(decisions, 0.50);
        k.p95_decision_latency_ms = percentile(decisions, 0.95);
        k.p99_decision_latency_ms = percentile(decisions, 0.99);

        k.app_load_time_ms_baseline = baseline_app_load_ms_;
        k.app_load_time_ms_current = current_app_load_ms_;
        k.app_load_time_improvement_pct = baseline_app_load_ms_ == 0.0 ? 0.0
            : (baseline_app_load_ms_ - current_app_load_ms_) * 100.0 / baseline_app_load_ms_;
        k.launch_time_ms_baseline = baseline_launch_ms_;
        k.launch_time_ms_current = current_launch_ms_;
        k.launch_time_improvement_pct = baseline_launch_ms_ == 0.0 ? 0.0
            : (baseline_launch_ms_ - current_launch_ms_) * 100.0 / baseline_launch_ms_;
        k.memory_utilization_efficiency_pct = util_efficiency_;
        return k;
    }

    std::string to_json() const override {
        const auto k = snapshot();
        json j = {
            {"app_load_time_ms_baseline", k.app_load_time_ms_baseline},
            {"app_load_time_ms_current", k.app_load_time_ms_current},
            {"app_load_time_improvement_pct", k.app_load_time_improvement_pct},
            {"launch_time_ms_baseline", k.launch_time_ms_baseline},
            {"launch_time_ms_current", k.launch_time_ms_current},
            {"launch_time_improvement_pct", k.launch_time_improvement_pct},
            {"thrashing_events_baseline", k.thrashing_events_baseline},
            {"thrashing_events_current", k.thrashing_events_current},
            {"thrashing_reduction_pct", k.thrashing_reduction_pct},
            {"stability_issues", k.stability_issues},
            {"prediction_accuracy_top1", k.prediction_accuracy_top1},
            {"prediction_accuracy_top3", k.prediction_accuracy_top3},
            {"cache_hit_rate", k.cache_hit_rate},
            {"memory_utilization_efficiency_pct", k.memory_utilization_efficiency_pct},
            {"p50_decision_latency_ms", k.p50_decision_latency_ms},
            {"p95_decision_latency_ms", k.p95_decision_latency_ms},
            {"p99_decision_latency_ms", k.p99_decision_latency_ms},
        };
        return j.dump(2);
    }

    // These setters let the benchmark harness feed baseline numbers in.
    void set_baseline_app_load(double ms) { std::lock_guard g(mu_); baseline_app_load_ms_ = ms; }
    void set_current_app_load(double ms) { std::lock_guard g(mu_); current_app_load_ms_ = ms; }
    void set_baseline_launch(double ms) { std::lock_guard g(mu_); baseline_launch_ms_ = ms; }
    void set_current_launch(double ms) { std::lock_guard g(mu_); current_launch_ms_ = ms; }
    void set_util_efficiency(double pct) { std::lock_guard g(mu_); util_efficiency_ = pct; }
    void set_baseline_thrashing(double t) { std::lock_guard g(mu_); baseline_thrashing_ = t; }

private:
    void trim() {
        while (events_.size() > cfg_.telemetry_buffer) events_.pop_front();
    }

    mutable std::mutex mu_;
    Config cfg_;
    std::deque<std::string> events_;
    std::unordered_map<std::string, std::vector<double>> latencies_;
    std::size_t predictions_total_ = 0, predictions_top1_ = 0, predictions_top3_ = 0;
    std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> cache_;
    std::size_t thrash_events_ = 0, stability_issues_ = 0;
    std::vector<std::string> stability_reasons_;
    double baseline_app_load_ms_ = 0, current_app_load_ms_ = 0;
    double baseline_launch_ms_ = 0, current_launch_ms_ = 0;
    double util_efficiency_ = 0;
    double baseline_thrashing_ = 0;
};

}  // namespace

Result<std::unique_ptr<Telemetry>> make_telemetry(const Config& cfg) {
    return std::unique_ptr<Telemetry>(new InMemoryTelemetry(cfg));
}

}  // namespace mem
