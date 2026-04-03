// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mem {

struct ModelSlot {
    ModelId id;
    std::size_t bytes = 0;
    bool resident = false;
    Timestamp last_used = 0.0;
    int priority = 0;
};

// Keeps track of loaded model weights and orchestrates swap-in / swap-out.
// The host app implements the actual fs IO via callbacks supplied at init.
class ModelSwapManager {
public:
    virtual ~ModelSwapManager() = default;

    virtual void register_model(const ModelSlot& slot) = 0;
    virtual void mark_used(const ModelId& id, Timestamp now) = 0;

    // Given current RAM budget + predicted next usage, decide which to swap.
    virtual std::vector<ModelId> plan_evictions(std::size_t ram_budget_bytes) = 0;

    // Given predictor hints, prefetch these if they fit in the budget.
    virtual std::vector<ModelId> plan_preloads(const std::vector<ModelId>& predicted,
                                               std::size_t ram_budget_bytes) = 0;

    virtual const ModelSlot* find(const ModelId& id) const = 0;
    virtual std::vector<ModelSlot> snapshot() const = 0;
};

Result<std::unique_ptr<ModelSwapManager>> make_model_swap_manager(const Config& cfg);

}  // namespace mem
