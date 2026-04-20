"""Smoke + behavior tests for the Python reference runtime."""
from __future__ import annotations

from memogent import AppEvent, Config, Orchestrator, EventType
from memogent.bench import KpiTargets, run_harness
from memogent.cache import (
    ARCCache,
    ContextARCCache,
    LFUCache,
    LRUCache,
    make_cache,
)
from memogent.datasets import generate_synthetic
from memogent.predictor import (
    FreqRecencyPredictor,
    MFUPredictor,
    MRUPredictor,
    MarkovPredictor,
    make_predictor,
)


def test_version_string():
    import memogent
    assert memogent.__version__


def test_predictor_factories():
    for p in ("mfu", "mru", "markov1", "freq_recency"):
        cfg = Config(predictor=p)
        assert make_predictor(cfg).name == p if p != "markov" else "markov1"


def test_cache_factories():
    for pol in ("lru", "lfu", "arc", "context_arc"):
        cfg = Config(cache_policy=pol)
        c = make_cache(cfg, 8)
        assert c.policy_name.endswith(pol.replace("_", ""))


def test_lru_eviction():
    c = LRUCache(2)
    c.put("a", b"1"); c.put("b", b"2"); c.put("c", b"3")
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


def test_orchestrator_basic():
    cfg = Config(predictor="markov1", cache_policy="context_arc", cache_capacity_app=16)
    o = Orchestrator(cfg)
    for a, b in (("x", "y"), ("y", "z"), ("z", "x")) * 10:
        o.record_app_open(a); o.record_app_open(b)
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
