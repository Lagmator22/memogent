// SPDX-License-Identifier: MIT
import SwiftUI

@main
struct MemogentDemoApp: App {
    @StateObject private var engine = MemogentEngine(capacity: 6)

    var body: some Scene {
        WindowGroup {
            ContentView(engine: engine)
        }
    }
}
