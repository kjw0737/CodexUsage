// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "CodexUsageMac",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "CodexUsageMac", targets: ["CodexUsageMac"])
    ],
    targets: [
        .executableTarget(name: "CodexUsageMac"),
        .testTarget(name: "CodexUsageMacTests", dependencies: ["CodexUsageMac"])
    ]
)