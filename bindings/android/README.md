# Memogent for Android

An AAR that wraps the Memogent C++ core with JNI + Kotlin. Drop it into any
Android Studio project targeting API 28+.

## Gradle

```kotlin
repositories { mavenCentral() }

dependencies {
    implementation("dev.memogent:memogent:0.1.0")
}
```

## Usage

```kotlin
val orch = Memogent.Orchestrator.create(context)
orch.recordAppOpen("com.example.app")
val predictions = orch.predictNextApps(k = 3)

// Wire automatic telemetry from the Android system:
val usage = orch.bindUsageStatsSource(context)
usage.start()
```

`bindUsageStatsSource` polls `BatteryManager`, `PowerManager`,
`ActivityManager.MemoryInfo`, and (with the user's consent)
`UsageStatsManager` once every two seconds. The host app is responsible for
requesting the `PACKAGE_USAGE_STATS` special permission before calling it.

## Building locally

```bash
cd bindings/android
./gradlew assembleRelease
```

Requires Android NDK r26+ and Gradle 8+.
