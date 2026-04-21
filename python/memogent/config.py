"""Runtime configuration — parity with core/include/memogent/config.hpp."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass
class Config:
    # storage
    data_dir: Path = Path("./data")
    # predictor
    predictor: str = "freq_recency"
    predictor_model_path: str = ""
    predictor_history: int = 32
    prediction_top_k: int = 3
    # cache
    cache_policy: str = "context_arc"
    cache_capacity_app: int = 32
    cache_capacity_model: int = 4
    cache_capacity_kv_pages: int = 1024
    cache_capacity_embeddings: int = 1024
    # arbiter
    arbiter: str = "heuristic"
    ram_budget_mb: int = 768
    # preloader
    preload_enabled: bool = True
    preload_top_k: int = 2
    # LLM
    llm_backend: str = "mock"
    llm_model_path: str = ""
    kv_cache_token_budget: int = 4096
    kv_cache_sink_tokens: int = 4
    kv_cache_recent_tokens: int = 512
    # QoS
    qos_battery_reduced: float = 30.0
    qos_battery_minimal: float = 15.0
    qos_battery_paused: float = 5.0
    qos_thermal_reduced: float = 0.55
    qos_thermal_minimal: float = 0.75
    qos_thermal_paused: float = 0.90
    qos_ram_minimal_mb: float = 128.0
    qos_ram_paused_mb: float = 48.0
    # misc
    telemetry_enabled: bool = True
    telemetry_buffer: int = 4096
    session_id: str = "default"
    random_seed: int = 0x5EED
    verbose: bool = False

    def ensure_dirs(self) -> None:
        Path(self.data_dir).mkdir(parents=True, exist_ok=True)
