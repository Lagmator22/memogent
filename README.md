<div align="center">

# Memogent

**Context-aware, adaptive memory orchestrator for on-device agentic systems.**

_Primary **C++23** core · **iOS** (Swift Package) · **Android** (AAR + JNI) · **Python** reference runtime · Built for production, engineered for flexibility._

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue)]()
[![Python](https://img.shields.io/badge/Python-3.10%2B-blue)]()
[![iOS](https://img.shields.io/badge/iOS-15%2B-black)]()
[![Android](https://img.shields.io/badge/Android-API%2028%2B-green)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey)]()

</div>

---

Memogent is a **cross-platform framework that makes mobile AI feel instant.** It sits between your app and the operating system and decides, second by second:

- Which app / model / KV-cache page should live in hot RAM right now.
- Which one to preload next because the user is about to need it.
- Which one to evict, swap to cold storage, or recompute.

The framework is **context-aware** — it learns per-user app sequences, time-of-day rhythms, location cues, and LLM salience signals — and **adaptive** — it degrades gracefully under battery / thermal / RAM pressure instead of crashing.

It is designed to ship in real products. One C++23 core is consumed by iOS via a Swift Package, by Android via an AAR, by Flutter / React Native / Rust via a stable C ABI, and by Python for research and benchmarking. Every module hides behind a pure-virtual port so you can swap models, caches, and storage backends without touching the app.

---

## Table of contents

- [Why this exists](#why-this-exists)
- [Samsung AX Hackathon 2026 · Problem #3 alignment](#samsung-ax-hackathon-2026--problem-3-alignment)
- [Architecture at a glance](#architecture-at-a-glance)
- [Repository layout](#repository-layout)
- [Quick start — Python reference](#quick-start--python-reference)
- [Quick start — C++ core](#quick-start--c-core)
- [Integrating on iOS](#integrating-on-ios)
- [Integrating on Android](#integrating-on-android)
- [Public C ABI](#public-c-abi)
- [Swapping predictors / caches / models](#swapping-predictors--caches--models)
- [Benchmarks](#benchmarks)
- [Security & privacy model](#security--privacy-model)
- [Production checklist](#production-checklist)
- [Roadmap](#roadmap)
- [References](#references)
- [License](#license)

---

## Why this exists

Modern phones run several AI workloads concurrently — LLM for the assistant, vision for the camera, speech for dictation, plus classic apps fighting for RAM. Today, these systems:

- Allocate RAM with coarse OS heuristics (LRU, LowMemoryKiller) that know nothing about user intent.
- Never preload the next likely app or model, so every switch costs a full cold start.
- Treat LLM KV-cache, model weights, and app state as three unrelated pools, which causes thrashing when any single pool spikes.
- Have no shared context between the agent (“what does the user care about?”) and the runtime (“what should be hot?”).

Memogent fixes all four. It ships with a next-context predictor, a plug-in adaptive cache with four policies (LRU, LFU, ARC, ContextARC), an LLM-aware KV-cache manager, a multi-model swap manager, a predictive preloader, and a heuristic + RL-ready memory arbiter — all stitched together by a telemetry layer that automatically reports the exact KPIs a hackathon judge or product manager cares about.

## Samsung AX Hackathon 2026 · Problem #3 alignment

This project directly targets Problem Statement #3: **Context-Aware, Adaptive Memory Solution for Mobile Agentic Systems**. Every Samsung-listed KPI is baked into the benchmark harness as an automated assertion, not a slide claim:

| KPI (Samsung brief) | Target | Measured by | Module in this repo |
|---|---:|---|---|
| Application Load Time Improvement | ≥20% | Mean cold-start delta on replayed Android trace vs baseline | `bench/harness.py` + `Preloader` |
| App Launch Time Improvement | ≥10% | Median first-frame-to-interactive delta | `bench/harness.py` + `Preloader` |
| Memory Thrashing Reduction | ≥50% | Page-fault + LMK-kill events/hr | `bench/harness.py` + `Arbiter` |
| System Stability | 0 crashes | OOM / crash count across 10 stress scenarios | `bench/scenarios/*` |
| Next-Context Prediction Accuracy | ≥75% top-3 | HR@1, HR@3 on held-out LSApp split | `Predictor` (Markov/FreqRecency/LSTM) |
| Cache Hit Rate | ≥85% | Hits / total lookups in `AdaptiveCache` | `AdaptiveCache` (ContextARC) |
| Memory Utilization Efficiency | ≥+30% | Useful-bytes-resident / total-bytes-resident | `Arbiter` + `Telemetry` |

Run the harness against the canonical [LSApp](https://github.com/aliannejadi/LSApp) dataset to reproduce the numbers:

```bash
bash scripts/fetch_datasets.sh          # downloads LSApp (599k events, 87 apps, 292 users)
cd python && make install && make bench # trains predictors, replays trace, emits JSON report
```

## Architecture at a glance

```
              +-------------------------------------------------+
              |                 YOUR APP (iOS / Android)        |
              +----------+----------------+---------------------+
                         | Swift          | Kotlin (JNI)
                         v                v
              +-----------------+  +----------------------+
              |  Swift Package  |  |  AAR (Kotlin + C++)  |
              +--------+--------+  +----------+-----------+
                       |  C ABI (memogent_c.h)  |
                       +-----------+-----------+
                                   |
                    +--------------v---------------+
                    |     memogent-core (C++23)    |
                    |                              |
                    |  Orchestrator facade         |
                    |   ├ AppEventSink             |
                    |   ├ Predictor (Markov, FR,   |
                    |   │   LSTM, Transformer)     |
                    |   ├ AdaptiveCache            |
                    |   │   (LRU/LFU/ARC/CtxARC)   |
                    |   ├ KVCacheManager           |
                    |   ├ ModelSwapManager         |
                    |   ├ Preloader                |
                    |   ├ Arbiter (Heuristic / RL) |
                    |   └ Telemetry (SQLite)       |
                    +---------------+--------------+
                                    |
                 +------------------+-------------------+
                 v                  v                   v
           +-----------+      +-----------+       +-----------+
           | Storage   |      | llama.cpp |       | ONNX RT / |
           | SQLite    |      | / Ollama  |       | LiteRT    |
           +-----------+      +-----------+       +-----------+
```

Every box is an interface with at least one shipping implementation and one or more planned alternatives. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full module contract.

## Repository layout

```
memogent/
├── README.md                            ← this file
├── LICENSE                              ← MIT
├── CMakeLists.txt                       ← top-level C++ build
├── cmake/                               ← CPM + dependency + platform detection
│
├── core/                                ← PRIMARY C++23 LIBRARY (memogent-core)
│   ├── include/memogent/                ← public headers = the API surface
│   └── src/                             ← implementations
│
├── bindings/
│   ├── c/memogent_c.h                   ← stable C ABI (versioned)
│   ├── ios/                             ← Swift Package (SPM) wrapping the C ABI
│   ├── android/                         ← Gradle module producing an AAR
│   └── python/                          ← pybind11 wheel (parity with python/)
│
├── python/                              ← SECONDARY REFERENCE RUNTIME
│   ├── pyproject.toml
│   ├── Makefile                         ← make chat | bench | orchestrate | train
│   └── memogent/                        ← pure-Python mirror for research + tests
│
├── bench/                               ← canonical benchmark harness (JSON KPI report)
├── cpp_tests/                           ← Catch2 unit + integration tests
├── examples/
│   ├── cpp_cli/                         ← native CLI binary (mem)
│   ├── ios_demo/                        ← SwiftUI demo app
│   └── android_demo/                    ← Jetpack Compose demo app
├── docs/                                ← architecture, KPI harness, integration, security, roadmap
├── scripts/                             ← install, dataset fetch, helpers
└── .github/workflows/                   ← CI: Linux, macOS, iOS, Android, Python
```

## Quick start — Python reference

```bash
cd python
make install            # creates .venv and installs memogent editable
make orchestrate        # replays a synthetic trace; prints live KPI table
make bench              # full benchmark vs {MFU, MRU, LRU, ARC, random} baselines
make train              # train the LSTM predictor on LSApp
make test               # pytest suite
```

No model download is required. The Python runtime ships with deterministic baselines so the KPI harness runs green on any laptop.

## Quick start — C++ core

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./examples/cpp_cli/mem orchestrate --trace ../python/memogent/datasets/samples/synth.jsonl
./cpp_tests/memogent_tests
```

The build auto-downloads SQLite, `nlohmann/json`, `fmt`, `spdlog`, and Catch2 through CPM. No system package install is needed.

## Integrating on iOS

```swift
// Package.swift
.package(url: "https://github.com/Lagmator22/memogent.git", branch: "main")

// YourApp.swift
import Memogent

let engine = try Memogent.Orchestrator(dataDir: URL.documentsDirectory)
engine.recordAppOpen(appId: "com.foo.bar")
let next = engine.predictNextApps(k: 3)
print(next)          // ["com.mail.Inbox", "com.msg.Chat", "com.music.Player"]
engine.attachThermalHook()   // maps ProcessInfo.thermalState → PowerMode
```

See [`docs/INTEGRATION_IOS.md`](docs/INTEGRATION_IOS.md) for a screen-by-screen walkthrough, permissions, and the recommended `UIApplication` lifecycle wiring.

## Integrating on Android

```kotlin
// build.gradle.kts
implementation("dev.memogent:memogent:0.1.0")

// MainActivity.kt
val engine = Memogent.Orchestrator(this)
engine.recordAppOpen(packageName = packageName)
val next = engine.predictNextApps(k = 3)
engine.bindUsageStatsSource()     // wires UsageStatsManager + BatteryManager + PowerManager
```

See [`docs/INTEGRATION_ANDROID.md`](docs/INTEGRATION_ANDROID.md) for the `UsageStatsManager` permission flow and foreground-service recipe.

## Public C ABI

Everything above is one layer. The stability guarantee is the C ABI in [`bindings/c/memogent_c.h`](bindings/c/memogent_c.h). Every other language binding goes through it, so `Swift` / `Kotlin` / `Python` / `Rust` / `Dart FFI` all see the same API.

```c
mem_orchestrator_t* orch = mem_orchestrator_create("/path/to/data");
mem_orchestrator_record_app_event(orch, &event);
mem_prediction_t preds[3];
size_t n = mem_orchestrator_predict_next(orch, preds, 3);
mem_orchestrator_destroy(orch);
```

Symbols are SemVer-tagged: `mem_*` is the stable surface (additions only within a major version); `mem_internal_*` is subject to change at any time.

## Swapping predictors / caches / models

Every heavy dependency is injected through a pure-virtual port.

```cpp
#include <memogent/memogent.hpp>

mem::Config cfg;
cfg.predictor     = "lstm";                          // markov | freq_recency | lstm | transformer
cfg.predictor_model_path = "assets/app_lstm.tflite";
cfg.cache_policy  = "context_arc";                   // lru | lfu | arc | context_arc
cfg.arbiter       = "heuristic";                     // heuristic | td3
cfg.llm_backend   = "llama.cpp";                     // llama.cpp | ollama | mock
cfg.llm_model_path = "assets/gemma-3-4b-it-Q4_K_M.gguf";

auto orch = mem::Orchestrator::create(cfg).value();
```

## Benchmarks

The harness at `bench/` replays Android usage traces and emits a JSON report with every Samsung KPI. It ships with adapters for:

- **LSApp** — 599k events / 87 apps / 292 users ([aliannejadi/LSApp](https://github.com/aliannejadi/LSApp))
- **Tsinghua App Usage** — 1k users / 2k apps, with base-station + traffic metadata
- **Synthetic** — deterministic trace generator we use in CI so pipelines stay fast

Baseline comparisons:

- `MFU` (most frequent), `MRU` (most recent), `Markov1`, `Markov2`
- `LRU`, `LFU`, `ARC`, `ContextARC` (ours)
- `MnemoLite` (our knowledge-memory companion project, knowledge-only baseline)

See [`docs/KPI_HARNESS.md`](docs/KPI_HARNESS.md) for exact metric definitions and the current numbers on each dataset.

## Security & privacy model

- **On-device by default.** No network call happens from the core. The only way data leaves the device is if you explicitly pass in a cloud LLM adapter.
- **Sandbox-compatible.** SQLite lives inside the app container (iOS) / internal storage (Android). No shared filesystem writes.
- **At-rest encryption.** Optional `SQLCipher` backend (build flag `MEM_WITH_SQLCIPHER=ON`).
- **Auditable.** Every arbiter decision, admission, and eviction is logged with its inputs, so you can prove why a memory page was evicted.
- **Kill-switch.** `orchestrator.reset()` wipes every store + cache + WAL in a single transaction — wire it to your "forget me" UI.

See [`docs/SECURITY.md`](docs/SECURITY.md) for the threat model.

## Production checklist

- [x] C++23 core with thread-safe facades (single writer, many readers).
- [x] Stable SemVer-tagged C ABI.
- [x] Swift Package + Android AAR build recipes.
- [x] Exception-free public surface (`Result<T, Error>` everywhere).
- [x] Bounded memory for every cache / index — no unbounded growth.
- [x] Observability: structured logs (spdlog), metrics (SQLite events), tracing-friendly spans.
- [x] Automated KPI harness with JSON report.
- [x] Deterministic baselines for CI.
- [x] Cross-platform build matrix.
- [x] Security review checklist in `docs/SECURITY.md`.
- [ ] Real device benchmarks on Pixel 8 + iPhone 15 Pro (Phase 2 of hackathon).
- [ ] Fuzzing harness for the JNI / C ABI surface (Phase 3).
- [ ] Crash-report export plugin (Phase 4).

## Roadmap

- ✅ Core + predictor + adaptive cache + orchestrator + KPI harness.
- ✅ Python reference parity.
- ✅ iOS Swift Package and Android AAR build recipes.
- 🔜 LSTM / Transformer predictor checkpoints (tiny) released as assets.
- 🔜 Direct `llama.cpp` integration (mobile-native LLM backend).
- 🔜 ONNX Runtime / LiteRT embedder.
- 🔜 TD3 RL arbiter (offline-trained on LSApp).
- 🔜 Real-device power numbers on Pixel 8 + iPhone 15 Pro.
- 🔜 Flutter + React Native + Rust bindings (the C ABI supports them today).

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the full plan.

## References

Primary research sources (full list in [`docs/RESEARCH_SURVEY.md`](docs/RESEARCH_SURVEY.md)):

- Aliannejadi et al. **LSApp — Context-Aware Target Apps Selection.** ACM TOIS, 2020.
- Li et al. **TGT — Temporal Gating Transformer for Smartphone App Usage Prediction.** arXiv:2502.16957.
- Li et al. **Atten-Transformer.** arXiv:2502.16957.
- Khaokaew et al. **MAPLE — LLM-Embeddings for Mobile App Prediction.** arXiv:2309.08648.
- Kwon et al. **PagedAttention (vLLM).** SOSP 2023.
- Wang et al. **SAGE-KV — Self-Attention-Guided KV Eviction.** arXiv:2503.08879.
- Rehg. **KV-Compress — Paged KV-Cache Compression with Variable Rates.** arXiv:2410.00161.
- Xiao et al. **StreamingLLM.** arXiv:2309.17453.
- Zhang et al. **H2O — Heavy-Hitter Oracle for Efficient Generative Inference.** NeurIPS 2023.
- Liu et al. **MobiMem — Memory-Centric Agent System.** arXiv:2512.15784.
- Zhao et al. **AME — Heterogeneous Agentic Memory Engine for Smartphones.** arXiv:2511.19192.
- Megiddo & Modha. **ARC — A Self-Tuning, Low Overhead Replacement Cache.** USENIX FAST 2003.

Internal inspirations:

- **MnemoLite** (sister project, `github.com/Lagmator22/mnemolite`) — knowledge-memory companion.
- **OvaSearch** — native C++ RAG spine, embedding-cache patterns.
- **CPPCode** — interface-first C++23 agent harness.

## License

MIT — see [`LICENSE`](LICENSE).

---

<sub>Built for Samsung ennovateX AX Hackathon 2026, Problem Statement #3 — and engineered to outlive the hackathon in real production apps.</sub>
