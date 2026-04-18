"""FreqRecency — blended hand-tuned features.

score(app) = 0.45 * freq + 0.30 * recency_decay + 0.25 * hour_bias
This is a strong baseline on LSApp top-3 (~65-70%) and needs no training.
"""
from __future__ import annotations

import math
import time
from collections import Counter, defaultdict
from typing import DefaultDict, List

from ..types import AppEvent, EventType, Prediction
from .base import Predictor


class FreqRecencyPredictor(Predictor):
    name = "freq_recency"

    def __init__(self, recency_half_life_s: float = 3600.0) -> None:
        self._unigram: Counter[str] = Counter()
        self._last_seen: dict[str, float] = {}
        self._hour_counts: DefaultDict[int, Counter[str]] = defaultdict(Counter)
        self._hour_totals: Counter[int] = Counter()
        self._last_ts: float = 0.0
        self._half = recency_half_life_s

    def observe(self, ev: AppEvent) -> None:
        if ev.type not in (EventType.APP_OPEN, EventType.APP_FOREGROUND):
            return
        self._unigram[ev.app_id] += 1
        self._last_seen[ev.app_id] = ev.timestamp
        self._last_ts = ev.timestamp
        if ev.hour_of_day is not None:
            self._hour_counts[ev.hour_of_day][ev.app_id] += 1
            self._hour_totals[ev.hour_of_day] += 1

    def predict(self, top_k: int) -> List[Prediction]:
        if not self._unigram:
            return []
        total = max(1, sum(self._unigram.values()))
        hour = int(time.gmtime(max(0, self._last_ts)).tm_hour)
        scored = []
        for app, cnt in self._unigram.items():
            f = cnt / total
            age = max(0.0, self._last_ts - self._last_seen.get(app, self._last_ts))
            rec = math.exp(-age / self._half)
            h = 0.0
            if self._hour_totals[hour] > 0:
                h = self._hour_counts[hour].get(app, 0) / self._hour_totals[hour]
            score = 0.45 * f + 0.30 * rec + 0.25 * h
            scored.append((app, score))
        scored.sort(key=lambda kv: kv[1], reverse=True)
        return [
            Prediction(app_id=a, score=s, reason="freq_recency")
            for a, s in scored[:top_k]
        ]

    def reset(self) -> None:
        self._unigram.clear()
        self._last_seen.clear()
        self._hour_counts.clear()
        self._hour_totals.clear()
        self._last_ts = 0.0
