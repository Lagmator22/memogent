// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/app_event.hpp>
#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <memory>
#include <span>
#include <vector>

namespace mem {

// Pure-virtual port; implementations live under core/src/predictors/.
class Predictor {
public:
    virtual ~Predictor() = default;

    // Feed one observed event into the model's state.
    virtual void observe(const AppEvent& ev) = 0;

    // Return up to top_k predictions, sorted by descending score.
    virtual std::vector<Prediction> predict(std::size_t top_k) = 0;

    // Reset all internal state (forget everything).
    virtual void reset() = 0;

    // Persist / restore state from a byte blob so the predictor survives restart.
    virtual std::vector<std::uint8_t> serialize() const = 0;
    virtual Result<bool> deserialize(std::span<const std::uint8_t> bytes) = 0;

    // Human-readable identifier, e.g. "markov1" or "lstm-64-48".
    virtual std::string_view name() const noexcept = 0;
};

// Factory: picks an implementation based on Config.predictor.
Result<std::unique_ptr<Predictor>> make_predictor(const Config& cfg);

}  // namespace mem
