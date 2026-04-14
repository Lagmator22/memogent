"""Cache policies with a single interface."""
from .arc import ARCCache, ContextARCCache
from .base import AdaptiveCache
from .factory import make_cache
from .lfu import LFUCache
from .lru import LRUCache

__all__ = [
    "AdaptiveCache",
    "LRUCache",
    "LFUCache",
    "ARCCache",
    "ContextARCCache",
    "make_cache",
]
