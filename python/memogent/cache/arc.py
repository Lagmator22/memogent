"""ARC (Megiddo & Modha 2003) and ContextARC.

ARC maintains two lists of live entries (T1, T2) and two ghost lists (B1, B2)
that remember recently evicted keys. A single parameter `p` adapts between
the two based on ghost hits. ContextARC adds a `hint_future()` hook used by
the Memogent preloader to proactively promote predicted-next entries into T2.
"""
from __future__ import annotations

from collections import OrderedDict
from typing import List, Optional

from ..types import CacheStats, Prediction
from .base import AdaptiveCache


class ARCCache(AdaptiveCache):
    policy_name = "arc"

    def __init__(self, capacity: int) -> None:
        self._cap = capacity
        self._p = 0
        self._t1: "OrderedDict[str, bytes]" = OrderedDict()
        self._t2: "OrderedDict[str, bytes]" = OrderedDict()
        self._b1: "OrderedDict[str, None]" = OrderedDict()
        self._b2: "OrderedDict[str, None]" = OrderedDict()
        self._hits = self._misses = self._evictions = 0

    # ---------------- public ----------------
    def get(self, key: str) -> Optional[bytes]:
        if key in self._t1:
            self._hits += 1
            v = self._t1.pop(key)
            self._t2[key] = v
            self._t2.move_to_end(key, last=False)
            return v
        if key in self._t2:
            self._hits += 1
            self._t2.move_to_end(key, last=False)
            return self._t2[key]
        self._misses += 1
        return None

    def put(self, key: str, value: bytes) -> None:
        if key in self._t1:
            v = self._t1.pop(key)  # noqa: F841
            self._t2[key] = value
            self._t2.move_to_end(key, last=False)
            return
        if key in self._t2:
            self._t2[key] = value
            self._t2.move_to_end(key, last=False)
            return
        if key in self._b1:
            self._p = min(self._cap, self._p + max(1, len(self._b2) // max(1, len(self._b1))))
            self._replace(key, False)
            self._b1.pop(key, None)
            self._t2[key] = value
            self._t2.move_to_end(key, last=False)
            return
        if key in self._b2:
            self._p = max(0, self._p - max(1, len(self._b1) // max(1, len(self._b2))))
            self._replace(key, True)
            self._b2.pop(key, None)
            self._t2[key] = value
            self._t2.move_to_end(key, last=False)
            return
        # fresh miss
        if len(self._t1) + len(self._b1) >= self._cap:
            if len(self._t1) < self._cap:
                if self._b1:
                    self._b1.popitem(last=True)
                self._replace(key, False)
            else:
                self._t1.popitem(last=True)
                self._evictions += 1
        else:
            total = len(self._t1) + len(self._t2) + len(self._b1) + len(self._b2)
            if total >= self._cap:
                if total == 2 * self._cap and self._b2:
                    self._b2.popitem(last=True)
                self._replace(key, False)
        self._t1[key] = value
        self._t1.move_to_end(key, last=False)

    def erase(self, key: str) -> None:
        for d in (self._t1, self._t2, self._b1, self._b2):
            d.pop(key, None)

    def resize(self, capacity: int) -> None:
        self._cap = capacity
        while len(self._t1) + len(self._t2) > self._cap:
            if self._t1:
                self._t1.popitem(last=True)
            elif self._t2:
                self._t2.popitem(last=True)
            self._evictions += 1

    def clear(self) -> None:
        self._t1.clear()
        self._t2.clear()
        self._b1.clear()
        self._b2.clear()
        self._p = 0
        self._hits = self._misses = self._evictions = 0

    def stats(self) -> CacheStats:
        return CacheStats(
            hits=self._hits,
            misses=self._misses,
            evictions=self._evictions,
            size=len(self._t1) + len(self._t2),
            capacity=self._cap,
        )

    # ---------------- internals ----------------
    def _replace(self, key: str, in_b2: bool) -> None:
        if self._t1 and (len(self._t1) > self._p or (in_b2 and len(self._t1) == self._p)):
            k, _ = self._t1.popitem(last=True)
            self._b1[k] = None
        elif self._t2:
            k, _ = self._t2.popitem(last=True)
            self._b2[k] = None
        self._evictions += 1


class ContextARCCache(ARCCache):
    policy_name = "context_arc"

    def hint_future(self, predictions: List[Prediction]) -> None:
        """Promote predicted-next keys into T2 so ARC keeps them hot."""
        for p in predictions:
            k = p.app_id
            if k in self._t1:
                v = self._t1.pop(k)
                self._t2[k] = v
                self._t2.move_to_end(k, last=False)
            elif k in self._t2:
                self._t2.move_to_end(k, last=False)
            # If the entry is not cached at all, the preloader materializes
            # it; we do nothing extra here.
