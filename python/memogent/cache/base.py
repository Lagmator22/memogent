"""Abstract cache interface — parity with core/include/memogent/adaptive_cache.hpp."""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import List, Optional

from ..types import CacheStats, Prediction


class AdaptiveCache(ABC):
    policy_name: str = "cache"

    @abstractmethod
    def get(self, key: str) -> Optional[bytes]: ...

    @abstractmethod
    def put(self, key: str, value: bytes) -> None: ...

    @abstractmethod
    def erase(self, key: str) -> None: ...

    @abstractmethod
    def resize(self, capacity: int) -> None: ...

    @abstractmethod
    def clear(self) -> None: ...

    @abstractmethod
    def stats(self) -> CacheStats: ...

    def hint_future(self, predictions: List[Prediction]) -> None:
        """No-op by default. ContextARC overrides."""
