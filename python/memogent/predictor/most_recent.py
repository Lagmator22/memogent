"""Most-recently-used baseline."""
from __future__ import annotations

from collections import deque
from typing import Deque, List

from ..types import AppEvent, EventType, Prediction
from .base import Predictor


class MRUPredictor(Predictor):
    name = "mru"

    def __init__(self, cap: int = 256) -> None:
        self._history: Deque[str] = deque(maxlen=cap)

    def observe(self, ev: AppEvent) -> None:
        if ev.type in (EventType.APP_OPEN, EventType.APP_FOREGROUND):
            self._history.append(ev.app_id)

    def predict(self, top_k: int) -> List[Prediction]:
        if not self._history:
            return []
        seen = {}
        for i, app in enumerate(self._history):
            seen[app] = i  # last index wins
        ordered = sorted(seen.items(), key=lambda kv: kv[1], reverse=True)
        out: List[Prediction] = []
        for i, (app, _) in enumerate(ordered[:top_k]):
            score = max(0.0, 1.0 - i / max(1, len(ordered)))
            out.append(Prediction(app_id=app, score=score, reason="mru"))
        return out

    def reset(self) -> None:
        self._history.clear()
