"""Deterministic synthetic app-usage trace generator.

We model a handful of daily-routine patterns: morning messenger + browser,
lunch map + music, evening video + games. This is enough to make ARC /
ContextARC beat LRU / MRU in the KPI harness, which is the only thing CI
needs to prove.
"""
from __future__ import annotations

import random
from typing import Iterator, List

from ..types import AppEvent, EventType


_CLUSTERS = {
    "morning": ["com.messenger", "com.browser", "com.mail", "com.news", "com.weather"],
    "commute": ["com.maps", "com.music", "com.messenger", "com.news"],
    "work":    ["com.mail", "com.docs", "com.browser", "com.calendar", "com.slack"],
    "lunch":   ["com.maps", "com.music", "com.food", "com.browser"],
    "evening": ["com.video", "com.games", "com.music", "com.messenger"],
}

_HOUR_BY_CLUSTER = {
    "morning": list(range(6, 10)),
    "commute": list(range(9, 11)) + list(range(17, 19)),
    "work":    list(range(10, 17)),
    "lunch":   [12, 13],
    "evening": list(range(19, 23)),
}


def generate_synthetic(
    n_events: int = 5_000,
    n_days: int = 60,
    seed: int = 42,
) -> List[AppEvent]:
    rng = random.Random(seed)
    out: List[AppEvent] = []
    day = 0
    ts = 0.0
    while len(out) < n_events and day < n_days:
        for hour in range(24):
            cluster_name, apps = _pick_cluster(hour)
            if not apps:
                continue
            k = rng.randint(1, 5)
            for _ in range(k):
                app = rng.choices(apps, weights=_weights(apps), k=1)[0]
                out.append(
                    AppEvent(
                        type=EventType.APP_OPEN,
                        app_id=app,
                        timestamp=ts,
                        hour_of_day=hour,
                    )
                )
                ts += rng.uniform(5.0, 120.0)
                if len(out) >= n_events:
                    break
            if len(out) >= n_events:
                break
        day += 1
    return out


def _pick_cluster(hour: int) -> tuple[str, List[str]]:
    for name, hours in _HOUR_BY_CLUSTER.items():
        if hour in hours:
            return name, _CLUSTERS[name]
    return "", []


def _weights(apps: List[str]) -> List[float]:
    # heavier tail for first entries — favors a few "go-to" apps per cluster
    w = [max(0.1, 1.0 - 0.18 * i) for i in range(len(apps))]
    return w
