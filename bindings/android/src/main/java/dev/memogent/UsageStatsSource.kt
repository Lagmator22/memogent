// SPDX-License-Identifier: MIT
package dev.memogent

import android.app.ActivityManager
import android.app.usage.UsageStatsManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.PowerManager
import android.util.Log
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/**
 * Wires Android device-state signals into the Memogent orchestrator.
 *
 * Call [start] once after the host app has granted the `PACKAGE_USAGE_STATS`
 * special permission (via Settings -> Apps with Usage Access). Call [stop]
 * from onDestroy. Polls default every 2s.
 */
class UsageStatsSource internal constructor(
    private val orch: Orchestrator,
    private val context: Context
) : AutoCloseable {

    private var exec: ScheduledExecutorService? = null
    private val activity = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    private val usage = context.getSystemService(Context.USAGE_STATS_SERVICE) as? UsageStatsManager
    private val power = context.getSystemService(Context.POWER_SERVICE) as PowerManager

    fun start(periodMs: Long = 2_000) {
        if (exec != null) return
        val svc = Executors.newSingleThreadScheduledExecutor { r ->
            Thread(r, "memogent-usagesource").apply { isDaemon = true }
        }
        exec = svc
        svc.scheduleAtFixedRate(
            { runCatching { tick() }.onFailure { Log.w(TAG, "poll failed", it) } },
            0, periodMs, TimeUnit.MILLISECONDS
        )
    }

    fun stop() {
        exec?.shutdownNow()
        exec = null
    }

    override fun close() = stop()

    /** Exposed so host apps can forward foreground app transitions as they arrive. */
    fun reportForegroundApp(packageName: String) {
        orch.recordAppOpen(packageName)
    }

    private fun tick() {
        // battery
        val batIntent = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = batIntent?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = batIntent?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        val batteryPct = if (level >= 0 && scale > 0) (level * 100f / scale) else 100f
        val charging = batIntent?.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) ?: 0
        val isCharging = charging != 0
        val lowPower = power.isPowerSaveMode

        // memory
        val mi = ActivityManager.MemoryInfo()
        activity.getMemoryInfo(mi)
        val freeMb = (mi.availMem / (1024f * 1024f))

        // thermal (API 29+)
        val thermal = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            when (power.currentThermalStatus) {
                PowerManager.THERMAL_STATUS_NONE -> 0.1f
                PowerManager.THERMAL_STATUS_LIGHT -> 0.30f
                PowerManager.THERMAL_STATUS_MODERATE -> 0.55f
                PowerManager.THERMAL_STATUS_SEVERE -> 0.75f
                PowerManager.THERMAL_STATUS_CRITICAL -> 0.90f
                PowerManager.THERMAL_STATUS_EMERGENCY -> 0.95f
                PowerManager.THERMAL_STATUS_SHUTDOWN -> 1.0f
                else -> 0.25f
            }
        } else 0.25f

        orch.updateDeviceState(
            batteryPct = batteryPct,
            thermal = thermal,
            ramFreeMb = freeMb,
            charging = isCharging,
            lowPowerMode = lowPower
        )
        orch.tick()

        // Opportunistically sample the currently foreground app from UsageStatsManager
        val now = System.currentTimeMillis()
        val us = usage ?: return
        val stats = us.queryUsageStats(UsageStatsManager.INTERVAL_DAILY, now - 10_000, now)
        stats?.maxByOrNull { it.lastTimeUsed }?.let {
            if (it.lastTimeUsed > now - 5_000) {
                orch.recordAppOpen(it.packageName)
            }
        }
    }

    companion object {
        private const val TAG = "memogent-usage"
    }
}
