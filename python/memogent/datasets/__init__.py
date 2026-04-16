"""Trace datasets used by the benchmark harness."""
from .lsapp import load_lsapp_tsv
from .synth import generate_synthetic

__all__ = ["load_lsapp_tsv", "generate_synthetic"]
