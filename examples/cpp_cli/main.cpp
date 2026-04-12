// SPDX-License-Identifier: MIT
// Tiny CLI used for a live demo from the C++ side.
//
// Usage:
//   ./mem                       - print version
//   ./mem demo                  - seed a synthetic trace, print predictions + KPIs
#include <memogent/memogent.hpp>

#include <cstdlib>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

void run_demo() {
    mem::Config cfg;
    cfg.predictor = "markov1";
    cfg.cache_policy = "context_arc";
    cfg.cache_capacity_app = 16;

    auto r = mem::Orchestrator::create(cfg);
    if (!r) {
        std::fprintf(stderr, "init failed: %s\n", r.error().message.c_str());
        std::exit(1);
    }
    auto& orch = *r.value();

    // Morning, commute, evening clusters.
    const std::vector<std::string> morning = {
        "com.messenger", "com.browser", "com.mail", "com.news"
    };
    const std::vector<std::string> evening = {
        "com.video", "com.games", "com.music", "com.messenger"
    };

    std::mt19937 rng(0x5EED);
    auto pick = [&](const std::vector<std::string>& v) {
        return v[std::uniform_int_distribution<std::size_t>(0, v.size() - 1)(rng)];
    };

    for (int day = 0; day < 14; ++day) {
        for (int i = 0; i < 8; ++i) orch.record_app_open(pick(morning));
        for (int i = 0; i < 8; ++i) orch.record_app_open(pick(evening));
        orch.tick();
    }

    std::printf("memogent %s\n", mem::version_string);
    std::printf("predictions:\n");
    for (const auto& p : orch.predict_next(3)) {
        std::printf("  %-22s  score=%.2f  (%s)\n",
                    p.app_id.c_str(), p.score, p.reason.c_str());
    }

    auto stats = orch.app_cache_stats();
    std::printf("\ncache: hits=%zu misses=%zu hit_rate=%.2f size=%zu cap=%zu\n",
                stats.hits, stats.misses, stats.hit_rate(), stats.size, stats.capacity);
    std::printf("\nKPIs:\n%s\n", orch.kpis_json().c_str());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string{argv[1]} == "demo") {
        run_demo();
        return 0;
    }
    std::printf("memogent %s  --  run `%s demo` for a live example\n",
                mem::version_string, argv[0]);
    return 0;
}
