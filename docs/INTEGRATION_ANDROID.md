# Integrating Memogent into an Android app

## Add the dependency

```kotlin
// app/build.gradle.kts
dependencies {
    implementation("dev.memogent:memogent:0.1.0")
}
```

## Minimal setup

```kotlin
import dev.memogent.Memogent
import dev.memogent.Orchestrator

val orch = Orchestrator.create(applicationContext)
orch.recordAppOpen(packageName)
```

## Wire automatic telemetry

```kotlin
val source = orch.bindUsageStatsSource(applicationContext)
source.start()           // polls BatteryManager + PowerManager + UsageStatsManager
```

### Special permission: `PACKAGE_USAGE_STATS`

`UsageStatsManager` requires a one-time special permission granted via
`Settings.ACTION_USAGE_ACCESS_SETTINGS`. The library does *not* request it
automatically; the host app decides whether it wants automatic foreground
tracking.

```kotlin
fun ensureUsageAccess(activity: Activity) {
    val appOps = activity.getSystemService(Context.APP_OPS_SERVICE) as AppOpsManager
    val mode = appOps.checkOpNoThrow(
        "android:get_usage_stats",
        Process.myUid(), activity.packageName
    )
    if (mode != AppOpsManager.MODE_ALLOWED) {
        activity.startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS))
    }
}
```

If the user denies the permission, Memogent still works — the host is
expected to call `orch.recordAppOpen(...)` explicitly from its own foreground
lifecycle hooks.

## Threading

`Orchestrator` is thread-safe. Call `tick()` on `Dispatchers.Default` on a
1-second schedule:

```kotlin
lifecycleScope.launch(Dispatchers.Default) {
    while (isActive) {
        orch.tick()
        delay(1000)
    }
}
```

## Closing the orchestrator

```kotlin
override fun onDestroy() {
    source.stop()
    orch.close()
    super.onDestroy()
}
```

## Diagnostics

- `orch.kpisJson()` — JSON string with every KPI.
- `orch.powerMode()` — current QoS tier.
- `orch.reset()` — wipe the store.

## Deployment checklist

- Bundle the native libs for `arm64-v8a` (primary) and `armeabi-v7a` / `x86_64` (optional).
- R8 is fine; the Kotlin API is not reflective.
- ProGuard rules are included via `consumer-rules.pro`.
