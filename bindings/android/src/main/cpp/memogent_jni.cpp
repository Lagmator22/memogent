// SPDX-License-Identifier: MIT
// Thin JNI bridge. Kotlin holds a long (jlong) handle that we reinterpret as a
// mem_orchestrator_t* pointer. All methods are defensive: null-checks, no
// exceptions cross the JNI boundary.

#include <jni.h>
#include <android/log.h>

#include <cstring>
#include <string>

#include "memogent_c.h"

#define LOG_TAG "memogent"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
inline mem_orchestrator_t* handle(jlong h) {
    return reinterpret_cast<mem_orchestrator_t*>(static_cast<uintptr_t>(h));
}

inline std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out{c ? c : ""};
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}
}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_dev_memogent_Memogent_nativeVersion(JNIEnv* env, jclass) {
    return env->NewStringUTF(mem_version_string());
}

JNIEXPORT jlong JNICALL
Java_dev_memogent_Orchestrator_nativeCreate(JNIEnv* env, jobject,
                                            jstring data_dir,
                                            jstring predictor,
                                            jstring cache_policy) {
    mem_config_t* cfg = mem_config_create();
    if (!cfg) return 0;
    auto dd = jstr(env, data_dir);
    auto pr = jstr(env, predictor);
    auto cp = jstr(env, cache_policy);
    if (!dd.empty()) mem_config_set_string(cfg, "data_dir", dd.c_str());
    if (!pr.empty()) mem_config_set_string(cfg, "predictor", pr.c_str());
    if (!cp.empty()) mem_config_set_string(cfg, "cache_policy", cp.c_str());

    mem_orchestrator_t* orch = nullptr;
    const auto st = mem_orchestrator_create(cfg, &orch);
    mem_config_destroy(cfg);
    if (st != MEM_OK) {
        LOGE("mem_orchestrator_create failed: %d", st);
        return 0;
    }
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(orch));
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeDestroy(JNIEnv*, jobject, jlong h) {
    if (h != 0) mem_orchestrator_destroy(handle(h));
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeRecordAppOpen(JNIEnv* env, jobject,
                                                   jlong h, jstring app_id) {
    if (!h || !app_id) return;
    auto s = jstr(env, app_id);
    mem_orchestrator_record_app_open(handle(h), s.c_str());
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeRecordAppClose(JNIEnv* env, jobject,
                                                    jlong h, jstring app_id) {
    if (!h || !app_id) return;
    auto s = jstr(env, app_id);
    mem_orchestrator_record_app_close(handle(h), s.c_str());
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeRecordCacheHit(JNIEnv*, jobject,
                                                    jlong h, jint pool) {
    if (!h) return;
    mem_orchestrator_record_cache_hit(handle(h), static_cast<mem_pool_t>(pool));
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeRecordCacheMiss(JNIEnv*, jobject,
                                                     jlong h, jint pool) {
    if (!h) return;
    mem_orchestrator_record_cache_miss(handle(h), static_cast<mem_pool_t>(pool));
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeUpdateDeviceState(JNIEnv*, jobject,
                                                       jlong h,
                                                       jfloat battery_pct,
                                                       jfloat thermal,
                                                       jfloat ram_free_mb,
                                                       jboolean charging,
                                                       jboolean low_power) {
    if (!h) return;
    mem_device_state_t s;
    s.battery_pct = battery_pct;
    s.thermal = thermal;
    s.ram_free_mb = ram_free_mb;
    s.charging = charging ? 1 : 0;
    s.low_power_mode = low_power ? 1 : 0;
    s.timestamp = 0.0;
    mem_orchestrator_update_device_state(handle(h), &s);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_memogent_Orchestrator_nativePredictNext(JNIEnv* env, jobject,
                                                 jlong h, jint k) {
    const jclass string_cls = env->FindClass("java/lang/String");
    if (!h) return env->NewObjectArray(0, string_cls, nullptr);
    const size_t cap = static_cast<size_t>(std::max(1, std::min(k, 16)));
    mem_prediction_t* buf = new mem_prediction_t[cap];
    size_t n = mem_orchestrator_predict_next(handle(h), buf, cap);
    // Each prediction -> "app_id|score|reason" joined string for easy Kotlin parse.
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(n), string_cls, nullptr);
    for (size_t i = 0; i < n; ++i) {
        std::string joined;
        joined.reserve(64);
        joined.append(buf[i].app_id ? buf[i].app_id : "");
        joined.append("|");
        joined.append(std::to_string(buf[i].score));
        joined.append("|");
        joined.append(buf[i].reason ? buf[i].reason : "");
        jstring s = env->NewStringUTF(joined.c_str());
        env->SetObjectArrayElement(out, static_cast<jsize>(i), s);
        env->DeleteLocalRef(s);
    }
    delete[] buf;
    return out;
}

JNIEXPORT jint JNICALL
Java_dev_memogent_Orchestrator_nativePowerMode(JNIEnv*, jobject, jlong h) {
    if (!h) return MEM_POWER_FULL;
    return static_cast<jint>(mem_orchestrator_power_mode(handle(h)));
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeTick(JNIEnv*, jobject, jlong h) {
    if (!h) return;
    mem_orchestrator_tick(handle(h));
}

JNIEXPORT jstring JNICALL
Java_dev_memogent_Orchestrator_nativeKpisJson(JNIEnv* env, jobject, jlong h) {
    if (!h) return env->NewStringUTF("");
    const char* s = mem_orchestrator_kpis_json(handle(h));
    return env->NewStringUTF(s ? s : "");
}

JNIEXPORT void JNICALL
Java_dev_memogent_Orchestrator_nativeReset(JNIEnv*, jobject, jlong h) {
    if (!h) return;
    mem_orchestrator_reset(handle(h));
}

}  // extern "C"
