// SPDX-License-Identifier: MIT
#include <memogent/types.hpp>

namespace mem {

const char* power_mode_name(PowerMode m) noexcept {
    switch (m) {
        case PowerMode::Full: return "full";
        case PowerMode::Reduced: return "reduced";
        case PowerMode::Minimal: return "minimal";
        case PowerMode::Paused: return "paused";
    }
    return "unknown";
}

const char* event_type_name(EventType t) noexcept {
    switch (t) {
        case EventType::AppOpen: return "app_open";
        case EventType::AppClose: return "app_close";
        case EventType::AppBackground: return "app_background";
        case EventType::AppForeground: return "app_foreground";
        case EventType::UserInteraction: return "user_interaction";
        case EventType::ModelLoad: return "model_load";
        case EventType::ModelUnload: return "model_unload";
        case EventType::ModelInvoke: return "model_invoke";
        case EventType::CacheHit: return "cache_hit";
        case EventType::CacheMiss: return "cache_miss";
        case EventType::Preload: return "preload";
        case EventType::Evict: return "evict";
    }
    return "unknown";
}

const char* resource_pool_name(ResourcePool p) noexcept {
    switch (p) {
        case ResourcePool::AppState: return "app_state";
        case ResourcePool::ModelWeights: return "model_weights";
        case ResourcePool::KVCache: return "kv_cache";
        case ResourcePool::Embedding: return "embedding";
        case ResourcePool::Other: return "other";
    }
    return "unknown";
}

}  // namespace mem
