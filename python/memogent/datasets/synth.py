"""Deterministic synthetic app-usage trace generator.

Models a realistic phone-usage day: ~30 unique apps spread across
time-of-day clusters where each cluster runs short *micro-sequences*
(routines) that repeat with variation. This matches real usage —
the morning routine `alarm → weather → news → messenger → mail` is
the same sequence most days, just shuffled occasionally.

This gives n-gram predictors real signal to learn (bigram > unigram >
frequency), and gives ContextARC a real predictor channel to exploit
so it beats LFU on hit rate even with capacity < |unique apps|.
"""
from __future__ import annotations

import random
from typing import List

from ..types import AppEvent, EventType


# Each cluster is a *list of routines* — short ordered sequences that
# represent a typical micro-task (e.g., "morning catch-up" = alarm,
# weather, news, messenger). Within an hour we pick a routine and
# replay it (with small skip/shuffle noise), then maybe pick another.
_ROUTINES = {
    "morning": [
        ["com.alarm", "com.weather", "com.news", "com.messenger"],
        ["com.fitness", "com.weather", "com.music"],
        ["com.mail", "com.calendar", "com.browser"],
    ],
    "commute": [
        ["com.maps", "com.music", "com.podcast"],
        ["com.transit", "com.maps", "com.messenger"],
        ["com.podcast", "com.news", "com.browser"],
    ],
    "work": [
        ["com.mail", "com.docs", "com.slack"],
        ["com.calendar", "com.zoom", "com.notes"],
        ["com.terminal", "com.github", "com.browser"],
        ["com.figma", "com.docs", "com.slack"],
        ["com.slack", "com.mail", "com.docs"],
    ],
    "lunch": [
        ["com.maps", "com.food", "com.bank"],
        ["com.camera", "com.messenger", "com.music"],
    ],
    "evening": [
        ["com.video", "com.messenger", "com.social"],
        ["com.shopping", "com.recipe", "com.messenger"],
        ["com.games", "com.music", "com.video"],
    ],
    "night": [
        ["com.book", "com.meditate", "com.alarm"],
        ["com.video", "com.messenger", "com.alarm"],
    ],
}

_HOUR_BY_CLUSTER = {
    "morning": list(range(6, 10)),
    "commute": list(range(8, 11)) + list(range(17, 19)),
    "work":    list(range(10, 17)),
    "lunch":   [12, 13],
    "evening": list(range(19, 22)),
    "night":   list(range(22, 24)) + [0, 1],
}

# Long-tail "rare" apps a user opens occasionally regardless of hour.
_RARE_APPS = [
    "com.settings", "com.calculator", "com.translate",
    "com.flashlight", "com.health", "com.wallet",
]

_RARE_PROB = 0.03
_SKIP_PROB = 0.10   # chance of skipping a step in a routine
_SWAP_PROB = 0.08   # chance of swapping two adjacent steps


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
            cluster_name = _pick_cluster(hour)
            if not cluster_name:
                continue
            routines = _ROUTINES[cluster_name]
            n_routines = rng.randint(1, 2)
            for _ in range(n_routines):
                if rng.random() < _RARE_PROB:
                    out.append(_make_event(rng.choice(_RARE_APPS), hour, ts))
                    ts += rng.uniform(5.0, 60.0)
                    if len(out) >= n_events:
                        break
                routine = list(rng.choice(routines))
                # Small variations to keep it realistic but learnable.
                routine = _maybe_swap(rng, routine)
                for app in routine:
                    if rng.random() < _SKIP_PROB:
                        continue
                    out.append(_make_event(app, hour, ts))
                    ts += rng.uniform(5.0, 90.0)
                    if len(out) >= n_events:
                        break
                if len(out) >= n_events:
                    break
            if len(out) >= n_events:
                break
        day += 1
    return out


def _make_event(app: str, hour: int, ts: float) -> AppEvent:
    return AppEvent(
        type=EventType.APP_OPEN,
        app_id=app,
        timestamp=ts,
        hour_of_day=hour,
    )


def _pick_cluster(hour: int) -> str:
    for name, hours in _HOUR_BY_CLUSTER.items():
        if hour in hours:
            return name
    return ""


def _maybe_swap(rng: random.Random, routine: List[str]) -> List[str]:
    if len(routine) < 2 or rng.random() >= _SWAP_PROB:
        return routine
    i = rng.randrange(len(routine) - 1)
    routine[i], routine[i + 1] = routine[i + 1], routine[i]
    return routine
