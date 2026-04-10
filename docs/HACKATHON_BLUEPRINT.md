# Samsung AX Hackathon 2026 · Problem #3 — Blueprint

This document maps every deliverable Samsung asked for to a specific module
in this repository.

## Problem statement recap

> *Context-aware, adaptive memory solution for mobile agentic systems* —
> dynamically allocate RAM, predictively pre-load components, adjust
> retention/eviction based on usage and memory availability.

## Our deliverables vs the brief

| Samsung ask | Where it lives |
|---|---|
| Context-Aware Memory Allocation | `Arbiter` (`core/src/arbiter/*`, `python/memogent/arbiter/*`) + `DeviceState` classifier |
| Predictive Pre-Loading | `Predictor` (`core/src/predictors/predictors.cpp`) + `Preloader` (`core/src/preloader/preloader.cpp`) |
| Adaptive Caching Policies | `AdaptiveCache` (`core/src/caches/caches.cpp`) — LRU / LFU / ARC / ContextARC |
| KV-cache management (LLMs) | `KVCacheManager` (`core/src/kv_cache/kv_cache.cpp`) |
| Multi-model weights swap | `ModelSwapManager` (`core/src/model_swap/model_swap.cpp`) |
| Telemetry + KPI reporting | `Telemetry` (`core/src/telemetry/telemetry.cpp`) + harness (`python/memogent/bench/harness.py`) |

## KPI table (targets from brief)

See [`KPI_HARNESS.md`](KPI_HARNESS.md) for formulas + JSON schema.

| KPI | Target | Measured where |
|---|---:|---|
| Application Load Time Improvement | ≥20% | `bench/harness.py` + `Preloader` + `ContextARC` |
| App Launch Time Improvement | ≥10% | same |
| Memory Thrashing Reduction | ≥50% | `bench/harness.py` eviction delta |
| System Stability | 0 | `Telemetry.record_stability_issue` |
| Next-Context Prediction Accuracy | ≥75% top-3 | `Predictor` implementations (Markov, FreqRecency, LSTM) |
| Cache Hit Rate | ≥85% | `AdaptiveCache` (ContextARC) |
| Memory Utilization Efficiency | +30% | harness util_pct proxy |

## Datasets (as suggested)

| Dataset | Used for | Loader |
|---|---|---|
| LSApp | predictor training + replay | `memogent.datasets.load_lsapp_tsv` |
| Tsinghua App Usage Dataset | predictor training | planned |
| KV Cache Workloads | KV manager stress test | synthesized by `bench/harness.py` |
| Context Query Logs (Melbourne parking) | context prediction ablation | planned |
| Synthetic | CI smoke | `memogent.datasets.generate_synthetic` |

## Model palette

| Role | Primary | Optional |
|---|---|---|
| Next-app predictor | Markov-1 + FreqRecency (online-trainable, interpretable) | LSTM (offline, ~150 kB), Atten-Transformer (Phase 3) |
| On-device LLM | llama.cpp with Gemma-3-4B-it Q4_K_M | Ollama for desktop dev |
| Embedder | Static (not strictly needed for KPI) | ONNX MiniLM INT8 (Phase 2) |

## Phase alignment

| Hackathon phase | Deliverable here |
|---|---|
| Phase 1 Blueprint (due May 13) | This file + README + C++ headers |
| Phase 2 Full Solution (due Jun 22) | Working `Orchestrator` + iOS/Android bindings + KPI harness results on LSApp |
| Phase 3 Online Presentation (Jul 3) | Live demo on laptop + Android emulator |
| Phase 4 Grand Finale (Jul 30) | Pixel 8 + iPhone 15 Pro side-by-side on stage |

## Going above the brief

1. **Unified system-memory and agent-memory story**: Memogent is designed to sit underneath MnemoLite (our companion agentic-memory project) as the allocation brain. The agent's semantic memory drives the arbiter.
2. **Cross-platform from day one**: C++23 core, Swift Package, AAR, Python reference — SemVer-tagged C ABI for Flutter / React Native / Rust.
3. **Audit-friendly**: every arbiter decision carries a `rationale` string; every KPI is reproducible from the JSON report.
4. **Privacy-by-default**: zero network calls in the core; SQLCipher optional.
5. **Open benchmark**: the full harness + predictor checkpoints will be released under MIT so other teams can compare apples-to-apples.
