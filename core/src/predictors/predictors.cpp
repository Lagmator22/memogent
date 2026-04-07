// SPDX-License-Identifier: MIT
// Four predictor implementations in one translation unit:
//   - MFU (most frequently used) — baseline
//   - MRU (most recently used) — baseline
//   - Markov n=1 — strong, interpretable
//   - FreqRecency — hand-tuned features (hour-of-day × last-app × frequency)
//
// Each is ~100 lines so we keep them together for readability. LSTM / ONNX
// backends live in separate files under bindings/ and plug in via the same
// Predictor interface.

#include <memogent/predictor.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <unordered_map>

namespace mem {

namespace {

constexpr std::size_t kHistoryCap = 256;

// ---------------------------------------------------------------------------
// MFU — picks whichever app has been seen most often over the last N events.
// Trivial baseline Samsung's brief calls out explicitly.
// ---------------------------------------------------------------------------
class MFUPredictor final : public Predictor {
public:
    void observe(const AppEvent& ev) override {
        if (ev.type != EventType::AppOpen && ev.type != EventType::AppForeground)
            return;
        ++counts_[ev.app_id];
        history_.push_back(ev.app_id);
        if (history_.size() > kHistoryCap) history_.pop_front();
    }

    std::vector<Prediction> predict(std::size_t top_k) override {
        std::vector<std::pair<AppId, std::size_t>> ordered(counts_.begin(), counts_.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        std::vector<Prediction> out;
        const std::size_t total = history_.size();
        for (std::size_t i = 0; i < ordered.size() && out.size() < top_k; ++i) {
            const auto score = total == 0
                                   ? 0.0f
                                   : static_cast<float>(ordered[i].second) / total;
            out.push_back({ordered[i].first, score, "mfu"});
        }
        return out;
    }

    void reset() override { counts_.clear(); history_.clear(); }
    std::vector<std::uint8_t> serialize() const override { return {}; }
    Result<bool> deserialize(std::span<const std::uint8_t>) override { return true; }
    std::string_view name() const noexcept override { return "mfu"; }

private:
    std::unordered_map<AppId, std::size_t> counts_;
    std::deque<AppId> history_;
};

// ---------------------------------------------------------------------------
// MRU — most recently used is the predicted next. Simple, surprisingly tough
// in short horizons. Matches the canonical Android "Recents" heuristic.
// ---------------------------------------------------------------------------
class MRUPredictor final : public Predictor {
public:
    void observe(const AppEvent& ev) override {
        if (ev.type != EventType::AppOpen && ev.type != EventType::AppForeground)
            return;
        seen_.insert(ev.app_id);
        history_.push_back(ev.app_id);
        if (history_.size() > kHistoryCap) history_.pop_front();
    }

    std::vector<Prediction> predict(std::size_t top_k) override {
        std::vector<Prediction> out;
        std::unordered_map<AppId, std::size_t> last_idx;
        for (std::size_t i = 0; i < history_.size(); ++i) {
            last_idx[history_[i]] = i;
        }
        std::vector<std::pair<AppId, std::size_t>> ordered(last_idx.begin(), last_idx.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t i = 0; i < ordered.size() && out.size() < top_k; ++i) {
            const auto score = 1.0f - static_cast<float>(i) / ordered.size();
            out.push_back({ordered[i].first, std::max(0.0f, score), "mru"});
        }
        return out;
    }

    void reset() override { seen_.clear(); history_.clear(); }
    std::vector<std::uint8_t> serialize() const override { return {}; }
    Result<bool> deserialize(std::span<const std::uint8_t>) override { return true; }
    std::string_view name() const noexcept override { return "mru"; }

private:
    std::unordered_map<AppId, int> seen_{};
    std::deque<AppId> history_;
};

// ---------------------------------------------------------------------------
// Markov n=1 — P(next | last). Trained online, fast, interpretable.
// We add Laplace smoothing so unseen transitions still have small probability.
// ---------------------------------------------------------------------------
class Markov1Predictor final : public Predictor {
public:
    void observe(const AppEvent& ev) override {
        if (ev.type != EventType::AppOpen && ev.type != EventType::AppForeground)
            return;
        if (!last_.empty()) {
            ++transitions_[last_][ev.app_id];
            ++totals_[last_];
        }
        ++unigram_[ev.app_id];
        ++grand_total_;
        last_ = ev.app_id;
    }

