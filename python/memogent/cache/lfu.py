"""Approximate LFU with recency tiebreak."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Optional

from ..types import CacheStats
from .base import AdaptiveCache


@dataclass
class _Item:
    value: bytes
    freq: int
    ts: int


class LFUCache(AdaptiveCache):
    policy_name = "lfu"

    def __init__(self, capacity: int) -> None:
        self._cap = capacity
        self._store: Dict[str, _Item] = {}
        self._tick = 0
        self._hits = self._misses = self._evictions = 0

    def get(self, key: str) -> Optional[bytes]:
        it = self._store.get(key)
        if it is None:
            self._misses += 1
            return None
        self._hits += 1
        it.freq += 1
        self._tick += 1
        it.ts = self._tick
        return it.value

    def put(self, key: str, value: bytes) -> None:
        it = self._store.get(key)
        if it is not None:
            it.value = value
            it.freq += 1
            self._tick += 1
            it.ts = self._tick
            return
        if len(self._store) >= self._cap:
            self._evict()
        self._tick += 1
        self._store[key] = _Item(value=value, freq=1, ts=self._tick)

    def erase(self, key: str) -> None:
        self._store.pop(key, None)

    def resize(self, capacity: int) -> None:
        self._cap = capacity
        while len(self._store) > self._cap:
            self._evict()

    def clear(self) -> None:
        self._store.clear()
        self._tick = 0
        self._hits = self._misses = self._evictions = 0

    def stats(self) -> CacheStats:
        return CacheStats(
            hits=self._hits,
            misses=self._misses,
            evictions=self._evictions,
            size=len(self._store),
            capacity=self._cap,
        )

    def _evict(self) -> None:
        if not self._store:
            return
        victim_key = min(
            self._store,
            key=lambda k: (self._store[k].freq, self._store[k].ts),
        )
        self._store.pop(victim_key, None)
        self._evictions += 1
