# Benchmark harness

This directory hosts the canonical traces + a minimal C++ replay harness
(Phase 2 deliverable). The Python-side harness at `python/memogent/bench/`
is the authoritative implementation today.

## Datasets

| Dataset | Source | Expected path |
|---|---|---|
| LSApp (default) | https://github.com/aliannejadi/LSApp | `bench/traces/lsapp.tsv` |
| Tsinghua App Usage | IEEE DataPort | `bench/traces/tsinghua.txt` |

Run `bash scripts/fetch_datasets.sh` to download LSApp. Larger datasets may
require manual registration — see their upstream READMEs.

## Running

```bash
# Python (authoritative in v0.1):
cd python && python -m memogent.cli.main bench --trace ../bench/traces/lsapp.tsv

# C++ (Phase 2):
./build/examples/cpp_cli/mem bench --trace bench/traces/lsapp.tsv
```
