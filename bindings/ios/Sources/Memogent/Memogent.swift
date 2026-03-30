// SPDX-License-Identifier: MIT
// Memogent — Swift wrapper over the Memogent C ABI.
//
// Public surface:
//   - Memogent.Orchestrator — thread-safe handle tied to a data directory.
//   - Memogent.Prediction, MemoryKind, PowerMode — value types that mirror
//     the C ABI enums.
//   - attachThermalHook() — wires ProcessInfo.thermalState → PowerMode.
//
// Everything below the .swift layer delegates to the C ABI so behavior is
// identical across iOS, macOS, and Android.

import Foundation
import MemogentC

public enum Memogent {

    // MARK: - Error

    public enum Error: Swift.Error {
        case initializationFailed(status: Int32)
        case invalidArgument(String)
        case internalError(status: Int32)

        public init(status: mem_status_t) {
            switch status {
            case MEM_OK:
                self = .internalError(status: 0)
            case MEM_ERR_INVALID_ARGUMENT:
                self = .invalidArgument("C ABI reported invalid argument")
            default:
                self = .internalError(status: status.rawValue)
            }
        }
    }

    // MARK: - Enums

    public enum PowerMode: Int32 {
        case full = 0
        case reduced = 1
        case minimal = 2
        case paused = 3

        fileprivate init(_ raw: mem_power_mode_t) {
            switch raw {
            case MEM_POWER_FULL: self = .full
            case MEM_POWER_REDUCED: self = .reduced
            case MEM_POWER_MINIMAL: self = .minimal
            case MEM_POWER_PAUSED: self = .paused
            default: self = .full
            }
        }
    }

    public enum MemoryPool: Int32 {
        case appState = 0
        case modelWeights = 1
        case kvCache = 2
        case embedding = 3
        case other = 255

        fileprivate var c: mem_pool_t {
            mem_pool_t(rawValue: UInt32(rawValue)) ?? MEM_POOL_OTHER
        }
    }

    // MARK: - Value types

    public struct Prediction: Equatable {
        public let appId: String
        public let score: Float
        public let reason: String
    }

    public struct KpiSnapshot {
        public let appLoadTimeImprovementPct: Double
        public let launchTimeImprovementPct: Double
        public let thrashingReductionPct: Double
        public let stabilityIssues: UInt64
        public let predictionAccuracyTop1: Double
        public let predictionAccuracyTop3: Double
        public let cacheHitRate: Double
        public let memoryUtilizationEfficiencyPct: Double
        public let p50DecisionLatencyMs: Double
        public let p95DecisionLatencyMs: Double
        public let p99DecisionLatencyMs: Double

        fileprivate init(_ k: mem_kpi_snapshot_t) {
            self.appLoadTimeImprovementPct = k.app_load_time_improvement_pct
            self.launchTimeImprovementPct = k.launch_time_improvement_pct
            self.thrashingReductionPct = k.thrashing_reduction_pct
            self.stabilityIssues = k.stability_issues
            self.predictionAccuracyTop1 = k.prediction_accuracy_top1
            self.predictionAccuracyTop3 = k.prediction_accuracy_top3
            self.cacheHitRate = k.cache_hit_rate
            self.memoryUtilizationEfficiencyPct = k.memory_utilization_efficiency_pct
            self.p50DecisionLatencyMs = k.p50_decision_latency_ms
            self.p95DecisionLatencyMs = k.p95_decision_latency_ms
            self.p99DecisionLatencyMs = k.p99_decision_latency_ms
        }
    }

    // MARK: - Orchestrator

    public final class Orchestrator {
        private let handle: OpaquePointer
        #if canImport(UIKit)
        private var thermalObserver: NSObjectProtocol?
        #endif

        public static var version: String { String(cString: mem_version_string()) }

        public init(dataDir: URL,
                    predictor: String = "freq_recency",
                    cachePolicy: String = "context_arc") throws {
            try FileManager.default.createDirectory(at: dataDir, withIntermediateDirectories: true)

            let cfg = mem_config_create()!
            defer { mem_config_destroy(cfg) }

            _ = dataDir.path.withCString { mem_config_set_string(cfg, "data_dir", $0) }
            _ = predictor.withCString { mem_config_set_string(cfg, "predictor", $0) }
            _ = cachePolicy.withCString { mem_config_set_string(cfg, "cache_policy", $0) }

            var opaque: UnsafeMutablePointer<mem_orchestrator>? = nil
            let status = mem_orchestrator_create(cfg, &opaque)
            guard status == MEM_OK, let ptr = opaque else {
                throw Error(status: status)
            }
            self.handle = OpaquePointer(ptr)
        }

