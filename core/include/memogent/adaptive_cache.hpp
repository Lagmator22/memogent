// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mem {

// Statistics returned by any cache backend.
struct CacheStats {
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t evictions = 0;
    std::size_t size = 0;
    std::size_t capacity = 0;

    [[nodiscard]] double hit_rate() const noexcept {
        const auto total = hits + misses;
        return total == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(total);
    }
};

// Key-value cache parametrized by string key / bytes value so the C ABI can
// pass through anything the host wants (serialized app state, model metadata,
// KV-cache page ids, ...).
class AdaptiveCache {
public:
    virtual ~AdaptiveCache() = default;

    // Return a copy of the stored value if present; nullopt otherwise.
    virtual std::optional<std::vector<std::uint8_t>> get(std::string_view key) = 0;

    // Insert or overwrite. May evict according to the policy.
    virtual void put(std::string_view key, std::vector<std::uint8_t> value) = 0;

    // Remove a specific key; no-op if missing.
    virtual void erase(std::string_view key) = 0;

    // Hint from the Predictor — tells the policy what is likely next.
    // Default implementation is a no-op; ContextARC uses it to boost scores.
    virtual void hint_future(std::span<const Prediction> predictions) {
        (void)predictions;
    }

    // Resize the capacity online.
    virtual void resize(std::size_t capacity) = 0;

    virtual void clear() = 0;

    virtual CacheStats stats() const noexcept = 0;

    virtual std::string_view policy_name() const noexcept = 0;
};

// Factory: picks a policy implementation based on Config.cache_policy.
Result<std::unique_ptr<AdaptiveCache>> make_cache(const Config& cfg,
                                                  std::size_t capacity);

}  // namespace mem
