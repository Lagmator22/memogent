// SPDX-License-Identifier: MIT
#include <memogent/predictor.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("predictor factory rejects unknown names") {
    mem::Config cfg;
    cfg.predictor = "bogus";
    auto r = mem::make_predictor(cfg);
    REQUIRE_FALSE(r);
}

TEST_CASE("MFU predicts most frequent app") {
    mem::Config cfg;
    cfg.predictor = "mfu";
    auto p = mem::make_predictor(cfg).value();
    mem::AppEvent ev{};
    ev.type = mem::EventType::AppOpen;
    for (const auto& app : {"a", "a", "b", "a", "c"}) {
        ev.app_id = app;
        p->observe(ev);
    }
    auto preds = p->predict(1);
    REQUIRE(!preds.empty());
    REQUIRE(preds[0].app_id == "a");
}

TEST_CASE("Markov captures a simple cycle") {
    mem::Config cfg;
    cfg.predictor = "markov1";
    auto p = mem::make_predictor(cfg).value();
    mem::AppEvent ev{};
    ev.type = mem::EventType::AppOpen;
    for (int i = 0; i < 10; ++i) {
        for (const auto& app : {"a", "b", "c"}) {
            ev.app_id = app;
            p->observe(ev);
        }
    }
    auto preds = p->predict(1);
    REQUIRE(!preds.empty());
    // last observed is "c", so next likely is "a"
    REQUIRE(preds[0].app_id == "a");
}
