// SPDX-License-Identifier: MIT
import SwiftUI

struct AppCard: Identifiable {
    let id: String
    let icon: String
    let label: String
}

private let appPalette: [AppCard] = [
    AppCard(id: "com.alarm",     icon: "alarm.fill",         label: "Alarm"),
    AppCard(id: "com.weather",   icon: "cloud.sun.fill",     label: "Weather"),
    AppCard(id: "com.news",      icon: "newspaper.fill",     label: "News"),
    AppCard(id: "com.messenger", icon: "message.fill",       label: "Messenger"),
    AppCard(id: "com.mail",      icon: "envelope.fill",      label: "Mail"),
    AppCard(id: "com.calendar",  icon: "calendar",           label: "Calendar"),
    AppCard(id: "com.maps",      icon: "map.fill",           label: "Maps"),
    AppCard(id: "com.music",     icon: "music.note",         label: "Music"),
    AppCard(id: "com.podcast",   icon: "mic.fill",           label: "Podcast"),
    AppCard(id: "com.docs",      icon: "doc.text.fill",      label: "Docs"),
    AppCard(id: "com.slack",     icon: "number",             label: "Slack"),
    AppCard(id: "com.terminal",  icon: "terminal.fill",      label: "Terminal"),
    AppCard(id: "com.video",     icon: "play.rectangle.fill", label: "Video"),
    AppCard(id: "com.games",     icon: "gamecontroller.fill", label: "Games"),
    AppCard(id: "com.book",      icon: "book.fill",          label: "Book"),
]

private let routines: [(name: String, sequence: [String])] = [
    ("Morning",  ["com.alarm", "com.weather", "com.news", "com.messenger"]),
    ("Commute",  ["com.maps", "com.music", "com.podcast"]),
    ("Work",     ["com.mail", "com.docs", "com.slack"]),
    ("Evening",  ["com.video", "com.messenger", "com.music"]),
]

struct ContentView: View {
    @ObservedObject var engine: MemogentEngine

    var body: some View {
        VStack(spacing: 16) {
            kpiBar
            predictionStrip
            cacheStrip
            appGrid
            routineRow
        }
        .padding()
    }

    private var kpiBar: some View {
        HStack(spacing: 12) {
            kpiCell(title: "Hit Rate",       value: pct(engine.kpis.cacheHitRate))
            kpiCell(title: "vs LRU",         value: pct(engine.kpis.baselineHitRate))
            kpiCell(title: "Pred Top-3",     value: pct(engine.kpis.predictionAccuracyTop3))
            kpiCell(title: "Thrashing ↓",    value: pctSigned(engine.kpis.thrashingReductionPct))
            kpiCell(title: "Mem Util ↑",     value: pctSigned(engine.kpis.memoryUtilizationPct))
        }
    }

    private func kpiCell(title: String, value: String) -> some View {
        VStack(spacing: 2) {
            Text(value).font(.system(.headline, design: .monospaced))
            Text(title).font(.caption).foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
        .background(Color.secondary.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    private var predictionStrip: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Next likely").font(.caption).foregroundStyle(.secondary)
            HStack(spacing: 8) {
                if engine.predictions.isEmpty {
                    Text("Open an app to start prediction").font(.callout).foregroundStyle(.tertiary)
                } else {
                    ForEach(engine.predictions) { pred in
                        chip(label: shortName(pred.appId), color: .blue.opacity(0.15))
                    }
                }
                Spacer()
            }
        }
    }

    private var cacheStrip: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Cache (hot first)").font(.caption).foregroundStyle(.secondary)
            HStack(spacing: 8) {
                if engine.cacheView.isEmpty {
                    Text("Empty").font(.callout).foregroundStyle(.tertiary)
                } else {
                    ForEach(engine.cacheView, id: \.self) { id in
                        chip(label: shortName(id), color: .green.opacity(0.15))
                    }
                }
                Spacer()
            }
        }
    }

    private func chip(label: String, color: Color) -> some View {
        Text(label)
            .font(.caption)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(color)
            .clipShape(Capsule())
    }

    private var appGrid: some View {
        let columns = [GridItem(.adaptive(minimum: 64), spacing: 8)]
        return ScrollView {
            LazyVGrid(columns: columns, spacing: 8) {
                ForEach(appPalette) { app in
                    Button {
                        engine.recordOpen(app.id, hour: currentHour())
                    } label: {
                        VStack(spacing: 4) {
                            Image(systemName: app.icon).font(.title2)
                            Text(app.label).font(.caption2).lineLimit(1)
                        }
                        .frame(width: 64, height: 64)
                        .background(Color.secondary.opacity(0.10))
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    private var routineRow: some View {
        HStack {
            ForEach(routines, id: \.name) { routine in
                Button("Replay \(routine.name)") {
                    Task { await replay(routine.sequence) }
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        }
    }

    private func replay(_ sequence: [String]) async {
        for id in sequence {
            engine.recordOpen(id, hour: currentHour())
            try? await Task.sleep(nanoseconds: 120_000_000)
        }
    }

    private func currentHour() -> Int {
        Calendar.current.component(.hour, from: Date())
    }

    private func shortName(_ id: String) -> String {
        id.split(separator: ".").last.map(String.init)?.capitalized ?? id
    }

    private func pct(_ v: Double) -> String { String(format: "%.0f%%", v * 100) }
    private func pctSigned(_ v: Double) -> String { String(format: "%+.0f%%", v) }
}
