"""Textbook LRU using OrderedDict."""
from __future__ import annotations

from collections import OrderedDict
from typing import Optional

from ..types import CacheStats
from .base import AdaptiveCache


class LRUCache(AdaptiveCache):
    policy_name = "lru"

    def __init__(self, capacity: int) -> None:
        self._cap = capacity
        self._store: "OrderedDict[str, bytes]" = OrderedDict()
        self._hits = self._misses = self._evictions = 0

    def get(self, key: str) -> Optional[bytes]:
        if key in self._store:
            self._hits += 1
            self._store.move_to_end(key, last=False)
            return self._store[key]
        self._misses += 1
        return None

    def put(self, key: str, value: bytes) -> None:
        if key in self._store:
            self._store[key] = value
            self._store.move_to_end(key, last=False)
            return
        self._store[key] = value
        self._store.move_to_end(key, last=False)
        while len(self._store) > self._cap:
            self._store.popitem(last=True)
            self._evictions += 1

    def erase(self, key: str) -> None:
        self._store.pop(key, None)

    def resize(self, capacity: int) -> None:
        self._cap = capacity
        while len(self._store) > self._cap:
            self._store.popitem(last=True)
            self._evictions += 1

    def clear(self) -> None:
        self._store.clear()
        self._hits = self._misses = self._evictions = 0

    def stats(self) -> CacheStats:
        return CacheStats(
            hits=self._hits,
            misses=self._misses,
            evictions=self._evictions,
            size=len(self._store),
            capacity=self._cap,
        )
