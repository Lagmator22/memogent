// SPDX-License-Identifier: MIT
// Four cache policies behind the single AdaptiveCache interface:
//   LRU       — classic, textbook baseline
//   LFU       — frequency-weighted
//   ARC       — Megiddo & Modha (USENIX FAST 2003)
//   ContextARC — our contribution: ARC augmented with a future-hint boost
//
// Implementation style chosen for readability over every last perf knob. The
// public interface is stable so a specialized implementation (e.g. lock-free
// or fixed-capacity arena) can drop in without touching callers.

#include <memogent/adaptive_cache.hpp>

#include <algorithm>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace mem {

namespace {

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------
using Bytes = std::vector<std::uint8_t>;

struct Entry {
    std::string key;
    Bytes value;
};

// ---------------------------------------------------------------------------
// LRU
// ---------------------------------------------------------------------------
class LRUCache final : public AdaptiveCache {
public:
    explicit LRUCache(std::size_t cap) : capacity_(cap) {}

    std::optional<Bytes> get(std::string_view key) override {
        std::lock_guard g(mu_);
        const auto it = map_.find(std::string{key});
        if (it == map_.end()) {
            ++misses_;
            return std::nullopt;
        }
        ++hits_;
        list_.splice(list_.begin(), list_, it->second);
        return it->second->value;
    }

    void put(std::string_view key, Bytes value) override {
        std::lock_guard g(mu_);
        const std::string k{key};
        const auto it = map_.find(k);
        if (it != map_.end()) {
            it->second->value = std::move(value);
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        list_.push_front(Entry{k, std::move(value)});
        map_[k] = list_.begin();
        evict_if_over();
    }

    void erase(std::string_view key) override {
        std::lock_guard g(mu_);
        auto it = map_.find(std::string{key});
        if (it == map_.end()) return;
        list_.erase(it->second);
        map_.erase(it);
    }

    void resize(std::size_t capacity) override {
        std::lock_guard g(mu_);
        capacity_ = capacity;
        evict_if_over();
    }

    void clear() override {
        std::lock_guard g(mu_);
        list_.clear(); map_.clear();
        hits_ = misses_ = evictions_ = 0;
    }

    CacheStats stats() const noexcept override {
        std::lock_guard g(mu_);
        return {hits_, misses_, evictions_, list_.size(), capacity_};
    }

    std::string_view policy_name() const noexcept override { return "lru"; }

private:
    void evict_if_over() {
        while (list_.size() > capacity_ && !list_.empty()) {
            const auto& tail = list_.back();
            map_.erase(tail.key);
            list_.pop_back();
            ++evictions_;
        }
    }
    mutable std::mutex mu_;
    std::list<Entry> list_;
    std::unordered_map<std::string, std::list<Entry>::iterator> map_;
    std::size_t capacity_;
    std::size_t hits_ = 0, misses_ = 0, evictions_ = 0;
};

// ---------------------------------------------------------------------------
// LFU — approximate, count-based; ties broken by recency.
// ---------------------------------------------------------------------------
class LFUCache final : public AdaptiveCache {
public:
    explicit LFUCache(std::size_t cap) : capacity_(cap) {}

    std::optional<Bytes> get(std::string_view key) override {
        std::lock_guard g(mu_);
        const auto it = store_.find(std::string{key});
        if (it == store_.end()) {
            ++misses_;
            return std::nullopt;
        }
        ++hits_;
        ++it->second.freq;
        it->second.ts = ++tick_;
        return it->second.value;
    }

    void put(std::string_view key, Bytes value) override {
        std::lock_guard g(mu_);
        const std::string k{key};
        auto it = store_.find(k);
        if (it != store_.end()) {
            it->second.value = std::move(value);
            ++it->second.freq;
            it->second.ts = ++tick_;
            return;
        }
        if (store_.size() >= capacity_) evict_victim();
        store_[k] = Item{std::move(value), 1, ++tick_};
    }

    void erase(std::string_view key) override {
        std::lock_guard g(mu_);
        store_.erase(std::string{key});
    }

    void resize(std::size_t c) override {
        std::lock_guard g(mu_);
        capacity_ = c;
        while (store_.size() > capacity_) evict_victim();
    }

    void clear() override {
        std::lock_guard g(mu_);
        store_.clear();
        hits_ = misses_ = evictions_ = 0;
        tick_ = 0;
    }

    CacheStats stats() const noexcept override {
        std::lock_guard g(mu_);
        return {hits_, misses_, evictions_, store_.size(), capacity_};
    }

