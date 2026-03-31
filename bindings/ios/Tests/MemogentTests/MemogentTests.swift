// SPDX-License-Identifier: MIT
import XCTest
@testable import Memogent

final class MemogentTests: XCTestCase {
    func testVersion() {
        XCTAssertFalse(Memogent.Orchestrator.version.isEmpty)
    }

    func testBasicLifecycle() throws {
        let dir = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("memogent-test")
        let engine = try Memogent.Orchestrator(dataDir: dir)
        engine.recordAppOpen(appId: "com.foo.bar")
        engine.recordAppOpen(appId: "com.baz.qux")
        engine.recordAppOpen(appId: "com.foo.bar")
        let predictions = engine.predictNextApps(k: 3)
        XCTAssertFalse(predictions.isEmpty)
        _ = engine.tick()
        let snapshot = engine.kpis()
        XCTAssertGreaterThanOrEqual(snapshot.cacheHitRate, 0)
        engine.reset()
    }
}
