// SPDX-License-Identifier: MIT
#include <memogent/device_state.hpp>

namespace mem {

PowerMode classify(const DeviceState& s, const Config& cfg) noexcept {
    // Paused level: any single resource critical.
    if (s.thermal >= cfg.qos_thermal_paused ||
        (s.battery_pct <= cfg.qos_battery_paused && !s.charging) ||
        s.ram_free_mb <= cfg.qos_ram_paused_mb) {
        return PowerMode::Paused;
    }
    // Minimal.
    if (s.thermal >= cfg.qos_thermal_minimal ||
        (s.battery_pct <= cfg.qos_battery_minimal && !s.charging) ||
        s.ram_free_mb <= cfg.qos_ram_minimal_mb ||
        s.low_power_mode) {
        return PowerMode::Minimal;
    }
    // Reduced.
    if (s.thermal >= cfg.qos_thermal_reduced ||
        (s.battery_pct <= cfg.qos_battery_reduced && !s.charging)) {
        return PowerMode::Reduced;
    }
    return PowerMode::Full;
}

}  // namespace mem