    std::string_view policy_name() const noexcept override { return "lfu"; }

private:
    struct Item { Bytes value; std::size_t freq; std::uint64_t ts; };
    void evict_victim() {
        if (store_.empty()) return;
        auto victim = store_.begin();
        for (auto it = store_.begin(); it != store_.end(); ++it) {
            if (it->second.freq < victim->second.freq ||
                (it->second.freq == victim->second.freq &&
                 it->second.ts < victim->second.ts))
                victim = it;
        }
        store_.erase(victim);
        ++evictions_;
    }

    mutable std::mutex mu_;
    std::unordered_map<std::string, Item> store_;
    std::size_t capacity_;
    std::size_t hits_ = 0, misses_ = 0, evictions_ = 0;
    std::uint64_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// ARC — Megiddo & Modha 2003. Four lists: T1 (recent once), T2 (recent 2+),
// B1/B2 ghost entries. `p` adapts the balance.
// ---------------------------------------------------------------------------
class ARCCache : public AdaptiveCache {
public:
    explicit ARCCache(std::size_t cap) : capacity_(cap) {}

    std::optional<Bytes> get(std::string_view key) override {
        std::lock_guard g(mu_);
        const std::string k{key};
        if (auto it = map_t1_.find(k); it != map_t1_.end()) {
            ++hits_;
            // promote to T2
            auto entry = *it->second;
            t1_.erase(it->second); map_t1_.erase(it);
            t2_.push_front(std::move(entry));
            map_t2_[k] = t2_.begin();
            return t2_.begin()->value;
        }
        if (auto it = map_t2_.find(k); it != map_t2_.end()) {
            ++hits_;
            t2_.splice(t2_.begin(), t2_, it->second);
            return it->second->value;
        }
        ++misses_;
        return std::nullopt;
    }

    void put(std::string_view key, Bytes value) override {
        std::lock_guard g(mu_);
        const std::string k{key};
        if (auto it = map_t1_.find(k); it != map_t1_.end()) {
            auto entry = *it->second;
            entry.value = std::move(value);
            t1_.erase(it->second); map_t1_.erase(it);
            t2_.push_front(std::move(entry));
            map_t2_[k] = t2_.begin();
            return;
        }
        if (auto it = map_t2_.find(k); it != map_t2_.end()) {
            it->second->value = std::move(value);
            t2_.splice(t2_.begin(), t2_, it->second);
            return;
        }
        if (auto it = ghost_b1_.find(k); it != ghost_b1_.end()) {
            p_ = std::min(capacity_, p_ + std::max<std::size_t>(1, ghost_b2_.size() / std::max<std::size_t>(1, ghost_b1_.size())));
            replace(k, false);
            ghost_b1_.erase(it);
            t2_.push_front(Entry{k, std::move(value)});
            map_t2_[k] = t2_.begin();
            return;
        }
        if (auto it = ghost_b2_.find(k); it != ghost_b2_.end()) {
            p_ = (p_ > std::max<std::size_t>(1, ghost_b1_.size() / std::max<std::size_t>(1, ghost_b2_.size())))
                     ? p_ - std::max<std::size_t>(1, ghost_b1_.size() / std::max<std::size_t>(1, ghost_b2_.size()))
                     : 0;
            replace(k, true);
            ghost_b2_.erase(it);
            t2_.push_front(Entry{k, std::move(value)});
            map_t2_[k] = t2_.begin();
            return;
        }
        // fresh miss
        if (t1_.size() + ghost_b1_.size() >= capacity_) {
            if (t1_.size() < capacity_) {
                if (!ghost_b1_.empty()) {
                    auto victim = *ghost_b1_.begin();
                    ghost_b1_.erase(victim);
                }
                replace(k, false);
            } else {
                // drop LRU of T1 entirely
                if (!t1_.empty()) {
                    auto last = t1_.back().key;
                    t1_.pop_back(); map_t1_.erase(last);
                    ++evictions_;
                }
            }
        } else {
            const auto total = t1_.size() + t2_.size() + ghost_b1_.size() + ghost_b2_.size();
            if (total >= capacity_) {
                if (total == 2 * capacity_ && !ghost_b2_.empty()) {
                    auto victim = *ghost_b2_.begin();
                    ghost_b2_.erase(victim);
                }
                replace(k, false);
            }
        }
        t1_.push_front(Entry{k, std::move(value)});
        map_t1_[k] = t1_.begin();
    }

