// SPDX-License-Identifier: MIT
//
// MemogentEngine — pure-Swift port of the ContextARC cache + MarkovBigram
// predictor used by the C++ core (`core/src/{caches,predictors}`) and the
// Python reference (`python/memogent/{cache,predictor}`).
//
// The intent of this file is to make the iOS demo runnable without first
// building the cross-platform C++ core. The algorithms are intentionally
// written line-for-line with the Python implementation so behavior is
// identical.

import Foundation
import SwiftUI

// MARK: - Public types

public struct AppPrediction: Identifiable, Equatable {
    public let appId: String
    public let score: Double
    public var id: String { appId }
}

public struct EngineKpis: Equatable {
    public var cacheHitRate: Double = 0
    public var baselineHitRate: Double = 0
    public var thrashingReductionPct: Double = 0
    public var predictionAccuracyTop3: Double = 0
    public var predictionAccuracyTop1: Double = 0
    public var totalEvents: Int = 0
    public var memoryUtilizationPct: Double = 0
    public var p95DecisionMs: Double = 0
}

// MARK: - LRU baseline

final class LRUBaseline {
    private let cap: Int
    private var order: [String] = []
    private var present: Set<String> = []
    private(set) var hits = 0
    private(set) var misses = 0
    private(set) var evictions = 0

    init(capacity: Int) { self.cap = capacity }

    func access(_ key: String) {
        if present.contains(key) {
            hits += 1
            order.removeAll { $0 == key }
            order.append(key)
        } else {
            misses += 1
            order.append(key)
            present.insert(key)
            if order.count > cap {
                let dropped = order.removeFirst()
                present.remove(dropped)
                evictions += 1
            }
        }
    }

    var hitRate: Double {
        let t = hits + misses
        return t == 0 ? 0 : Double(hits) / Double(t)
    }
}

// MARK: - ContextARC (simplified — T1/T2 + ghost lists)

final class ContextARCCache {
    private let cap: Int
    private var p: Int = 0
    private var t1: [String] = []   // recently seen (cold side)
    private var t2: [String] = []   // recurring (hot side)
    private var b1: [String] = []   // ghost: evicted from T1
    private var b2: [String] = []   // ghost: evicted from T2
    private(set) var hits = 0
    private(set) var misses = 0
    private(set) var evictions = 0

    init(capacity: Int) { self.cap = capacity }

    var residency: [String] { t2 + t1 }

    func access(_ key: String) {
        if let i = t1.firstIndex(of: key) {
            hits += 1
            t1.remove(at: i)
            t2.insert(key, at: 0)
            return
        }
        if let i = t2.firstIndex(of: key) {
            hits += 1
            t2.remove(at: i)
            t2.insert(key, at: 0)
            return
        }
        misses += 1
        if let i = b1.firstIndex(of: key) {
            p = min(cap, p + max(1, b2.count / max(1, b1.count)))
            replace(inB2: false)
            b1.remove(at: i)
            t2.insert(key, at: 0)
            return
        }
        if let i = b2.firstIndex(of: key) {
            p = max(0, p - max(1, b1.count / max(1, b2.count)))
            replace(inB2: true)
            b2.remove(at: i)
            t2.insert(key, at: 0)
            return
        }
        // fresh miss
        if t1.count + b1.count >= cap {
            if t1.count < cap {
                if !b1.isEmpty { b1.removeLast() }
                replace(inB2: false)
            } else if !t1.isEmpty {
                t1.removeLast()
                evictions += 1
            }
        } else {
            let total = t1.count + t2.count + b1.count + b2.count
            if total >= cap {
                if total == 2 * cap, !b2.isEmpty { b2.removeLast() }
                replace(inB2: false)
            }
        }
        t1.insert(key, at: 0)
    }

    /// Promotion-only hint channel (resident → T2). Pre-warming the cold
    /// top-1 is left to the higher-level Preloader to avoid eviction churn
    /// on hints with low confidence.
    func hintFuture(_ predictions: [AppPrediction]) {
        for p in predictions {
            if let i = t1.firstIndex(of: p.appId) {
                t1.remove(at: i)
                t2.insert(p.appId, at: 0)
            } else if let i = t2.firstIndex(of: p.appId), i > 0 {
                t2.remove(at: i)
                t2.insert(p.appId, at: 0)
            }
        }
    }

    private func replace(inB2: Bool) {
        if !t1.isEmpty, t1.count > p || (inB2 && t1.count == p) {
            let k = t1.removeLast()
            b1.insert(k, at: 0)
        } else if !t2.isEmpty {
            let k = t2.removeLast()
            b2.insert(k, at: 0)
        }
        evictions += 1
    }

    var hitRate: Double {
        let t = hits + misses
        return t == 0 ? 0 : Double(hits) / Double(t)
    }
}

// MARK: - MarkovBigram predictor (with backoff + recency + hour prior)

