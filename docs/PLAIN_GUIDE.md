# Memogent in plain language

The other docs here are written for integrators and judges. This one is
written for you at 2am. It explains what every piece does and why it
exists, with no jargon left undefined.

## What problem does this solve?

Phones have limited fast memory (RAM). AI apps want huge things in RAM:
models, and "KV caches" (the scratchpad an LLM builds up while reading
your conversation; recomputing it from scratch is slow and wastes
battery). The operating system evicts memory blindly, so apps feel slow
and AI sessions "forget".

Memogent is a librarian for that memory. It watches how you use your
phone, predicts what you'll open next, and decides ahead of time what to
keep hot in RAM, what to preload, and what to evict or park on disk.

## The core loop (one sentence per step)

1. The host app reports events: "user opened WhatsApp", "model X loaded",
   "battery at 20%".
2. The **predictor** updates its guess of what comes next (it learns your
   app-to-app habits, time of day, and similar signals).
3. The **arbiter** weighs that guess against the device's condition
   (RAM pressure, battery, temperature) and produces decisions.
4. The **preloader/cache** carries the decisions out: warm this up, evict
   that, move this to storage.
5. **Telemetry** records what happened so the KPI harness can measure
   whether we actually made things faster.

## C++ core, file by file (core/include/memogent/)

| Header | What it is, simply |
|---|---|
| `orchestrator.hpp` | The front door. Apps create ONE `Orchestrator`, feed it events (`record_app_open` etc.), and it drives everything else. Thread-safe. |
| `app_event.hpp` | The event vocabulary: app opened/closed, model loaded, etc. |
| `predictor.hpp` | The "what's next?" brain. Ships with a bigram/frequency model (which app usually follows which). It's a pure-virtual port, so an LSTM or anything else can be swapped in later. |
| `arbiter.hpp` | The referee. Takes predictions + device state and decides keep/preload/evict. Rule-based today; an RL version is a roadmap item. |
| `adaptive_cache.hpp` | The RAM shelf. A cache that changes its eviction behavior based on pressure instead of plain LRU. |
| `kv_cache.hpp` | Represents LLM scratchpads (KV pages) as first-class cacheable items, so a chat session can survive an app switch. |
| `preloader.hpp` | Runs "warm this up before the user asks" work. |
| `model_swap.hpp` | Moves models between hot RAM and cold storage. |
| `device_state.hpp` | Snapshot of battery / thermal / memory pressure. The graceful-degradation signals. |
| `storage.hpp` | Where cold things go. A port; in-memory today, SQLite planned. |
| `telemetry.hpp` | Counters and timings for the KPI harness. |
| `config.hpp` | All the knobs in one struct. |
| `result.hpp` | Error handling without exceptions (`Result<T>`; `create()` never throws). |
| `types.hpp`, `clock.hpp`, `logger.hpp`, `version.hpp` | IDs, testable time source, logging, version constants. |

`core/src/` holds the implementations; `cpp_tests/` are Catch2 tests for
cache, predictor, and orchestrator (8 suites).

## The other languages

- **`bindings/c/`**: a stable C ABI (plain C functions) wrapping the C++
  core. This is the universal adapter: Swift, Kotlin/JNI, Flutter, Rust
  all talk to Memogent through this one surface.
- **iOS / Android folders**: thin platform wrappers around the C ABI
  (Swift Package; AAR + JNI).
- **`python/memogent/`**: a full re-implementation in Python, kept small,
  used for research and for the KPI benchmark harness (`memogent.bench`).
  Same module names as the C++ (orchestrator, predictor, arbiter, cache),
  so learning one teaches you the other. `python -m pytest` runs 12 tests.

## The KPI harness

`memogent.bench` replays app-usage traces and measures the six hackathon
KPIs (hit rate, cold starts avoided, latency, memory ceiling, battery
proxy, degradation behavior) and writes a JSON report. `demo_report.md`
in the repo root is a generated example.

## Building it

```sh
cmake -B build -DMEM_BUILD_TESTS=ON && cmake --build build -j
cd build && ctest              # C++ tests
cd python && make test         # Python tests (or: python -m pytest)
```

Dependencies (fmt, spdlog, Catch2) are fetched automatically by CPM at
configure time. Note: fmt is pinned at 11.x and spdlog at 1.15.x because
older pins failed to compile on current Apple Clang.

## What is actually left to do (honest status)

Shipped and passing today: the C++ core + tests, C ABI, Python runtime +
tests, KPI harness with all 6 KPIs passing, platform packaging skeletons,
docs. Remaining roadmap items are real feature work, in priority order:

1. SQLite-backed Storage (biggest practical win; the port already exists).
2. Trained LSTM predictor to replace the bigram model (needs the LSApp
   dataset pipeline in `scripts/fetch_datasets.sh`).
3. Direct llama.cpp KV-cache hook.
4. RL arbiter, ONNX runtime path, Flutter/RN wrappers, store publishing.
