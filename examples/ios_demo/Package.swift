// swift-tools-version: 5.9
// SPDX-License-Identifier: MIT
//
// Lets the demo run on macOS via `swift run MemogentDemo` without a full
// Xcode project. For iOS Simulator: open this folder in Xcode (File → Open),
// add an iOS App target, and include the three files in `MemogentDemo/`.

import PackageDescription

let package = Package(
    name: "MemogentDemo",
    platforms: [
        .macOS(.v13),
        .iOS(.v16),
    ],
    products: [
        .executable(name: "MemogentDemo", targets: ["MemogentDemo"]),
    ],
    targets: [
        .executableTarget(
            name: "MemogentDemo",
            path: "MemogentDemo"
        ),
    ]
)
