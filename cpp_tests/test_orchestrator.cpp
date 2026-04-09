// SPDX-License-Identifier: MIT
#include <memogent/memogent.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("orchestrator lifecycle") {
    mem::Config cfg;
    cfg.predictor = "markov1";
    cfg.cache_policy = "context_arc";
    auto r = mem::Orchestrator::create(cfg);
    REQUIRE(r);
    auto& o = *r.value();

    for (const auto& a : {"x", "y", "z", "x", "y", "z", "x", "y", "z"}) {
        o.record_app_open(a);
    }
    auto preds = o.predict_next(3);
    REQUIRE_FALSE(preds.empty());
    auto plan = o.tick();
    REQUIRE(plan.app_cache_capacity > 0);
    REQUIRE(o.current_power_mode() == mem::PowerMode::Full);
    REQUIRE(!o.kpis_json().empty());
}

TEST_CASE("device pressure degrades QoS") {
    mem::Config cfg;
    auto r = mem::Orchestrator::create(cfg);
    REQUIRE(r);
    auto& o = *r.value();
    mem::DeviceState hot{};
    hot.battery_pct = 10.0f;
    hot.thermal = 0.92f;
    hot.ram_free_mb = 30.0f;
    hot.charging = false;
    o.update_device_state(hot);
    REQUIRE(o.current_power_mode() == mem::PowerMode::Paused);
}
