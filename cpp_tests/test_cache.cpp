// SPDX-License-Identifier: MIT
#include <memogent/adaptive_cache.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("LRU evicts the oldest item") {
    mem::Config cfg;
    cfg.cache_policy = "lru";
    auto c = mem::make_cache(cfg, 2).value();
    c->put("a", {'1'});
    c->put("b", {'2'});
    c->put("c", {'3'});
    REQUIRE_FALSE(c->get("a").has_value());
    REQUIRE(c->get("b").has_value());
    REQUIRE(c->get("c").has_value());
    auto s = c->stats();
    REQUIRE(s.evictions >= 1);
}

TEST_CASE("ARC handles ghost hits") {
    mem::Config cfg;
    cfg.cache_policy = "arc";
    auto c = mem::make_cache(cfg, 4).value();
    for (int i = 0; i < 32; ++i) {
        c->put(std::to_string(i), {static_cast<std::uint8_t>(i % 255)});
        c->get(std::to_string(i % 5));
    }
    auto s = c->stats();
    REQUIRE(s.capacity == 4);
    REQUIRE(s.size <= 4);
}

TEST_CASE("ContextARC accepts future hints") {
    mem::Config cfg;
    cfg.cache_policy = "context_arc";
    auto c = mem::make_cache(cfg, 3).value();
    c->put("a", {'1'});
    c->put("b", {'2'});
    c->put("c", {'3'});
    std::array<mem::Prediction, 2> hints = {
        mem::Prediction{"a", 0.9f, "unit"},
        mem::Prediction{"b", 0.5f, "unit"},
    };
    c->hint_future(hints);
    REQUIRE(c->get("a").has_value());
    REQUIRE(c->get("b").has_value());
}
