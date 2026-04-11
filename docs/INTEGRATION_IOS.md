# Integrating Memogent into an iOS app

## Add the package

```swift
// Package.swift
.package(url: "https://github.com/Lagmator22/memogent.git", branch: "main"),
```

Attach the `Memogent` library to your target.

## Minimal setup

```swift
import Memogent

let orch = try Memogent.Orchestrator(
    dataDir: .documentsDirectory.appendingPathComponent("memogent")
)
orch.attachThermalHook()   // automatically sync ProcessInfo.thermalState
```

## Hook into app lifecycle

Send events every time your app moves between states. The framework has no
OS-wide foreground-tracking privileges on iOS, so the host app is expected
to report the apps/screens it owns.

```swift
import UIKit

final class SceneCoordinator {
    static let shared = SceneCoordinator()
    let orch = try! Memogent.Orchestrator(dataDir: URL.documentsDirectory)

    func observe() {
        NotificationCenter.default.addObserver(
            forName: UIApplication.didBecomeActiveNotification,
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.orch.recordAppOpen(appId: Bundle.main.bundleIdentifier ?? "unknown")
        }
        NotificationCenter.default.addObserver(
            forName: UIApplication.didEnterBackgroundNotification,
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.orch.recordAppClose(appId: Bundle.main.bundleIdentifier ?? "unknown")
        }
    }
}
```

## Tick every second on a background queue

```swift
Task.detached(priority: .background) {
    while !Task.isCancelled {
        orch.tick()
        try? await Task.sleep(for: .seconds(1))
    }
}
```

## Wiring to llama.cpp (optional)

Build Memogent with `-DMEM_WITH_LLAMACPP=ON` and link against a prebuilt
llama.cpp xcframework. The `KVCacheManager` will then plug directly into
llama's paged attention and begin reporting KV telemetry.

## Diagnostics

- `orch.kpisJson()` returns a JSON string with the full KPI snapshot.
- `orch.powerMode()` returns `Memogent.PowerMode` — render it in a debug overlay.
- `orch.reset()` wipes every tier; wire it to your "Forget me" setting.

## Pitfalls

- Don't call `tick()` on the main thread in a loop.
- Keep the single `Orchestrator` instance alive for the whole session — creating it is cheap, but repeated instantiation defeats the predictor.
- `attachThermalHook()` depends on `UIKit`; wrap it in `#if canImport(UIKit)` if you target macOS Catalyst.
