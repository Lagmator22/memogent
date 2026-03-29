// SPDX-License-Identifier: MIT
package dev.memogent

import android.content.Context
import android.util.Log
import java.io.File
import java.util.concurrent.atomic.AtomicLong

/** One-to-one Kotlin facade for `mem::Orchestrator`. */
class Orchestrator private constructor(private val handle: AtomicLong) : AutoCloseable {

    data class Prediction(val appId: String, val score: Float, val reason: String)

    enum class Pool(val value: Int) {
        APP_STATE(0),
        MODEL_WEIGHTS(1),
        KV_CACHE(2),
        EMBEDDING(3),
        OTHER(255),
    }

    enum class PowerMode(val value: Int) {
        FULL(0), REDUCED(1), MINIMAL(2), PAUSED(3);
        companion object {
            fun of(v: Int) = values().firstOrNull { it.value == v } ?: FULL
        }
    }

    init {
        Memogent  // trigger native library load
    }

    // ----------------------------------------------------------------------
    // ingestion
    // ----------------------------------------------------------------------
    fun recordAppOpen(appId: String) {
        nativeRecordAppOpen(handle.get(), appId)
    }

    fun recordAppClose(appId: String) {
        nativeRecordAppClose(handle.get(), appId)
    }

    fun recordCacheHit(pool: Pool) {
        nativeRecordCacheHit(handle.get(), pool.value)
    }

    fun recordCacheMiss(pool: Pool) {
        nativeRecordCacheMiss(handle.get(), pool.value)
    }

    fun updateDeviceState(
        batteryPct: Float,
        thermal: Float,
        ramFreeMb: Float,
        charging: Boolean,
        lowPowerMode: Boolean
    ) {
        nativeUpdateDeviceState(
            handle.get(), batteryPct, thermal, ramFreeMb, charging, lowPowerMode
        )
    }

    // ----------------------------------------------------------------------
    // queries
    // ----------------------------------------------------------------------
    fun predictNextApps(k: Int = 3): List<Prediction> {
        val raw = nativePredictNext(handle.get(), k)
        return raw.map { line ->
            val parts = line.split("|")
            Prediction(
                appId = parts.getOrNull(0).orEmpty(),
                score = parts.getOrNull(1)?.toFloatOrNull() ?: 0f,
                reason = parts.getOrNull(2).orEmpty()
            )
        }
    }

    fun powerMode(): PowerMode = PowerMode.of(nativePowerMode(handle.get()))

    fun tick() {
        nativeTick(handle.get())
    }

    fun kpisJson(): String = nativeKpisJson(handle.get())

    fun reset() {
        nativeReset(handle.get())
    }

    /** Attach an Android-aware telemetry source that polls BatteryManager etc. */
    fun bindUsageStatsSource(context: Context): UsageStatsSource =
        UsageStatsSource(this, context.applicationContext)

    override fun close() {
        val h = handle.getAndSet(0)
        if (h != 0L) nativeDestroy(h)
    }

    // ----------------------------------------------------------------------
    // JNI declarations
    // ----------------------------------------------------------------------
    private external fun nativeCreate(
        dataDir: String, predictor: String, cachePolicy: String
    ): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeRecordAppOpen(handle: Long, appId: String)
    private external fun nativeRecordAppClose(handle: Long, appId: String)
    private external fun nativeRecordCacheHit(handle: Long, pool: Int)
    private external fun nativeRecordCacheMiss(handle: Long, pool: Int)
    private external fun nativeUpdateDeviceState(
        handle: Long,
        batteryPct: Float,
        thermal: Float,
        ramFreeMb: Float,
        charging: Boolean,
        lowPowerMode: Boolean
    )
    private external fun nativePredictNext(handle: Long, k: Int): Array<String>
    private external fun nativePowerMode(handle: Long): Int
    private external fun nativeTick(handle: Long)
    private external fun nativeKpisJson(handle: Long): String
    private external fun nativeReset(handle: Long)

    companion object {
        private const val TAG = "memogent"

        @JvmStatic
        fun create(
            context: Context,
            predictor: String = "freq_recency",
            cachePolicy: String = "context_arc"
        ): Orchestrator {
            val dir = File(context.filesDir, "memogent").apply { mkdirs() }
            val h = AtomicLong(0)
            val o = Orchestrator(h)
            val native = o.nativeCreate(dir.absolutePath, predictor, cachePolicy)
            if (native == 0L) {
                throw IllegalStateException("memogent: native orchestrator failed to initialize")
            }
            h.set(native)
            Log.i(TAG, "memogent v${Memogent.version} initialized at ${dir.absolutePath}")
            return o
        }
    }
}
