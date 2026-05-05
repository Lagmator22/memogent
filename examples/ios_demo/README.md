# Memogent iOS Demo

A self-contained SwiftUI demo showing **ContextARC** and the **MarkovBigram**
predictor running on iOS / macOS — the exact same algorithms as the C++ core
and Python reference, reimplemented in pure Swift so the demo builds without
linking the compiled C ABI.

## Why pure Swift?

The production binding lives at `bindings/ios/` and links against the C++23
core via a C ABI. For a tap-and-see demo we don't want judges to build a
cross-compiled C++ library + xcframework first — so this folder contains a
self-contained SwiftUI app that mirrors the algorithms one-to-one. When the
prebuilt `Memogent.xcframework` ships, the demo will switch over with a
single `import Memogent` change.

## Build

```bash
cd examples/ios_demo
open MemogentDemo.xcodeproj      # or: xcodebuild -scheme MemogentDemo
```

Targets iOS 16+ / macOS 13+. Runs in the iOS Simulator without an Apple ID.

## What you can do in the app

- Tap any app icon to record an `app_open` event.
- Watch the **Next likely** strip update live — those are the bigram
  predictor's top-3 picks, scored via the same blend used in the Python
  runtime (bigram + unigram + recency + hour-of-day).
- Watch the **Cache** strip — the 4 hottest entries in ContextARC's T2.
- Watch the **KPI bar** — live cache hit rate, prediction accuracy,
  and miss-reduction vs an internal LRU baseline running on the same
  events.
- Use **Replay routine** to fire one of the synthetic morning / commute /
  evening micro-sequences without tapping.

## Files

- `MemogentDemo/MemogentDemoApp.swift` — `@main` entrypoint.
- `MemogentDemo/ContentView.swift` — the SwiftUI dashboard.
- `MemogentDemo/MemogentEngine.swift` — pure-Swift port of the
  ContextARC + MarkovBigram + LRU baseline. Mirror of
  `python/memogent/{cache,predictor}` and `core/src/{caches,predictors}`.