    void erase(std::string_view key) override {
        std::lock_guard g(mu_);
        const std::string k{key};
        if (auto it = map_t1_.find(k); it != map_t1_.end()) {
            t1_.erase(it->second); map_t1_.erase(it);
        } else if (auto it2 = map_t2_.find(k); it2 != map_t2_.end()) {
            t2_.erase(it2->second); map_t2_.erase(it2);
        }
        ghost_b1_.erase(k); ghost_b2_.erase(k);
    }

    void resize(std::size_t c) override {
        std::lock_guard g(mu_); capacity_ = c;
        while (t1_.size() + t2_.size() > capacity_) {
            if (!t1_.empty()) {
                auto k = t1_.back().key;
                t1_.pop_back(); map_t1_.erase(k);
            } else if (!t2_.empty()) {
                auto k = t2_.back().key;
                t2_.pop_back(); map_t2_.erase(k);
            }
            ++evictions_;
        }
    }

    void clear() override {
        std::lock_guard g(mu_);
        t1_.clear(); t2_.clear(); map_t1_.clear(); map_t2_.clear();
        ghost_b1_.clear(); ghost_b2_.clear();
        hits_ = misses_ = evictions_ = 0; p_ = 0;
    }

    CacheStats stats() const noexcept override {
        std::lock_guard g(mu_);
        return {hits_, misses_, evictions_, t1_.size() + t2_.size(), capacity_};
    }

    std::string_view policy_name() const noexcept override { return "arc"; }

protected:
    void replace(const std::string& key, bool in_b2) {
        if (!t1_.empty() && (t1_.size() > p_ || (in_b2 && t1_.size() == p_))) {
            auto k = t1_.back().key;
            t1_.pop_back(); map_t1_.erase(k);
            ghost_b1_.insert(k);
        } else if (!t2_.empty()) {
            auto k = t2_.back().key;
            t2_.pop_back(); map_t2_.erase(k);
            ghost_b2_.insert(k);
        }
        ++evictions_;
        (void)key;
    }

    mutable std::mutex mu_;
    std::list<Entry> t1_, t2_;
    std::unordered_map<std::string, std::list<Entry>::iterator> map_t1_, map_t2_;
    std::unordered_set<std::string> ghost_b1_, ghost_b2_;
    std::size_t capacity_ = 0, p_ = 0;
    std::size_t hits_ = 0, misses_ = 0, evictions_ = 0;
};

// ---------------------------------------------------------------------------
// ContextARC — our contribution. ARC with an extra "future-hint" score that
// the Preloader / Predictor write into via hint_future(). Keys with active
// hints are treated as recently accessed, pulling them into T2 so the
// standard ARC eviction keeps them hot.
// ---------------------------------------------------------------------------
class ContextARCCache final : public ARCCache {
public:
    using ARCCache::ARCCache;

    void hint_future(std::span<const Prediction> predictions) override {
        std::lock_guard g(mu_);
        for (const auto& p : predictions) {
            const auto& k = p.app_id;
            if (auto it = map_t1_.find(k); it != map_t1_.end()) {
                auto entry = *it->second;
                t1_.erase(it->second); map_t1_.erase(it);
                t2_.push_front(std::move(entry));
                map_t2_[k] = t2_.begin();
            } else if (auto it = map_t2_.find(k); it != map_t2_.end()) {
                t2_.splice(t2_.begin(), t2_, it->second);
            }
            // Entries not in the cache are left alone — the Preloader decides
            // whether to materialize them.
        }
    }

    std::string_view policy_name() const noexcept override { return "context_arc"; }
};

}  // namespace

Result<std::unique_ptr<AdaptiveCache>> make_cache(const Config& cfg, std::size_t capacity) {
    if (cfg.cache_policy == "lru") return std::unique_ptr<AdaptiveCache>(new LRUCache(capacity));
    if (cfg.cache_policy == "lfu") return std::unique_ptr<AdaptiveCache>(new LFUCache(capacity));
    if (cfg.cache_policy == "arc") return std::unique_ptr<AdaptiveCache>(new ARCCache(capacity));
    if (cfg.cache_policy == "context_arc" || cfg.cache_policy == "contextarc")
        return std::unique_ptr<AdaptiveCache>(new ContextARCCache(capacity));
    return Error{ErrorCode::InvalidArgument, "unknown cache policy: " + cfg.cache_policy};
}

}  // namespace mem
