// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/app_event.hpp>
#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mem {

// Everything the orchestrator needs to persist survives here.
class Storage {
public:
    virtual ~Storage() = default;

    virtual Result<bool> append_event(const AppEvent& ev) = 0;
    virtual Result<std::vector<AppEvent>> load_events(std::size_t limit) const = 0;

    virtual Result<bool> save_predictor_state(std::string_view name,
                                              std::span<const std::uint8_t> blob) = 0;
    virtual Result<std::vector<std::uint8_t>> load_predictor_state(
        std::string_view name) const = 0;

    virtual Result<bool> save_kv(std::string_view key, std::string_view value) = 0;
    virtual Result<std::string> load_kv(std::string_view key) const = 0;

    virtual Result<bool> reset() = 0;
};

Result<std::unique_ptr<Storage>> open_storage(const Config& cfg);

}  // namespace mem
