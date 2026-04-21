// swift-tools-version: 5.9
// SPDX-License-Identifier: MIT
import PackageDescription

let package = Package(
    name: "Memogent",
    platforms: [
        .iOS(.v15),
        .macOS(.v13),
        .watchOS(.v9),
        .tvOS(.v15),
    ],
    products: [
        .library(name: "Memogent", targets: ["Memogent"]),
    ],
    targets: [
        // Thin Swift wrapper exposing a type-safe Orchestrator over the C ABI.
        .target(
            name: "Memogent",
            dependencies: ["MemogentC"],
            path: "Sources/Memogent"
        ),
        // The C ABI + compiled core bundled as a library so the package is
        // self-contained when checked out locally. For production you should
        // prefer linking against the prebuilt `memogent.xcframework` that our
        // build pipeline produces.
        .target(
            name: "MemogentC",
            dependencies: [],
            path: "Sources/MemogentC",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include"),
            ],
            cxxSettings: [
                .headerSearchPath("include"),
                .define("MEM_BUILDING_SHARED", to: "1"),
            ]
        ),
        .testTarget(
            name: "MemogentTests",
            dependencies: ["Memogent"],
            path: "Tests/MemogentTests"
        ),
    ],
    cxxLanguageStandard: .cxx2b
)