final class MarkovBigramPredictor {
    private var bigram: [String: [String: Int]] = [:]   // (a, b) → next counts; key = "a|b"
    private var unigramNext: [String: [String: Int]] = [:]
    private var unigram: [String: Int] = [:]
    private var hourCounts: [Int: [String: Int]] = [:]
    private var lastIdx: [String: Int] = [:]
    private var n = 0
    private var prev1 = ""
    private var prev2 = ""
    private var lastHour: Int? = nil

    private let wBi = 0.55
    private let wUni = 0.20
    private let wRec = 0.15
    private let wHr = 0.10

    func observe(appId: String, hour: Int? = nil) {
        n += 1
        unigram[appId, default: 0] += 1
        lastIdx[appId] = n
        if !prev1.isEmpty {
            unigramNext[prev1, default: [:]][appId, default: 0] += 1
        }
        if !prev1.isEmpty, !prev2.isEmpty {
            let key = "\(prev2)|\(prev1)"
            bigram[key, default: [:]][appId, default: 0] += 1
        }
        if let h = hour {
            hourCounts[h, default: [:]][appId, default: 0] += 1
        }
        lastHour = hour
        prev2 = prev1
        prev1 = appId
    }

    func predict(topK: Int = 3) -> [AppPrediction] {
        guard !prev1.isEmpty else { return [] }
        var scored: [String: Double] = [:]

        if !prev2.isEmpty, let ctx = bigram["\(prev2)|\(prev1)"] {
            let total = max(1, ctx.values.reduce(0, +) + 1)
            for (k, c) in ctx { scored[k, default: 0] += wBi * Double(c) / Double(total) }
        }
        if let ctx = unigramNext[prev1] {
            let total = max(1, ctx.values.reduce(0, +) + 1)
            for (k, c) in ctx { scored[k, default: 0] += wUni * Double(c) / Double(total) }
        }
        if !unigram.isEmpty {
            let grand = max(1, unigram.values.reduce(0, +))
            for (k, c) in unigram {
                let age = Double(n - (lastIdx[k] ?? 0)) / 50.0
                let rec = 1.0 / (1.0 + age)
                scored[k, default: 0] += wRec * rec * Double(c) / Double(grand)
            }
        }
        if let h = lastHour, let ctx = hourCounts[h] {
            let total = max(1, ctx.values.reduce(0, +) + 1)
            for (k, c) in ctx { scored[k, default: 0] += wHr * Double(c) / Double(total) }
        }
        return scored.sorted { $0.value > $1.value }
            .prefix(topK)
            .map { AppPrediction(appId: $0.key, score: $0.value) }
    }
}

// MARK: - Engine (orchestrator-equivalent)

@MainActor
public final class MemogentEngine: ObservableObject {
    @Published public private(set) var kpis = EngineKpis()
    @Published public private(set) var cacheView: [String] = []
    @Published public private(set) var predictions: [AppPrediction] = []
    @Published public private(set) var lastEvent: String = ""

    private let cache: ContextARCCache
    private let baseline: LRUBaseline
    private let predictor = MarkovBigramPredictor()
    private var predictionsSeen = 0
    private var top1Hits = 0
    private var top3Hits = 0
    private var lastDecisionMs: Double = 0

    public init(capacity: Int = 6) {
        self.cache = ContextARCCache(capacity: capacity)
        self.baseline = LRUBaseline(capacity: capacity)
    }

    public func recordOpen(_ appId: String, hour: Int? = nil) {
        let t0 = Date()
        // Score current predictions against the actual incoming event, then
        // observe + re-predict to feed the next tick.
        if !predictions.isEmpty {
            predictionsSeen += 1
            let ids = predictions.map(\.appId)
            if ids.first == appId { top1Hits += 1 }
            if ids.contains(appId) { top3Hits += 1 }
        }
        cache.access(appId)
        baseline.access(appId)
        predictor.observe(appId: appId, hour: hour)
        let preds = predictor.predict(topK: 3)
        cache.hintFuture(preds)
        predictions = preds
        cacheView = Array(cache.residency.prefix(6))
        lastEvent = appId
        lastDecisionMs = Date().timeIntervalSince(t0) * 1000
        recomputeKpis()
    }

    public func reset() {
        objectWillChange.send()
        predictions.removeAll()
        cacheView.removeAll()
        lastEvent = ""
        kpis = EngineKpis()
        // We can't truly reset internal state without rebuilding — simplest
        // is to swap with a fresh instance externally.
    }

    private func recomputeKpis() {
        let total = max(1, predictionsSeen)
        let baseMisses = max(1, baseline.misses)
        let curMisses = cache.misses
        let baseHit = baseline.hitRate
        let curHit = cache.hitRate
        let headroom = max(0.0001, 1.0 - baseHit)

        kpis = EngineKpis(
            cacheHitRate: curHit,
            baselineHitRate: baseHit,
            thrashingReductionPct: Double(baseMisses - curMisses) * 100.0 / Double(baseMisses),
            predictionAccuracyTop3: Double(top3Hits) / Double(total),
            predictionAccuracyTop1: Double(top1Hits) / Double(total),
            totalEvents: predictionsSeen,
            memoryUtilizationPct: (curHit - baseHit) * 100.0 / headroom,
            p95DecisionMs: lastDecisionMs
        )
    }
}
