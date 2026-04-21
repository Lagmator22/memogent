"""Top-level orchestrator for the Python reference runtime."""
from __future__ import annotations

import time
from dataclasses import dataclass
from typing import List, Optional

from .arbiter import ArbiterInputs, ArbiterPlan, make_arbiter
from .cache import make_cache
from .config import Config
from .predictor import make_predictor
from .telemetry import KpiSnapshot, Telemetry
from .types import (
    AppEvent,
    CacheStats,
    DeviceState,
    EventType,
    PowerMode,
    Prediction,
    ResourcePool,
)


def _classify(state: DeviceState, cfg: Config) -> PowerMode:
    if (
        state.thermal >= cfg.qos_thermal_paused
        or (state.battery_pct <= cfg.qos_battery_paused and not state.charging)
        or state.ram_free_mb <= cfg.qos_ram_paused_mb
    ):
        return PowerMode.PAUSED
    if (
        state.thermal >= cfg.qos_thermal_minimal
        or (state.battery_pct <= cfg.qos_battery_minimal and not state.charging)
        or state.ram_free_mb <= cfg.qos_ram_minimal_mb
        or state.low_power_mode
    ):
        return PowerMode.MINIMAL
    if state.thermal >= cfg.qos_thermal_reduced or (
        state.battery_pct <= cfg.qos_battery_reduced and not state.charging
    ):
        return PowerMode.REDUCED
    return PowerMode.FULL


@dataclass
class TickResult:
    plan: ArbiterPlan
    power_mode: PowerMode
    latency_ms: float


class Orchestrator:
    def __init__(self, cfg: Optional[Config] = None) -> None:
        self.cfg = cfg or Config()
        self.cfg.ensure_dirs()
        self.predictor = make_predictor(self.cfg)
        self.app_cache = make_cache(self.cfg, self.cfg.cache_capacity_app)
        self.model_cache = make_cache(self.cfg, self.cfg.cache_capacity_model)
        self.arbiter = make_arbiter(self.cfg)
        self.telemetry = Telemetry()
        self._device = DeviceState()
        self._mode = PowerMode.FULL

    # ------------------- ingestion -------------------
    def record_event(self, ev: AppEvent) -> None:
        self.predictor.observe(ev)

    def record_app_open(self, app_id: str) -> None:
        self.record_event(AppEvent(type=EventType.APP_OPEN, app_id=app_id, timestamp=time.time()))

    def record_app_close(self, app_id: str) -> None:
        self.record_event(AppEvent(type=EventType.APP_CLOSE, app_id=app_id, timestamp=time.time()))

    def record_cache_hit(self, pool: ResourcePool) -> None:
        s = self.app_cache.stats() if pool == ResourcePool.APP_STATE else self.model_cache.stats()
        self.telemetry.record_cache(pool.name.lower(), s.hits + 1, s.misses)

    def record_cache_miss(self, pool: ResourcePool) -> None:
        s = self.app_cache.stats() if pool == ResourcePool.APP_STATE else self.model_cache.stats()
        self.telemetry.record_cache(pool.name.lower(), s.hits, s.misses + 1)

    def update_device_state(self, state: DeviceState) -> None:
        self._device = state
        self._mode = _classify(state, self.cfg)

    # ------------------- queries -------------------
    def predict_next(self, k: Optional[int] = None) -> List[Prediction]:
        return self.predictor.predict(k or self.cfg.prediction_top_k)

    def power_mode(self) -> PowerMode:
        return self._mode

    # ------------------- tick -------------------
    def tick(self) -> TickResult:
        start = time.perf_counter()
        preds = self.predictor.predict(self.cfg.prediction_top_k)
        inp = ArbiterInputs(
            device=self._device,
            mode=self._mode,
            predictions=preds,
            app_cache=self.app_cache.stats(),
            model_cache=self.model_cache.stats(),
        )
        plan = self.arbiter.decide(inp)
        self.app_cache.resize(plan.app_cache_capacity)
        self.model_cache.resize(plan.model_cache_capacity)
        if plan.allow_preload:
            self.app_cache.hint_future(preds)
            # materialize top-k preloads as single-byte placeholders
            for i, p in enumerate(preds[: self.cfg.preload_top_k]):
                key = f"warm:{p.app_id}"
                if self.app_cache.get(key) is None:
                    self.app_cache.put(key, b"w")
        latency_ms = (time.perf_counter() - start) * 1000.0
        self.telemetry.record_latency("decision", latency_ms)
        ac = self.app_cache.stats()
        self.telemetry.record_cache("app", ac.hits, ac.misses)
        mc = self.model_cache.stats()
        self.telemetry.record_cache("model", mc.hits, mc.misses)
        return TickResult(plan=plan, power_mode=self._mode, latency_ms=latency_ms)

    # ------------------- introspection -------------------
    def app_cache_stats(self) -> CacheStats:
        return self.app_cache.stats()

    def kpis(self) -> KpiSnapshot:
        return self.telemetry.snapshot()

    def kpis_json(self) -> str:
        return self.telemetry.to_json()

    def reset(self) -> None:
        self.predictor.reset()
        self.app_cache.clear()
        self.model_cache.clear()
        self.telemetry = Telemetry()
