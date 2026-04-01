// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/types.hpp>

#include <optional>
#include <string>

namespace mem {

// Value type that carries one user-interaction or system observation.
// Producers: iOS MetricKit + UIApplication hooks, Android UsageStatsManager +
// ActivityManager, or the host process itself.
struct AppEvent {
    EventType type = EventType::UserInteraction;
    AppId app_id;                         // for app-related events
    ModelId model_id;                     // for model-related events
    Timestamp timestamp = 0.0;            // epoch seconds
    std::optional<std::string> location;  // coarse (country/city) or BSSID hash
    std::optional<int> hour_of_day;       // 0..23 convenience
    std::optional<std::string> screen_state;  // "on" | "off" | "locked"
    float battery_pct = 100.0f;
    float thermal = 0.25f;
};

}  // namespace mem
