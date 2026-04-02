// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/adaptive_cache.hpp>
#include <memogent/config.hpp>
#include <memogent/device_state.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mem {

// An immutable view of the current system + history we pass to the arbiter.
struct ArbiterInputs {
    DeviceState device;
    PowerMode mode = PowerMode::Full;
    std::vector<Prediction> predictions;
    std::size_t apps_resident = 0;
    std::size_t models_resident = 0;
    std::size_t kv_tokens = 0;
    CacheStats app_cache;
    CacheStats model_cache;
    CacheStats kv_cache;
};

// The decision produced per tick.
struct ArbiterPlan {
    std::size_t app_cache_capacity = 0;
    std::size_t model_cache_capacity = 0;
    std::size_t kv_cache_token_budget = 0;
    bool allow_preload = true;
    bool allow_consolidation = true;
    std::string rationale;
};

class Arbiter {
public:
    virtual ~Arbiter() = default;
    virtual ArbiterPlan decide(const ArbiterInputs& in) = 0;
    virtual std::string_view name() const noexcept = 0;
};

Result<std::unique_ptr<Arbiter>> make_arbiter(const Config& cfg);

}  // namespace mem
