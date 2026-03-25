# Changelog

All notable changes to this project are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- LSTM predictor checkpoint trained on LSApp.
- Direct `llama.cpp` KV-cache hook under `MEM_WITH_LLAMACPP`.
- Real-device Pixel 8 + iPhone 15 Pro benchmark numbers.

## [0.1.0] — 2026-04-20

### Added
- C++23 core library (`memogent::core`) with `Orchestrator` facade.
- `Predictor` port + four shipping implementations: MFU, MRU, Markov-1, FreqRecency.
- `AdaptiveCache` port + four policies: LRU, LFU, ARC, ContextARC.
- `KVCacheManager` with StreamingLLM-style sink+recent+salience eviction.
- `ModelSwapManager` with LRU + priority.
- `Preloader` driven by predictor output.
- `Arbiter` heuristic implementation.
- `Telemetry` with KPI snapshot + JSON export.
- `Storage` port with in-memory implementation.
- Stable C ABI (`memogent_c.h`) with opaque handles and SemVer guarantees.
- iOS Swift Package with `Memogent.Orchestrator` wrapper and thermal hook.
- Android AAR module with JNI bridge, `Orchestrator.kt`, and `UsageStatsSource`.
- Python reference runtime with parity modules, Rich CLI, and pytest suite.
- KPI harness (`mem bench`) reporting every Samsung AX Hackathon 2026
  Problem #3 KPI with pass/fail.
- Catch2 unit tests for predictor, cache, and orchestrator.
- CMake build system with CPM + auto-fetched deps (SQLite, fmt, spdlog, nlohmann_json, Catch2).
- Docs: Architecture, KPI harness, Hackathon blueprint, Predictor design,
  Security, Integration (iOS + Android), Roadmap, Research survey.
- GitHub Actions CI: C++ (Linux + macOS) and Python (3.10–3.12).

[Unreleased]: https://github.com/Lagmator22/memogent/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Lagmator22/memogent/releases/tag/v0.1.0
