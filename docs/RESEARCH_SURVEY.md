# Research survey

Summary of the literature that shaped Memogent's design. Full PDFs linked.

## App-usage prediction

- **Aliannejadi et al. — *Context-Aware Target Apps Selection and Recommendation for Enhancing Personal Mobile Assistants* (ACM TOIS 2020)**. The LSApp dataset: 599 k events, 87 apps, 292 users. Source of the canonical public benchmark.
- **Li, Qu, Wang — *TGT: Temporal Gating Transformer for Smartphone App Usage Prediction* (arXiv:2502.16957 v2, 2025)**. Temporal gating module conditions representations on hour-of-day; outperforms 15 baselines on LSApp.
- **Khaokaew, Xue, Salim — *MAPLE: Mobile App Prediction Leveraging LLM Embeddings* (arXiv:2309.08648)**. Uses LLM text embeddings to handle cold-start.
- **Yu, Li, Xu, Zhang — *Tsinghua App Usage Dataset* (IEEE DataPort)**. 1 k users × 2 k apps with base-station + traffic context.

## Cache replacement policies

- **Megiddo & Modha — *ARC: A Self-Tuning, Low Overhead Replacement Cache* (USENIX FAST 2003)**. The original Adaptive Replacement Cache; we extend it to *ContextARC* by adding a future-hint promote from the Predictor.
- **Vietri, Rodriguez — *SIEVE* (NSDI 2024)**. Simpler than ARC, competitive on web workloads. Candidate for v0.3.
- **Various — LRU/LFU/2Q/LIRS**. Textbook references.

## On-device memory for LLMs

- **Kwon et al. — *Efficient Memory Management for Large Language Model Serving with PagedAttention / vLLM* (SOSP 2023)**. Virtual-memory-style KV pages.
- **Wang et al. — *Self-Attention-Guided KV Cache Eviction (SAGE-KV)* (arXiv:2503.08879)**. One-pass top-k over attention heads.
- **Rehg — *KV-Compress* (arXiv:2410.00161)**. Variable-rate per-head compression inside PagedAttention.
- **Xiao et al. — *StreamingLLM* (arXiv:2309.17453)**. Sink + recent window pattern we embed in `KVCacheManager`.
- **Zhang et al. — *H2O: Heavy-Hitter Oracle for Efficient Generative Inference* (NeurIPS 2023)**. Salience-based eviction.

## Mobile-agentic systems

- **Liu, Zhang et al. — *MobiMem: Memory-Centric Agent System for Mobile* (arXiv:2512.15784)**. Profile / experience / action memory for Android agents — 280× faster than GraphRAG, Snapdragon-validated.
- **Zhao et al. — *AME: Heterogeneous Agentic Memory Engine for Smartphones* (arXiv:2511.19192)**. Joint CPU/GPU/NPU vector operations on mobile SoCs.
- **Vijayvargiya & Lokesh (Samsung + CMU) — *Efficient On-Device Agents via Adaptive Context Management* (arXiv:2511.03728)**. Context State Object compression via LoRA; 6× context reduction.

## Why these matter for Memogent

- We treat the predictor as a pluggable port so TGT / Atten-Transformer / MAPLE-lite checkpoints can be dropped in with zero code changes.
- ContextARC is Memogent's small original contribution — cheap augmentation of ARC with predictor hints.
- KV cache management borrows the Sink+Recent idea from StreamingLLM and the heavy-hitter salience from H2O / SAGE-KV without requiring any one vendor integration.
- MobiMem and AME inspired the cross-layer coupling: the agent's semantic memory drives system-memory allocation, not the other way around.
