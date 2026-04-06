// SPDX-License-Identifier: MIT
#include <memogent/arbiter.hpp>

#include <algorithm>

namespace mem {

namespace {

class HeuristicArbiter final : public Arbiter {
public:
    explicit HeuristicArbiter(const Config& cfg) : cfg_(cfg) {}

    ArbiterPlan decide(const ArbiterInputs& in) override {
        ArbiterPlan p{};
        p.app_cache_capacity = cfg_.cache_capacity_app;
        p.model_cache_capacity = cfg_.cache_capacity_model;
        p.kv_cache_token_budget = cfg_.kv_cache_token_budget;
        p.allow_preload = cfg_.preload_enabled;
        p.allow_consolidation = true;
        p.rationale = "baseline";

        switch (in.mode) {
            case PowerMode::Full:
                break;
            case PowerMode::Reduced:
                p.app_cache_capacity = p.app_cache_capacity * 3 / 4;
                p.model_cache_capacity = std::max<std::size_t>(1, p.model_cache_capacity - 1);
                p.kv_cache_token_budget = p.kv_cache_token_budget * 3 / 4;
                p.rationale = "reduced: shrink app+model caches, keep preload";
                break;
            case PowerMode::Minimal:
                p.app_cache_capacity = p.app_cache_capacity / 2;
                p.model_cache_capacity = 1;
                p.kv_cache_token_budget = p.kv_cache_token_budget / 2;
                p.allow_preload = false;
                p.rationale = "minimal: single model, halved caches, no preload";
                break;
            case PowerMode::Paused:
                p.app_cache_capacity = std::max<std::size_t>(4, p.app_cache_capacity / 4);
                p.model_cache_capacity = 0;
                p.kv_cache_token_budget = cfg_.kv_cache_sink_tokens + cfg_.kv_cache_recent_tokens;
                p.allow_preload = false;
                p.allow_consolidation = false;
                p.rationale = "paused: survival mode";
                break;
        }
        // Give preload an extra slot when we have high-confidence predictions.
        if (!in.predictions.empty() && in.predictions.front().score > 0.70f
            && in.mode <= PowerMode::Reduced) {
            p.app_cache_capacity += 1;
            p.rationale += "; +1 slot for high-confidence preload";
        }
        return p;
    }

    std::string_view name() const noexcept override { return "heuristic"; }

private:
    Config cfg_;
};

}  // namespace

Result<std::unique_ptr<Arbiter>> make_arbiter(const Config& cfg) {
    if (cfg.arbiter == "heuristic" || cfg.arbiter == "soft_budget")
        return std::unique_ptr<Arbiter>(new HeuristicArbiter(cfg));
    return Error{ErrorCode::InvalidArgument, "unknown arbiter: " + cfg.arbiter};
}

}  // namespace mem
