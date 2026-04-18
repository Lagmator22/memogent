"""Markov n=1 predictor — strong interpretable baseline."""
from __future__ import annotations

from collections import Counter, defaultdict
from typing import DefaultDict, List

from ..types import AppEvent, EventType, Prediction
from .base import Predictor


class MarkovPredictor(Predictor):
    name = "markov1"

    def __init__(self) -> None:
        self._transitions: DefaultDict[str, Counter[str]] = defaultdict(Counter)
        self._unigram: Counter[str] = Counter()
        self._grand_total = 0
        self._last: str = ""

    def observe(self, ev: AppEvent) -> None:
        if ev.type not in (EventType.APP_OPEN, EventType.APP_FOREGROUND):
            return
        if self._last:
            self._transitions[self._last][ev.app_id] += 1
        self._unigram[ev.app_id] += 1
        self._grand_total += 1
        self._last = ev.app_id

    def predict(self, top_k: int) -> List[Prediction]:
        if not self._last:
            return []
        tr = self._transitions.get(self._last)
        if not tr:
            # fallback: global unigram
            return [
                Prediction(
                    app_id=a,
                    score=(c / self._grand_total) if self._grand_total else 0.0,
                    reason="markov1/fallback",
                )
                for a, c in self._unigram.most_common(top_k)
            ]
        total = sum(tr.values()) + len(self._unigram)
        scored = [
            (app, (c + 1.0) / total) for app, c in tr.items()
        ]
        scored.sort(key=lambda kv: kv[1], reverse=True)
        return [
            Prediction(app_id=a, score=s, reason="markov1") for a, s in scored[:top_k]
        ]

    def reset(self) -> None:
        self._transitions.clear()
        self._unigram.clear()
        self._grand_total = 0
        self._last = ""
