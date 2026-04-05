// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mem {

using AppId = std::string;       // e.g. "com.instagram.android" or "Facebook"
using ModelId = std::string;     // e.g. "gemma-3-4b-it-Q4_K_M.gguf"
using Timestamp = double;        // seconds since epoch

// ---------------------------------------------------------------------------
// PowerMode — adaptive QoS states, mapped to battery/thermal/RAM pressure.
// ---------------------------------------------------------------------------
enum class PowerMode : std::uint8_t {
    Full = 0,       // everything on
    Reduced = 1,    // narrower prediction window, cheaper cache ops
    Minimal = 2,    // rule-based predictor, no preload, conservative evict
    Paused = 3,     // read-only; reject writes
};

const char* power_mode_name(PowerMode m) noexcept;

// ---------------------------------------------------------------------------
// EventType — what the OS / host app reports.
// ---------------------------------------------------------------------------
enum class EventType : std::uint8_t {
    AppOpen = 0,
    AppClose = 1,
    AppBackground = 2,
    AppForeground = 3,
    UserInteraction = 4,
    ModelLoad = 5,
    ModelUnload = 6,
    ModelInvoke = 7,
    CacheHit = 8,
    CacheMiss = 9,
    Preload = 10,
    Evict = 11,
};

const char* event_type_name(EventType t) noexcept;

// ---------------------------------------------------------------------------
// ResourcePool — what the cache slot represents.
// ---------------------------------------------------------------------------
enum class ResourcePool : std::uint8_t {
    AppState = 0,       // live app in foreground / recents
    ModelWeights = 1,   // quantized LLM / ONNX weights
    KVCache = 2,        // LLM KV pages
    Embedding = 3,      // vector embedder cache
    Other = 255,
};

const char* resource_pool_name(ResourcePool p) noexcept;

// ---------------------------------------------------------------------------
// Prediction — single hypothesis produced by a Predictor.
// ---------------------------------------------------------------------------
struct Prediction {
    AppId app_id;
    float score = 0.0f;  // [0,1] higher = more likely
    std::string reason;  // optional human-readable justification (debug only)
};

}  // namespace mem
