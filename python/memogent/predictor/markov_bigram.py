"""Markov n=2 (bigram) predictor with backoff to unigram and time-of-day prior.

Captures sequence patterns of length 2 (e.g., "browser then mail then docs"),
which dominate real app-usage traces. Falls back to unigram + recency when
the bigram context is unseen, and blends a hour-of-day prior to handle the
"morning vs evening" split that pure n-grams miss.
"""
from __future__ import annotations

from collections import Counter, defaultdict
from typing import DefaultDict, List, Tuple

from ..types import AppEvent, EventType, Prediction
from .base import Predictor


class MarkovBigramPredictor(Predictor):
    name = "markov2"

    def __init__(
        self,
        bigram_weight: float = 0.55,
        unigram_weight: float = 0.20,
        recency_weight: float = 0.15,
        hour_weight: float = 0.10,
    ) -> None:
        self._bigram: DefaultDict[Tuple[str, str], Counter[str]] = defaultdict(Counter)
        self._unigram_next: DefaultDict[str, Counter[str]] = defaultdict(Counter)
        self._unigram: Counter[str] = Counter()
        self._hour_counts: DefaultDict[int, Counter[str]] = defaultdict(Counter)
        self._last_idx: DefaultDict[str, int] = defaultdict(int)
        self._n = 0
        self._prev1: str = ""
        self._prev2: str = ""
        self._w_bi = bigram_weight
        self._w_uni = unigram_weight
        self._w_rec = recency_weight
        self._w_hr = hour_weight

    def observe(self, ev: AppEvent) -> None:
        if ev.type not in (EventType.APP_OPEN, EventType.APP_FOREGROUND):
            return
        app = ev.app_id
        self._n += 1
        self._unigram[app] += 1
        self._last_idx[app] = self._n
        if self._prev1:
            self._unigram_next[self._prev1][app] += 1
        if self._prev1 and self._prev2:
            self._bigram[(self._prev2, self._prev1)][app] += 1
        if ev.hour_of_day is not None:
            self._hour_counts[ev.hour_of_day][app] += 1
        self._last_hour = ev.hour_of_day
        self._prev2 = self._prev1
        self._prev1 = app

    def predict(self, top_k: int) -> List[Prediction]:
        if not self._prev1:
            return []
        candidates: Counter[str] = Counter()

        bi_ctx = self._bigram.get((self._prev2, self._prev1)) if self._prev2 else None
        if bi_ctx:
            total = sum(bi_ctx.values()) + 1
            for app, c in bi_ctx.items():
                candidates[app] += self._w_bi * (c / total)

        uni_ctx = self._unigram_next.get(self._prev1)
        if uni_ctx:
            total = sum(uni_ctx.values()) + 1
            for app, c in uni_ctx.items():
                candidates[app] += self._w_uni * (c / total)

        if self._unigram:
            grand = sum(self._unigram.values())
            for app, c in self._unigram.items():
                rec = 1.0 / (1.0 + (self._n - self._last_idx.get(app, 0)) / 50.0)
                candidates[app] += self._w_rec * rec * (c / grand)

        hr = getattr(self, "_last_hour", None)
        if hr is not None and self._hour_counts.get(hr):
            hctx = self._hour_counts[hr]
            total = sum(hctx.values()) + 1
            for app, c in hctx.items():
                candidates[app] += self._w_hr * (c / total)

        if not candidates:
            return []
        ranked = candidates.most_common(top_k)
        return [Prediction(app_id=a, score=float(s), reason="markov2") for a, s in ranked]

    def reset(self) -> None:
        self._bigram.clear()
        self._unigram_next.clear()
        self._unigram.clear()
        self._hour_counts.clear()
        self._last_idx.clear()
        self._n = 0
        self._prev1 = ""
        self._prev2 = ""
