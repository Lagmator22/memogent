// SPDX-License-Identifier: MIT
#include <memogent/preloader.hpp>

#include <algorithm>

namespace mem {

namespace {

class DefaultPreloader final : public Preloader {
public:
    explicit DefaultPreloader(const Config& cfg) : cfg_(cfg) {}

    PreloadPlan plan(std::span<const Prediction> predictions, PowerMode mode,
                     std::size_t /*ram_budget_mb*/) override {
        PreloadPlan p;
        if (mode >= PowerMode::Minimal) return p;

        const std::size_t k = std::min<std::size_t>(
            cfg_.preload_top_k,
            mode == PowerMode::Reduced ? 1 : cfg_.preload_top_k);
        for (std::size_t i = 0; i < predictions.size() && i < k; ++i) {
            p.apps.push_back(predictions[i].app_id);
            p.keys.push_back("warm:" + predictions[i].app_id);
        }
        return p;
    }

    void execute(const PreloadPlan& plan, AdaptiveCache& cache) override {
        for (const auto& key : plan.keys) {
            // Touch the cache with a minimal placeholder; host apps override
            // this method with their own preload routines via the C ABI.
            if (!cache.get(key)) {
                cache.put(key, std::vector<std::uint8_t>{'w'});
            }
        }
    }

private:
    Config cfg_;
};

}  // namespace

Result<std::unique_ptr<Preloader>> make_preloader(const Config& cfg) {
    return std::unique_ptr<Preloader>(new DefaultPreloader(cfg));
}

}  // namespace mem
