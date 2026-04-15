"""Cache factory."""
from __future__ import annotations

from ..config import Config
from .arc import ARCCache, ContextARCCache
from .base import AdaptiveCache
from .lfu import LFUCache
from .lru import LRUCache


def make_cache(cfg: Config, capacity: int) -> AdaptiveCache:
    name = cfg.cache_policy.lower()
    if name == "lru":
        return LRUCache(capacity)
    if name == "lfu":
        return LFUCache(capacity)
    if name == "arc":
        return ARCCache(capacity)
    if name in ("context_arc", "contextarc"):
        return ContextARCCache(capacity)
    raise ValueError(f"unknown cache policy: {cfg.cache_policy}")
