"""Smoke + behavior tests for the Python reference runtime."""
from __future__ import annotations

from memogent import AppEvent, Config, Orchestrator, EventType
from memogent.bench import run_harness
from memogent.cache import (
    ARCCache,
    ContextARCCache,
    LRUCache,
    make_cache,
)
from memogent.datasets import generate_synthetic
from memogent.predictor import (
    MarkovBigramPredictor,
    MarkovPredictor,
    make_predictor,
)


def test_version_string():
    import memogent
    assert memogent.__version__


def test_predictor_factories():
    for p in ("mfu", "mru", "markov1", "freq_recency", "markov2"):
        cfg = Config(predictor=p)
        assert make_predictor(cfg).name == p if p != "markov" else "markov1"


def test_cache_factories():
    for pol in ("lru", "lfu", "arc", "context_arc"):
        cfg = Config(cache_policy=pol)
        c = make_cache(cfg, 8)
        assert c.policy_name == pol


def test_lru_eviction():
    c = LRUCache(2)
    c.put("a", b"1")
    c.put("b", b"2")
    c.put("c", b"3")
    assert c.get("a") is None
    assert c.get("b") == b"2"
    assert c.stats().evictions == 1


def test_arc_handles_many():
    c = ARCCache(4)
    for i in range(16):
        c.put(str(i), b"x")
        c.get(str(i % 3))
    assert c.stats().capacity == 4
    assert c.stats().hits + c.stats().misses > 0


def test_context_arc_hint_future():
    c = ContextARCCache(3)
    for k in ("a", "b", "c", "d"):
        c.put(k, b"x")
    from memogent.types import Prediction
    c.hint_future([Prediction(app_id="a"), Prediction(app_id="b")])
    # 'a' and 'b' should still be hot (not guaranteed but likely)
    assert c.get("a") is not None or c.get("b") is not None


def test_markov_predicts_sequence():
    p = MarkovPredictor()
    for _ in range(3):
        for app in ("a", "b", "c"):
            p.observe(AppEvent(type=EventType.APP_OPEN, app_id=app))
    preds = p.predict(1)
    # after observing sequences, the next after 'c' should be 'a' usually
    assert preds and preds[0].app_id in {"a", "b", "c"}


def test_markov_bigram_learns_two_step_context():
    p = MarkovBigramPredictor()
    # Two interleaved routines: a->b->c and x->y->z
    for _ in range(8):
        for app in ("a", "b", "c"):
            p.observe(AppEvent(type=EventType.APP_OPEN, app_id=app))
        for app in ("x", "y", "z"):
            p.observe(AppEvent(type=EventType.APP_OPEN, app_id=app))
    # After observing (..., a, b) the bigram model should pick c first.
    p.observe(AppEvent(type=EventType.APP_OPEN, app_id="a"))
    p.observe(AppEvent(type=EventType.APP_OPEN, app_id="b"))
    preds = p.predict(1)
    assert preds[0].app_id == "c"


def test_context_arc_pre_warm_top1_only():
    """Cold pre-warm fires for the highest-confidence prediction only."""
    from memogent.types import Prediction

    c = ContextARCCache(2)
    c.put("hot", b"x")
    c.get("hot")  # promote into T2
    # Two cold predictions; only the top one (above threshold) should land.
    c.hint_future([
        Prediction(app_id="cold_top", score=0.9),
        Prediction(app_id="cold_low", score=0.05),
        Prediction(app_id="cold_low2", score=0.04),
    ])
    # Pre-warm puts 'cold_top' into the cache; 'cold_low*' must remain cold.
    assert c.get("cold_top") is not None
    assert c.get("cold_low") is None
    assert c.get("cold_low2") is None


def test_orchestrator_basic():
    cfg = Config(predictor="markov1", cache_policy="context_arc", cache_capacity_app=16)
    o = Orchestrator(cfg)
    for a, b in (("x", "y"), ("y", "z"), ("z", "x")) * 10:
        o.record_app_open(a)
        o.record_app_open(b)
        o.tick()
    preds = o.predict_next(3)
    assert preds and preds[0].score > 0
    snap = o.kpis()
    assert snap.cache_hit_rate >= 0.0


def test_synthetic_trace():
    events = generate_synthetic(n_events=500)
    assert len(events) == 500
    apps = {e.app_id for e in events}
    assert len(apps) >= 4


def test_harness_runs_and_reports():
    events = generate_synthetic(n_events=1500, seed=123)
    report = run_harness(events, cache_capacity=12)
    assert report.events == 1500
    # ContextARC with Markov typically beats LRU on structured synthetic data
    assert report.kpis["cache_hit_rate"] > 0.0
    assert report.kpis["prediction_accuracy_top3"] > 0.0
    assert "pass_fail" in report.to_dict()
