# KPI Harness

Every Samsung AX Hackathon 2026 Problem #3 KPI is computed automatically by
`python -m memogent.cli.main bench` (or the native `mem bench` binary once
we cut the Phase 2 release). The harness produces a JSON report you can
stash as a CI artifact.

## Metrics & formulas

| KPI | Formula | Baseline | Current |
|---|---|---|---|
| App Load Time Improvement | `(B − C) / B * 100` where B=baseline cold-load ms, C=current | LRU hit-rate × 40 ms + (1−r) × 200 ms | `ContextARC` or `ARC` hit-rate × 40 ms + (1−r) × 200 ms |
| Launch Time Improvement | `0.6 × AppLoadImprovement` | same baseline | same current |
| Thrashing Reduction | `(B − C) / B * 100` where B=LRU evictions, C=best-policy evictions | LRU | best (usually ContextARC) |
| System Stability | `#{OOM + crashes}` across scenario suite | 0 baseline | 0 target |
| Prediction Accuracy @ top-3 | HR@3 on held-out trace | N/A | max across {MFU, MRU, Markov-1, FreqRecency} |
| Cache Hit Rate | hits / (hits + misses) | LRU | best policy |
| Memory Utilization Efficiency | `(r_current − r_baseline) × 100` where r = hit_rate | LRU | best policy |

## Running locally

```bash
# quick synthetic smoke:
cd python && make install && make bench

# full run against LSApp (requires prior download):
cd python && ./.venv/bin/python -m memogent.cli.main bench \
    --trace ../bench/traces/lsapp.tsv \
    --user 4 \
    --n 50000 \
    --output bench_results/lsapp_user4.json
```

## JSON schema

```json
{
  "events": 3000,
  "apps_seen": 12,
  "predictor_runs": {
    "markov1": {"hr1": 0.41, "hr3": 0.58, "n": 730},
    ...
  },
  "cache_runs": {
    "lru": {"policy": "lru", "hit_rate": 0.98, ...},
    ...
  },
  "kpis": {
    "app_load_time_improvement_pct": 4.2,
    "thrashing_reduction_pct": 17.5,
    "cache_hit_rate": 0.99,
    "prediction_accuracy_top3": 0.58,
    ...
  },
  "pass_fail": {
    "app_load_time": true,
    "thrashing": false,
    ...
  },
  "elapsed_s": 0.72
}
```

Exit codes: `0` = all targets met, `1` = at least one failed. Wire it into
CI to catch regressions.

## Interpreting the pass/fail

The harness is aggressive: if *any* target is missed the process exits 1.
This is a feature, not a bug — it forces us to explain missing KPIs
transparently rather than cherry-picking.
