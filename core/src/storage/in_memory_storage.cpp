// SPDX-License-Identifier: MIT
// Minimal in-memory Storage implementation. Swap in a SQLite-backed version
// when linking with the shipping CPM dependency; the same interface is used.

#include <memogent/storage.hpp>

#include <mutex>
#include <unordered_map>

namespace mem {

namespace {

class InMemoryStorage final : public Storage {
public:
    explicit InMemoryStorage(Config cfg) : cfg_(std::move(cfg)) {}

    Result<bool> append_event(const AppEvent& ev) override {
        std::lock_guard g(mu_);
        events_.push_back(ev);
        return true;
    }

    Result<std::vector<AppEvent>> load_events(std::size_t limit) const override {
        std::lock_guard g(mu_);
        std::vector<AppEvent> out;
        const std::size_t n = std::min(limit, events_.size());
        out.reserve(n);
        for (std::size_t i = events_.size() - n; i < events_.size(); ++i) {
            out.push_back(events_[i]);
        }
        return out;
    }

    Result<bool> save_predictor_state(std::string_view name,
                                      std::span<const std::uint8_t> blob) override {
        std::lock_guard g(mu_);
        predictor_blobs_[std::string{name}].assign(blob.begin(), blob.end());
        return true;
    }

    Result<std::vector<std::uint8_t>> load_predictor_state(
        std::string_view name) const override {
        std::lock_guard g(mu_);
        const auto it = predictor_blobs_.find(std::string{name});
        if (it == predictor_blobs_.end()) {
            return Error{ErrorCode::NotFound, "no predictor state"};
        }
        return it->second;
    }

    Result<bool> save_kv(std::string_view key, std::string_view value) override {
        std::lock_guard g(mu_);
        kv_[std::string{key}] = std::string{value};
        return true;
    }

    Result<std::string> load_kv(std::string_view key) const override {
        std::lock_guard g(mu_);
        auto it = kv_.find(std::string{key});
        if (it == kv_.end()) return Error{ErrorCode::NotFound, "no kv"};
        return it->second;
    }

    Result<bool> reset() override {
        std::lock_guard g(mu_);
        events_.clear(); predictor_blobs_.clear(); kv_.clear();
        return true;
    }

private:
    mutable std::mutex mu_;
    Config cfg_;
    std::vector<AppEvent> events_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> predictor_blobs_;
    std::unordered_map<std::string, std::string> kv_;
};

}  // namespace

Result<std::unique_ptr<Storage>> open_storage(const Config& cfg) {
    return std::unique_ptr<Storage>(new InMemoryStorage(cfg));
}

}  // namespace mem
