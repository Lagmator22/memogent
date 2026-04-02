// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace mem {

// Central knob panel. All fields have safe defaults; a fresh Config is usable.
struct Config {
    // --- storage ---
    std::string data_dir{"./memogent-data"};
    std::string db_filename{"memogent.db"};
    bool enable_wal{true};

    // --- predictor ---
    std::string predictor{"freq_recency"};   // markov | freq_recency | lstm | mfu | mru
    std::string predictor_model_path;        // used by LSTM/Transformer/ONNX backends
    std::uint32_t predictor_history{32};     // tokens of history to feed
    std::uint32_t prediction_top_k{3};

    // --- adaptive cache ---
    std::string cache_policy{"context_arc"}; // lru | lfu | arc | context_arc
    std::size_t cache_capacity_app{32};
    std::size_t cache_capacity_model{4};
    std::size_t cache_capacity_kv_pages{1024};
    std::size_t cache_capacity_embeddings{1024};

    // --- arbiter ---
    std::string arbiter{"heuristic"};        // heuristic | soft_budget | td3
    std::size_t ram_budget_mb{768};          // soft cap used for decisions

    // --- preloader ---
    bool preload_enabled{true};
    std::uint32_t preload_top_k{2};

    // --- LLM / KV cache ---
    std::string llm_backend{"mock"};         // mock | ollama | llama.cpp
    std::string llm_model_path;
    std::size_t kv_cache_token_budget{4096};
    std::size_t kv_cache_sink_tokens{4};     // protected "sink" head
    std::size_t kv_cache_recent_tokens{512}; // always-keep window

    // --- qos thresholds ---
    float qos_battery_reduced{30.0f};
    float qos_battery_minimal{15.0f};
    float qos_battery_paused{5.0f};
    float qos_thermal_reduced{0.55f};
    float qos_thermal_minimal{0.75f};
    float qos_thermal_paused{0.90f};
    float qos_ram_minimal_mb{128.0f};
    float qos_ram_paused_mb{48.0f};

    // --- telemetry ---
    bool telemetry_enabled{true};
    std::size_t telemetry_buffer{4096};

    // --- misc ---
    std::string session_id{"default"};
    std::uint32_t random_seed{0x5EED};
    bool verbose{false};
};

}  // namespace mem
