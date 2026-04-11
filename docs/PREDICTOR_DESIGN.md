# Predictor design

## Why a zoo

The predictor is the lever that moves every downstream KPI. So we ship four
predictors you can swap with a config string:

| Name | Lines | Trainable | Size | HR@3 target (LSApp) |
|---|---:|---|---|---:|
| MFU | 60 | ✗ | ~1 kB | 35% (baseline) |
| MRU | 60 | ✗ | ~1 kB | 45% |
| Markov-1 | 120 | online | ~40 kB | 60% |
| FreqRecency | 150 | online | ~50 kB | 65% |
| LSTM (Phase 2) | 200 | offline | ~150 kB | 75%+ |
| Atten-Transformer (Phase 3) | 300 | offline | ~1 MB | 80%+ |

Even the non-trainable baselines (MFU, MRU) are kept because they are the
published baselines Samsung expects us to beat.

## Features we care about

Per Atten-Transformer and TGT (both the canonical 2025 papers on LSApp):

1. **App sequence** — the last N app opens.
2. **Time features** — hour-of-day, day-of-week, sin/cos encoding.
3. **User profile** — embedding id for the current user.
4. **Context tags** — "home / work / commute" if available, "WiFi / LTE" if
   available (AME and MobiMem use these).

The shipping baselines use only (1) and (2). LSTM/Transformer will add (3)
and (4).

## Training

- `memogent train` will eventually spin up a PyTorch LSTM on LSApp,
  emit a tiny ONNX / LiteRT checkpoint, and drop it in `assets/`.
- The on-device inference path is MEM_WITH_ONNX in C++ — runtime already has
  the interface slot open.

## Failure modes & fallbacks

Every predictor falls back to global unigram distribution when it has no
context (e.g. first event after boot). This is essential: predictor failures
should never stall the Arbiter.

## References

- Li, Qu, Wang — *TGT: Temporal Gating Transformer for Smartphone App Usage Prediction*, arXiv:2502.16957.
- Khaokaew, Xue, Salim — *MAPLE: Mobile App Prediction Leveraging LLM Embeddings*, arXiv:2309.08648.
- Aliannejadi et al. — *LSApp*, ACM TOIS 2020.
