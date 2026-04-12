# Roadmap

## v0.1 — shipped today
- ✅ C++23 core with `Orchestrator`, `Predictor` (MFU/MRU/Markov-1/FreqRecency), `AdaptiveCache` (LRU/LFU/ARC/ContextARC), `KVCacheManager`, `ModelSwapManager`, `Preloader`, `Arbiter`, `Telemetry`, `Storage`.
- ✅ Stable C ABI, Swift Package, Android AAR skeleton, Python reference runtime.
- ✅ KPI harness (`memogent.bench`) with JSON report.
- ✅ Catch2 + pytest suites.
- ✅ Architecture + KPI + Integration + Security docs.

## v0.2 — targeted for Phase 2 (Jun 22)
- LSTM predictor trained on LSApp, tflite + ONNX checkpoints.
- SQLite-backed `Storage` shipping; SQLCipher optional.
- `mem_orchestrator_set_db_key` C ABI addition for at-rest encryption.
- Prebuilt XCFramework + Maven Central publish recipes in CI.
- LSApp-based benchmark numbers published in `docs/BENCHMARKS.md`.
- Android emulator KPI run in CI.

## v0.3 — targeted for Phase 3 (Jul 3)
- TD3 RL arbiter (offline-trained).
- Direct `llama.cpp` KV hook under `MEM_WITH_LLAMACPP`.
- ONNX Runtime embedder + predictor for on-device inference.
- Flutter + React Native wrappers around the C ABI.

## v1.0 — Phase 4 finale (Jul 30)
- Real-device Pixel 8 + iPhone 15 Pro benchmarks on stage.
- Maven Central + CocoaPods publishes.
- Rust crate released on crates.io via the C ABI.
- Post-hackathon: open-call for contributors.

## Later (post-hackathon)
- Federated learning for the predictor without exporting user traces.
- Crash-report export plugin.
- Fuzzing harness (libFuzzer / jazzer) for the JNI + C ABI surface.
- Extended `UsageStatsSource` that consumes Samsung Health / SmartThings signals.
