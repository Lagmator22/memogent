"""Most-frequently-used baseline."""
from __future__ import annotations

from collections import Counter
from typing import List

from ..types import AppEvent, EventType, Prediction
from .base import Predictor


class MFUPredictor(Predictor):
    name = "mfu"

    def __init__(self) -> None:
        self._counts: Counter[str] = Counter()
        self._total = 0

    def observe(self, ev: AppEvent) -> None:
        if ev.type in (EventType.APP_OPEN, EventType.APP_FOREGROUND):
            self._counts[ev.app_id] += 1
            self._total += 1

    def predict(self, top_k: int) -> List[Prediction]:
        if self._total == 0:
            return []
        out: List[Prediction] = []
        for app, c in self._counts.most_common(top_k):
            out.append(Prediction(app_id=app, score=c / self._total, reason="mfu"))
        return out

    def reset(self) -> None:
        self._counts.clear()
        self._total = 0
