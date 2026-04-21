"""In-memory Telemetry — mirrors core/src/telemetry/telemetry.cpp.

Accumulates every Samsung KPI and exposes a snapshot + JSON dump.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Dict, List


@dataclass
class KpiSnapshot:
    app_load_time_ms_baseline: float = 0.0
    app_load_time_ms_current: float = 0.0
    app_load_time_improvement_pct: float = 0.0
    launch_time_ms_baseline: float = 0.0
    launch_time_ms_current: float = 0.0
    launch_time_improvement_pct: float = 0.0
    thrashing_events_baseline: float = 0.0
    thrashing_events_current: float = 0.0
    thrashing_reduction_pct: float = 0.0
    stability_issues: int = 0
    prediction_accuracy_top1: float = 0.0
    prediction_accuracy_top3: float = 0.0
    cache_hit_rate: float = 0.0
    memory_utilization_efficiency_pct: float = 0.0
    p50_decision_latency_ms: float = 0.0
    p95_decision_latency_ms: float = 0.0
    p99_decision_latency_ms: float = 0.0


@dataclass
class Telemetry:
    baseline_app_load_ms: float = 0.0
    current_app_load_ms: float = 0.0
    baseline_launch_ms: float = 0.0
    current_launch_ms: float = 0.0
    baseline_thrashing: float = 0.0
    current_thrashing: float = 0.0
    util_efficiency_pct: float = 0.0
    stability_issues: int = 0
    _pred_total: int = 0
    _pred_top1: int = 0
    _pred_top3: int = 0
    _caches: Dict[str, tuple[int, int]] = field(default_factory=dict)
    _latencies: Dict[str, List[float]] = field(default_factory=dict)

    # ---- mutators ----
    def record_prediction(self, top1: bool, top3: bool) -> None:
        self._pred_total += 1
        self._pred_top1 += int(top1)
        self._pred_top3 += int(top3)

    def record_cache(self, pool: str, hits: int, misses: int) -> None:
        self._caches[pool] = (hits, misses)

    def record_latency(self, kind: str, ms: float) -> None:
        self._latencies.setdefault(kind, []).append(ms)

    def record_stability_issue(self, _reason: str) -> None:
        self.stability_issues += 1

    def record_thrash(self, events: int) -> None:
        self.current_thrashing += events

    # ---- snapshot ----
    def snapshot(self) -> KpiSnapshot:
        k = KpiSnapshot()
        k.prediction_accuracy_top1 = (
            self._pred_top1 / self._pred_total if self._pred_total else 0.0
        )
        k.prediction_accuracy_top3 = (
            self._pred_top3 / self._pred_total if self._pred_total else 0.0
        )
        total_h = sum(h for h, _ in self._caches.values())
        total_m = sum(m for _, m in self._caches.values())
        k.cache_hit_rate = total_h / (total_h + total_m) if (total_h + total_m) else 0.0
        k.stability_issues = self.stability_issues
        k.thrashing_events_baseline = self.baseline_thrashing
        k.thrashing_events_current = self.current_thrashing
        k.thrashing_reduction_pct = (
            (self.baseline_thrashing - self.current_thrashing) * 100.0 / self.baseline_thrashing
            if self.baseline_thrashing
            else 0.0
        )
        k.app_load_time_ms_baseline = self.baseline_app_load_ms
        k.app_load_time_ms_current = self.current_app_load_ms
        if self.baseline_app_load_ms:
            k.app_load_time_improvement_pct = (
                (self.baseline_app_load_ms - self.current_app_load_ms)
                * 100.0
                / self.baseline_app_load_ms
            )
        k.launch_time_ms_baseline = self.baseline_launch_ms
        k.launch_time_ms_current = self.current_launch_ms
        if self.baseline_launch_ms:
            k.launch_time_improvement_pct = (
                (self.baseline_launch_ms - self.current_launch_ms)
                * 100.0
                / self.baseline_launch_ms
            )
        k.memory_utilization_efficiency_pct = self.util_efficiency_pct
        decisions = self._latencies.get("decision", [])
        if decisions:
            sorted_d = sorted(decisions)
            k.p50_decision_latency_ms = sorted_d[len(sorted_d) // 2]
            k.p95_decision_latency_ms = sorted_d[min(len(sorted_d) - 1, int(len(sorted_d) * 0.95))]
            k.p99_decision_latency_ms = sorted_d[min(len(sorted_d) - 1, int(len(sorted_d) * 0.99))]
        return k

    def to_dict(self) -> dict:
        s = self.snapshot()
        return {
            "app_load_time_ms_baseline": s.app_load_time_ms_baseline,
            "app_load_time_ms_current": s.app_load_time_ms_current,
            "app_load_time_improvement_pct": s.app_load_time_improvement_pct,
            "launch_time_ms_baseline": s.launch_time_ms_baseline,
            "launch_time_ms_current": s.launch_time_ms_current,
            "launch_time_improvement_pct": s.launch_time_improvement_pct,
            "thrashing_events_baseline": s.thrashing_events_baseline,
            "thrashing_events_current": s.thrashing_events_current,
            "thrashing_reduction_pct": s.thrashing_reduction_pct,
            "stability_issues": s.stability_issues,
            "prediction_accuracy_top1": s.prediction_accuracy_top1,
            "prediction_accuracy_top3": s.prediction_accuracy_top3,
            "cache_hit_rate": s.cache_hit_rate,
            "memory_utilization_efficiency_pct": s.memory_utilization_efficiency_pct,
            "p50_decision_latency_ms": s.p50_decision_latency_ms,
            "p95_decision_latency_ms": s.p95_decision_latency_ms,
            "p99_decision_latency_ms": s.p99_decision_latency_ms,
        }

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent)
