// SPDX-License-Identifier: MIT
#include <memogent/model_swap.hpp>

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace mem {

namespace {

class DefaultModelSwap final : public ModelSwapManager {
public:
    explicit DefaultModelSwap(const Config&) {}

    void register_model(const ModelSlot& slot) override {
        std::lock_guard g(mu_);
        slots_[slot.id] = slot;
    }

    void mark_used(const ModelId& id, Timestamp now) override {
        std::lock_guard g(mu_);
        auto it = slots_.find(id);
        if (it != slots_.end()) it->second.last_used = now;
    }

    std::vector<ModelId> plan_evictions(std::size_t ram_budget_bytes) override {
        std::lock_guard g(mu_);
        std::size_t total = 0;
        std::vector<ModelSlot> resident;
        for (const auto& [_, s] : slots_) {
            if (s.resident) { total += s.bytes; resident.push_back(s); }
        }
        if (total <= ram_budget_bytes) return {};
        std::sort(resident.begin(), resident.end(),
                  [](const ModelSlot& a, const ModelSlot& b) {
                      if (a.priority != b.priority) return a.priority < b.priority;
                      return a.last_used < b.last_used;
                  });
        std::vector<ModelId> out;
        for (const auto& s : resident) {
            if (total <= ram_budget_bytes) break;
            out.push_back(s.id);
            total -= s.bytes;
        }
        return out;
    }

    std::vector<ModelId> plan_preloads(const std::vector<ModelId>& predicted,
                                       std::size_t ram_budget_bytes) override {
        std::lock_guard g(mu_);
        std::size_t resident_bytes = 0;
        for (const auto& [_, s] : slots_) if (s.resident) resident_bytes += s.bytes;
        std::vector<ModelId> out;
        for (const auto& id : predicted) {
            auto it = slots_.find(id);
            if (it == slots_.end() || it->second.resident) continue;
            if (resident_bytes + it->second.bytes > ram_budget_bytes) break;
            out.push_back(id);
            resident_bytes += it->second.bytes;
        }
        return out;
    }

    const ModelSlot* find(const ModelId& id) const override {
        std::lock_guard g(mu_);
        auto it = slots_.find(id);
        return it == slots_.end() ? nullptr : &it->second;
    }

    std::vector<ModelSlot> snapshot() const override {
        std::lock_guard g(mu_);
        std::vector<ModelSlot> out;
        out.reserve(slots_.size());
        for (const auto& [_, s] : slots_) out.push_back(s);
        return out;
    }

private:
    mutable std::mutex mu_;
    std::unordered_map<ModelId, ModelSlot> slots_;
};

}  // namespace

Result<std::unique_ptr<ModelSwapManager>> make_model_swap_manager(const Config& cfg) {
    return std::unique_ptr<ModelSwapManager>(new DefaultModelSwap(cfg));
}

}  // namespace mem
