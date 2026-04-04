// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/adaptive_cache.hpp>
#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace mem {

// What the preloader decides to do this tick.
struct PreloadPlan {
    std::vector<AppId> apps;        // app states to rehydrate / warm
    std::vector<ModelId> models;    // model weights to bring resident
    std::vector<std::string> keys;  // generic cache keys to prime
};

class Preloader {
public:
    virtual ~Preloader() = default;

    // Build a plan given the predictor's top-k and current QoS.
    virtual PreloadPlan plan(std::span<const Prediction> predictions,
                             PowerMode mode,
                             std::size_t ram_budget_mb) = 0;

    // Execute a plan against a cache. Callers can skip this and perform
    // preloading themselves.
    virtual void execute(const PreloadPlan& plan, AdaptiveCache& cache) = 0;
};

Result<std::unique_ptr<Preloader>> make_preloader(const Config& cfg);

}  // namespace mem
