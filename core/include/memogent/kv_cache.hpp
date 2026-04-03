// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/config.hpp>
#include <memogent/result.hpp>
#include <memogent/types.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace mem {

// A single KV block tracked by the manager.
struct KVBlock {
    std::uint64_t sequence_id = 0;
    std::uint32_t token_start = 0;
    std::uint32_t token_count = 0;
    float salience = 0.0f;   // heavy-hitter / attention-mass hint
    bool pinned = false;     // sink / recent windows cannot be evicted
    std::size_t bytes = 0;
};

struct KVPlan {
    std::vector<std::uint64_t> evict;  // sequence ids to unpin + free
    std::vector<std::uint64_t> quantize; // demote fp16->int8 on these
};

// Pure-virtual manager so host apps can wire up llama.cpp / vLLM / custom.
class KVCacheManager {
public:
    virtual ~KVCacheManager() = default;

    virtual void observe(const KVBlock& blk) = 0;
    virtual void release(std::uint64_t sequence_id) = 0;
    virtual KVPlan plan(std::size_t token_budget) = 0;
    virtual void reset() = 0;
    virtual std::size_t total_tokens() const noexcept = 0;
};

Result<std::unique_ptr<KVCacheManager>> make_kv_cache_manager(const Config& cfg);

}  // namespace mem
