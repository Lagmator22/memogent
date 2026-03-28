// SPDX-License-Identifier: MIT
package dev.memogent

/**
 * Memogent entry point. Loads the JNI shared library exactly once and
 * exposes small static helpers.
 */
object Memogent {
    init { System.loadLibrary("memogent_jni") }

    val version: String
        get() = nativeVersion()

    @JvmStatic external fun nativeVersion(): String
}
