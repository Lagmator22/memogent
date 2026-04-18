"""Predictor factory."""
from __future__ import annotations

from ..config import Config
from .base import Predictor
from .freq_recency import FreqRecencyPredictor
from .markov import MarkovPredictor
from .most_frequent import MFUPredictor
from .most_recent import MRUPredictor


def make_predictor(cfg: Config) -> Predictor:
    name = cfg.predictor.lower()
    if name == "mfu":
        return MFUPredictor()
    if name == "mru":
        return MRUPredictor()
    if name in ("markov", "markov1"):
        return MarkovPredictor()
    if name in ("freq_recency", "freqrecency"):
        return FreqRecencyPredictor()
    raise ValueError(f"unknown predictor: {cfg.predictor}")