        deinit {
            #if canImport(UIKit)
            if let obs = thermalObserver {
                NotificationCenter.default.removeObserver(obs)
            }
            #endif
            mem_orchestrator_destroy(UnsafeMutablePointer(handle))
        }

        // MARK: ingestion helpers

        public func recordAppOpen(appId: String) {
            _ = appId.withCString {
                mem_orchestrator_record_app_open(UnsafeMutablePointer(handle), $0)
            }
        }

        public func recordAppClose(appId: String) {
            _ = appId.withCString {
                mem_orchestrator_record_app_close(UnsafeMutablePointer(handle), $0)
            }
        }

        public func recordCacheHit(pool: MemoryPool) {
            _ = mem_orchestrator_record_cache_hit(UnsafeMutablePointer(handle), pool.c)
        }

        public func recordCacheMiss(pool: MemoryPool) {
            _ = mem_orchestrator_record_cache_miss(UnsafeMutablePointer(handle), pool.c)
        }

        // MARK: queries

        public func predictNextApps(k: Int = 3) -> [Prediction] {
            let cap = max(1, min(k, 16))
            let buf = UnsafeMutablePointer<mem_prediction_t>.allocate(capacity: cap)
            defer { buf.deallocate() }
            let n = mem_orchestrator_predict_next(UnsafeMutablePointer(handle), buf, cap)
            var out = [Prediction]()
            out.reserveCapacity(Int(n))
            for i in 0..<Int(n) {
                let p = buf[i]
                out.append(Prediction(appId: String(cString: p.app_id),
                                      score: p.score,
                                      reason: String(cString: p.reason)))
            }
            return out
        }

        public func powerMode() -> PowerMode {
            PowerMode(mem_orchestrator_power_mode(UnsafeMutablePointer(handle)))
        }

        @discardableResult
        public func tick() -> Bool {
            mem_orchestrator_tick(UnsafeMutablePointer(handle)) == MEM_OK
        }

        public func kpis() -> KpiSnapshot {
            KpiSnapshot(mem_orchestrator_kpis(UnsafeMutablePointer(handle)))
        }

        public func kpisJson() -> String {
            String(cString: mem_orchestrator_kpis_json(UnsafeMutablePointer(handle)))
        }

        public func reset() {
            _ = mem_orchestrator_reset(UnsafeMutablePointer(handle))
        }

        // MARK: device state

        public func updateDeviceState(batteryPct: Float,
                                      thermal: Float,
                                      ramFreeMB: Float,
                                      charging: Bool,
                                      lowPowerMode: Bool) {
            var s = mem_device_state_t(
                battery_pct: batteryPct,
                thermal: thermal,
                ram_free_mb: ramFreeMB,
                charging: charging ? 1 : 0,
                low_power_mode: lowPowerMode ? 1 : 0,
                timestamp: Date().timeIntervalSince1970
            )
            _ = mem_orchestrator_update_device_state(UnsafeMutablePointer(handle), &s)
        }

        /// Wire ProcessInfo.thermalState and UIDevice battery into the arbiter.
        /// On iOS/tvOS/watchOS only; no-op on macOS.
        public func attachThermalHook() {
            #if canImport(UIKit)
            let center = NotificationCenter.default
            self.thermalObserver = center.addObserver(
                forName: ProcessInfo.thermalStateDidChangeNotification,
                object: nil,
                queue: .main
            ) { [weak self] _ in
                self?.syncFromProcessInfo()
            }
            syncFromProcessInfo()
            #endif
        }

        #if canImport(UIKit)
        private func syncFromProcessInfo() {
            let thermal: Float
            switch ProcessInfo.processInfo.thermalState {
            case .nominal: thermal = 0.2
            case .fair:    thermal = 0.45
            case .serious: thermal = 0.70
            case .critical: thermal = 0.92
            @unknown default: thermal = 0.3
            }
            updateDeviceState(
                batteryPct: 100.0,  // production apps should query UIDevice / batteryMonitoringEnabled
                thermal: thermal,
                ramFreeMB: 512.0,
                charging: false,
                lowPowerMode: ProcessInfo.processInfo.isLowPowerModeEnabled
            )
        }
        #endif
    }
}
