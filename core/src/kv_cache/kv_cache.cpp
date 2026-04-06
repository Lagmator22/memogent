// SPDX-License-Identifier: MIT
#include <memogent/kv_cache.hpp>

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace mem {

namespace {

// A pragmatic default that combines StreamingLLM's sink+recent idea with
// salience-aware eviction (H2O / SAGE-KV style). Exact paged-attention
// integration lives behind the MEM_WITH_LLAMACPP build flag and a separate
// adapter class.
class DefaultKVCacheManager final : public KVCacheManager {
public:
    explicit DefaultKVCacheManager(const Config& cfg) : cfg_(cfg) {}

    void observe(const KVBlock& blk) override {
        std::lock_guard g(mu_);
        blocks_[blk.sequence_id] = blk;
        total_tokens_ += blk.token_count;
    }

    void release(std::uint64_t sequence_id) override {
        std::lock_guard g(mu_);
        auto it = blocks_.find(sequence_id);
        if (it != blocks_.end()) {
            total_tokens_ -= it->second.token_count;
            blocks_.erase(it);
        }
    }

    KVPlan plan(std::size_t token_budget) override {
        std::lock_guard g(mu_);
        KVPlan out;
        if (total_tokens_ <= token_budget) return out;

        std::vector<const KVBlock*> ordered;
        ordered.reserve(blocks_.size());
        for (const auto& [_, b] : blocks_) ordered.push_back(&b);
        std::sort(ordered.begin(), ordered.end(),
                  [](const KVBlock* a, const KVBlock* b) {
                      return a->salience < b->salience;  // lowest salience first
                  });

        std::size_t to_evict = total_tokens_ - token_budget;
        for (const auto* b : ordered) {
            if (b->pinned) continue;
            // Protect the "sink" window and the "recent" tail from eviction.
            if (b->token_start < cfg_.kv_cache_sink_tokens) continue;
            if (b->token_start + b->token_count >
                token_budget - cfg_.kv_cache_recent_tokens)
                continue;
            out.evict.push_back(b->sequence_id);
            if (b->token_count >= to_evict) break;
            to_evict -= b->token_count;
        }
        return out;
    }

    void reset() override {
        std::lock_guard g(mu_); blocks_.clear(); total_tokens_ = 0;
    }

    std::size_t total_tokens() const noexcept override { return total_tokens_; }

private:
    mutable std::mutex mu_;
    std::unordered_map<std::uint64_t, KVBlock> blocks_;
    std::size_t total_tokens_ = 0;
    Config cfg_;
};

}  // namespace

Result<std::unique_ptr<KVCacheManager>> make_kv_cache_manager(const Config& cfg) {
    return std::unique_ptr<KVCacheManager>(new DefaultKVCacheManager(cfg));
}

}  // namespace mem