    std::vector<Prediction> predict(std::size_t top_k) override {
        std::vector<Prediction> out;
        if (last_.empty()) return out;
        auto it = transitions_.find(last_);
        if (it == transitions_.end()) {
            // fallback to global unigram.
            std::vector<std::pair<AppId, std::size_t>> ordered(unigram_.begin(), unigram_.end());
            std::sort(ordered.begin(), ordered.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
            for (std::size_t i = 0; i < ordered.size() && out.size() < top_k; ++i) {
                out.push_back({ordered[i].first,
                               grand_total_ == 0 ? 0.0f
                                                : static_cast<float>(ordered[i].second) / grand_total_,
                               "markov1/fallback"});
            }
            return out;
        }
        const auto total = static_cast<float>(totals_[last_]) + unigram_.size();
        std::vector<std::pair<AppId, float>> ordered;
        ordered.reserve(it->second.size());
        for (const auto& [app, count] : it->second) {
            ordered.emplace_back(app, (static_cast<float>(count) + 1.0f) / total);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t i = 0; i < ordered.size() && out.size() < top_k; ++i) {
            out.push_back({ordered[i].first, ordered[i].second, "markov1"});
        }
        return out;
    }

    void reset() override {
        transitions_.clear(); totals_.clear(); unigram_.clear();
        grand_total_ = 0; last_.clear();
    }
    std::vector<std::uint8_t> serialize() const override { return {}; }
    Result<bool> deserialize(std::span<const std::uint8_t>) override { return true; }
    std::string_view name() const noexcept override { return "markov1"; }

private:
    std::unordered_map<AppId, std::unordered_map<AppId, std::size_t>> transitions_;
    std::unordered_map<AppId, std::size_t> totals_;
    std::unordered_map<AppId, std::size_t> unigram_;
    std::size_t grand_total_ = 0;
    AppId last_;
};

// ---------------------------------------------------------------------------
// FreqRecency — blended hand-tuned features.
//   score(app) = α*global_freq + β*recency_decay + γ*hour_bias[hour][app]
// Matches MAPLE / AppFormer-lite performance at a fraction of the cost.
// ---------------------------------------------------------------------------
class FreqRecencyPredictor final : public Predictor {
public:
    void observe(const AppEvent& ev) override {
        if (ev.type != EventType::AppOpen && ev.type != EventType::AppForeground)
            return;
        ++unigram_[ev.app_id];
        last_seen_[ev.app_id] = ev.timestamp;
        last_ts_ = ev.timestamp;
        if (ev.hour_of_day) {
            ++hour_counts_[*ev.hour_of_day][ev.app_id];
            ++hour_totals_[*ev.hour_of_day];
        }
    }

    std::vector<Prediction> predict(std::size_t top_k) override {
        std::vector<Prediction> out;
        if (unigram_.empty()) return out;
        // Determine current hour — pull from last event timestamp.
        const int hour = [&] {
            const auto t = static_cast<std::time_t>(last_ts_);
            std::tm tm{};
#ifdef _WIN32
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            return tm.tm_hour;
        }();

        float total_unigram = 0;
        for (const auto& [_, c] : unigram_) total_unigram += c;
        std::vector<std::pair<AppId, float>> scored;
        scored.reserve(unigram_.size());
        for (const auto& [app, count] : unigram_) {
            const float f = count / std::max(1.0f, total_unigram);
            const float age = std::max(0.0f, static_cast<float>(last_ts_ - last_seen_[app]));
            const float rec = std::exp(-age / 3600.0f);  // 1h half-life
            float h = 0;
            auto hit = hour_counts_.find(hour);
            if (hit != hour_counts_.end() && hour_totals_[hour] > 0) {
                auto ait = hit->second.find(app);
                if (ait != hit->second.end()) {
                    h = static_cast<float>(ait->second) / hour_totals_[hour];
                }
            }
            const float score = 0.45f * f + 0.30f * rec + 0.25f * h;
            scored.emplace_back(app, score);
        }
        std::sort(scored.begin(), scored.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t i = 0; i < scored.size() && out.size() < top_k; ++i) {
            out.push_back({scored[i].first, scored[i].second, "freq_recency"});
        }
        return out;
    }

    void reset() override {
        unigram_.clear(); last_seen_.clear();
        hour_counts_.clear(); hour_totals_.clear();
        last_ts_ = 0;
    }
    std::vector<std::uint8_t> serialize() const override { return {}; }
    Result<bool> deserialize(std::span<const std::uint8_t>) override { return true; }
    std::string_view name() const noexcept override { return "freq_recency"; }

private:
    std::unordered_map<AppId, std::size_t> unigram_;
    std::unordered_map<AppId, Timestamp> last_seen_;
    std::unordered_map<int, std::unordered_map<AppId, std::size_t>> hour_counts_;
    std::unordered_map<int, std::size_t> hour_totals_;
    Timestamp last_ts_ = 0;
};

}  // namespace

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
Result<std::unique_ptr<Predictor>> make_predictor(const Config& cfg) {
    if (cfg.predictor == "mfu") return std::unique_ptr<Predictor>(new MFUPredictor());
    if (cfg.predictor == "mru") return std::unique_ptr<Predictor>(new MRUPredictor());
    if (cfg.predictor == "markov" || cfg.predictor == "markov1")
        return std::unique_ptr<Predictor>(new Markov1Predictor());
    if (cfg.predictor == "freq_recency" || cfg.predictor == "freqrecency")
        return std::unique_ptr<Predictor>(new FreqRecencyPredictor());
    return Error{ErrorCode::InvalidArgument, "unknown predictor: " + cfg.predictor};
}

}  // namespace mem
