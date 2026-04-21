"""Arbiter implementations."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List

from ..config import Config
from ..types import CacheStats, DeviceState, PowerMode, Prediction


@dataclass
class ArbiterInputs:
    device: DeviceState = field(default_factory=DeviceState)
    mode: PowerMode = PowerMode.FULL
    predictions: List[Prediction] = field(default_factory=list)
    apps_resident: int = 0
    models_resident: int = 0
    kv_tokens: int = 0
    app_cache: CacheStats = field(default_factory=CacheStats)
    model_cache: CacheStats = field(default_factory=CacheStats)
    kv_cache: CacheStats = field(default_factory=CacheStats)


@dataclass
class ArbiterPlan:
    app_cache_capacity: int
    model_cache_capacity: int
    kv_cache_token_budget: int
    allow_preload: bool
    allow_consolidation: bool
    rationale: str


class HeuristicArbiter:
    name = "heuristic"

    def __init__(self, cfg: Config) -> None:
        self._cfg = cfg

    def decide(self, inp: ArbiterInputs) -> ArbiterPlan:
        cfg = self._cfg
        plan = ArbiterPlan(
            app_cache_capacity=cfg.cache_capacity_app,
            model_cache_capacity=cfg.cache_capacity_model,
            kv_cache_token_budget=cfg.kv_cache_token_budget,
            allow_preload=cfg.preload_enabled,
            allow_consolidation=True,
            rationale="baseline",
        )
        if inp.mode == PowerMode.REDUCED:
            plan.app_cache_capacity = plan.app_cache_capacity * 3 // 4
            plan.model_cache_capacity = max(1, plan.model_cache_capacity - 1)
            plan.kv_cache_token_budget = plan.kv_cache_token_budget * 3 // 4
            plan.rationale = "reduced: shrink app+model caches, keep preload"
        elif inp.mode == PowerMode.MINIMAL:
            plan.app_cache_capacity = plan.app_cache_capacity // 2
            plan.model_cache_capacity = 1
            plan.kv_cache_token_budget = plan.kv_cache_token_budget // 2
            plan.allow_preload = False
            plan.rationale = "minimal: single model, halved caches, no preload"
        elif inp.mode == PowerMode.PAUSED:
            plan.app_cache_capacity = max(4, plan.app_cache_capacity // 4)
            plan.model_cache_capacity = 0
            plan.kv_cache_token_budget = cfg.kv_cache_sink_tokens + cfg.kv_cache_recent_tokens
            plan.allow_preload = False
            plan.allow_consolidation = False
            plan.rationale = "paused: survival mode"
        if inp.predictions and inp.predictions[0].score > 0.70 and inp.mode <= PowerMode.REDUCED:
            plan.app_cache_capacity += 1
            plan.rationale += "; +1 slot for high-confidence preload"
        return plan


def make_arbiter(cfg: Config) -> HeuristicArbiter:
    if cfg.arbiter not in ("heuristic", "soft_budget"):
        raise ValueError(f"unknown arbiter: {cfg.arbiter}")
    return HeuristicArbiter(cfg)
