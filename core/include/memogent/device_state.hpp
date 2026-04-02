// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/config.hpp>
#include <memogent/types.hpp>

namespace mem {

// Snapshot of the device. On iOS/Android the bindings populate this from
// ProcessInfo.thermalState / BatteryManager / ActivityManager. On desktop
// we synthesize values via a simulator for deterministic benchmarking.
struct DeviceState {
    float battery_pct = 100.0f;  // 0..100
    float thermal = 0.25f;       // 0=cool .. 1=critical
    float ram_free_mb = 1024.0f;
    bool charging = false;
    bool low_power_mode = false;
    Timestamp timestamp = 0.0;
};

PowerMode classify(const DeviceState& s, const Config& cfg) noexcept;

}  // namespace mem
