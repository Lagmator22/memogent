"""Memogent — Python reference runtime.

This package mirrors the C++23 core under `../core/` one-to-one. It is used
for prototyping, training, and the benchmark harness that asserts every
Samsung AX Hackathon 2026 Problem #3 KPI target.

Public API:
    Orchestrator — single entrypoint
    Config       — knob panel
    make_predictor / make_cache / make_arbiter — factory functions
"""
from .config import Config
from .orchestrator import Orchestrator
from .types import AppEvent, EventType, PowerMode, Prediction, ResourcePool

__version__ = "0.1.0"
__all__ = [
    "Config",
    "Orchestrator",
    "AppEvent",
    "EventType",
    "PowerMode",
    "Prediction",
    "ResourcePool",
]
