# Architecture

Memogent is organised around one facade (`Orchestrator`) that talks to a
handful of pure-virtual ports. Every port has at least one shipping
implementation and one or more planned alternatives.

## Module map

| Module | Role | Current impls | Planned |
|---|---|---|---|
| `Predictor` | Predict next-app / task | MFU, MRU, Markov-1, FreqRecency | LSTM, Atten-Transformer (MAPLE-lite) |
| `AdaptiveCache` | Typed key-value cache with policies | LRU, LFU, ARC, ContextARC (ours) | SIEVE, 2Q, LIRS |
| `KVCacheManager` | LLM KV-cache lifecycle | Sink+recent+salience | PagedAttention / SAGE-KV adapters |
| `ModelSwapManager` | Multi-model weights swap | LRU + priority | NPU-aware, quant-aware |
| `Preloader` | Warm-up top-k predictions | Default heuristic | Energy-aware, data-prefetch |
| `Arbiter` | Allocation policy brain | Heuristic (ships), soft_budget (alias) | TD3 RL, contextual bandit |
| `Telemetry` | KPI aggregation + JSON export | In-memory + optional SQLite | OpenTelemetry exporter |
| `Storage` | Persistent state | In-memory (ships), SQLite (linked) | SQLCipher, Room, GRDB |

## Data flow (tick)

```
record_event()  ─► Predictor.observe()
                    │
record_app_open()   │
                    ▼
tick():
   preds   = Predictor.predict(k)
   inputs  = {DeviceState, mode, preds, caches}
   plan    = Arbiter.decide(inputs)
   AppCache.resize(plan.app_cache_capacity)
   ModelCache.resize(plan.model_cache_capacity)
   if plan.allow_preload:
       AppCache.hint_future(preds)
       Preloader.execute(preds)
   KV.plan_and_evict(plan.kv_cache_token_budget)
   Telemetry.record_latency / record_cache
   return plan
```

## Ports = pure-virtual interfaces

Every heavy dependency is injected. You can swap:

```cpp
cfg.predictor = "lstm";          // ONNX or tflite model path
cfg.cache_policy = "context_arc"; // default; try "arc" or "lru" for ablation
cfg.arbiter = "heuristic";        // "td3" when the RL build arrives
cfg.llm_backend = "llama.cpp";    // direct llama.cpp hook on MEM_WITH_LLAMACPP
```

## Thread model

- `Orchestrator` serializes writes behind a single mutex.
- `AdaptiveCache` subclasses take their own mutex.
- Host apps should call `tick()` from a background thread / coroutine; keeping
  it off the UI thread is a 1-line job on iOS (`Task.detached`) and Android
  (`Dispatchers.Default`).

## Failure modes

- Invalid config ⇒ `Error{InvalidArgument, "…"}` returned from `create()`.
- Predictor or cache failures ⇒ caught by the Orchestrator, logged, and the
  KPI harness records a `stability_issue` so the report shows non-zero.
- Storage IO failures ⇒ Result<T,Error> with `IoError`.

## See also

- [`KPI_HARNESS.md`](KPI_HARNESS.md) — how each KPI is derived.
- [`PREDICTOR_DESIGN.md`](PREDICTOR_DESIGN.md) — predictor zoo details.
- [`SECURITY.md`](SECURITY.md) — threat model.
- [`INTEGRATION_IOS.md`](INTEGRATION_IOS.md) / [`INTEGRATION_ANDROID.md`](INTEGRATION_ANDROID.md).
