"""Parity with core/include/memogent/types.hpp."""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


class EventType(IntEnum):
    APP_OPEN = 0
    APP_CLOSE = 1
    APP_BACKGROUND = 2
    APP_FOREGROUND = 3
    USER_INTERACTION = 4
    MODEL_LOAD = 5
    MODEL_UNLOAD = 6
    MODEL_INVOKE = 7
    CACHE_HIT = 8
    CACHE_MISS = 9
    PRELOAD = 10
    EVICT = 11


class PowerMode(IntEnum):
    FULL = 0
    REDUCED = 1
    MINIMAL = 2
    PAUSED = 3


class ResourcePool(IntEnum):
    APP_STATE = 0
    MODEL_WEIGHTS = 1
    KV_CACHE = 2
    EMBEDDING = 3
    OTHER = 255


@dataclass
class AppEvent:
    type: EventType = EventType.USER_INTERACTION
    app_id: str = ""
    model_id: str = ""
    timestamp: float = 0.0
    hour_of_day: Optional[int] = None
    battery_pct: float = 100.0
    thermal: float = 0.25
    screen_state: Optional[str] = None
    location: Optional[str] = None


@dataclass
class Prediction:
    app_id: str
    score: float = 0.0
    reason: str = ""


@dataclass
class DeviceState:
    battery_pct: float = 100.0
    thermal: float = 0.25
    ram_free_mb: float = 1024.0
    charging: bool = False
    low_power_mode: bool = False
    timestamp: float = 0.0


@dataclass
class CacheStats:
    hits: int = 0
    misses: int = 0
    evictions: int = 0
    size: int = 0
    capacity: int = 0

    @property
    def hit_rate(self) -> float:
        total = self.hits + self.misses
        return 0.0 if total == 0 else self.hits / total
