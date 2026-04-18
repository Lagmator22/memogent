"""Abstract predictor interface."""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import List

from ..types import AppEvent, Prediction


class Predictor(ABC):
    name: str = "predictor"

    @abstractmethod
    def observe(self, ev: AppEvent) -> None: ...

    @abstractmethod
    def predict(self, top_k: int) -> List[Prediction]: ...

    def reset(self) -> None:
        """Forget every observation. Override when state is non-trivial."""
