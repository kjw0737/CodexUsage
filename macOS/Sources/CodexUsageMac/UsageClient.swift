import Foundation
import CoreFoundation

struct QuotaWindow: Equatable {
    let remaining: Double?
    let resetsAt: Date?
    let durationMinutes: Int?

    static let unavailable = QuotaWindow(remaining: nil, resetsAt: nil, durationMinutes: nil)
}

struct UsageSnapshot {
    let short: QuotaWindow
    let long: QuotaWindow
    let resetCredits: Int?
    let expirations: [Date]
    let updatedAt: Date
}

enum UsageError: LocalizedError {
    case cliMissing
    case invalidResponse
    case missingCodexBucket
    case requestFailed
    case timedOut

    var errorDescription: String? {
        switch self {
        case .cliMissing: "Codex CLI를 찾지 못했습니다. 설정에서 경로를 지정하세요."
        case .invalidResponse: "Codex 사용량 응답을 해석하지 못했습니다."
        case .missingCodexBucket: "응답에 Codex 기본 한도가 없습니다."
        case .requestFailed: "Codex 사용량 조회에 실패했습니다. 로그인과 네트워크를 확인하세요."
        case .timedOut: "Codex가 25초 안에 응답하지 않았습니다."
        }
    }
}

enum UsageParser {
    static func parse(_ data: Data, now: Date = Date()) throws -> UsageSnapshot {
        guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              let result = root["result"] as? [String: Any] else {
            throw UsageError.invalidResponse
        }
        return try parseResult(result, now: now)
    }

    static func parseResult(_ result: [String: Any], now: Date = Date()) throws -> UsageSnapshot {
        let bucket: [String: Any]
        if let buckets = result["rateLimitsByLimitId"] as? [String: Any], !buckets.isEmpty {
            guard let codex = buckets["codex"] as? [String: Any] else {
                throw UsageError.missingCodexBucket
            }
            bucket = codex
        } else if let legacy = result["rateLimits"] as? [String: Any] {
            bucket = legacy
        } else {
            throw UsageError.invalidResponse
        }

        let credits = result["rateLimitResetCredits"] as? [String: Any]
        let rows = credits?["credits"] as? [[String: Any]] ?? []
        let expirations = rows.compactMap { row -> Date? in
            guard row["status"] as? String == "available",
                  let seconds = number(row["expiresAt"]), seconds > 0 else { return nil }
            return Date(timeIntervalSince1970: seconds)
        }
        return UsageSnapshot(
            short: window(bucket["primary"]),
            long: window(bucket["secondary"]),
            resetCredits: number(credits?["availableCount"]).map(Int.init),
            expirations: expirations,
            updatedAt: now
        )
    }

    private static func window(_ value: Any?) -> QuotaWindow {
        guard let object = value as? [String: Any],
              let used = number(object["usedPercent"]) else { return .unavailable }
        let reset = number(object["resetsAt"]).flatMap { $0 > 0 ? Date(timeIntervalSince1970: $0) : nil }
        return QuotaWindow(
            remaining: min(100, max(0, 100 - used)),
            resetsAt: reset,
            durationMinutes: number(object["windowDurationMins"]).map(Int.init)
        )
    }

    private static func number(_ value: Any?) -> Double? {
        guard let n = value as? NSNumber, CFGetTypeID(n) != CFBooleanGetTypeID() else { return nil }
        return n.doubleValue
    }
}

final class AlertTracker {
    private var previous: [Int: QuotaWindow] = [:]

    func reset() { previous.removeAll() }

    func update(_ snapshot: UsageSnapshot, warning: Int, alert: Int) -> [String] {
        var messages: [String] = []
        for (index, title, current) in [(0, "단기 한도", snapshot.short), (1, "장기 한도", snapshot.long)] {
            guard let remaining = current.remaining else { continue }
            if let old = previous[index], let oldRemaining = old.remaining {
                let before = level(oldRemaining, warning: warning, alert: alert)
                let after = level(remaining, warning: warning, alert: alert)
                let text: String?
                if after > before {
                    text = after == 2 ? "Alert 기준 도달" : "Warning 기준 도달"
                } else if after < before ||
                            (remaining > oldRemaining && (current.resetsAt ?? .distantPast) > (old.resetsAt ?? .distantPast)) {
                    text = "남은 사용량 회복"
                } else {
                    text = nil
                }
                if let text { messages.append("\(title): \(text) (\(Int(remaining.rounded()))% 남음)") }
            }
            previous[index] = current
        }
        return messages
    }

    static func level(_ remaining: Double, warning: Int, alert: Int) -> Int {
        remaining <= Double(alert) ? 2 : remaining <= Double(warning) ? 1 : 0
    }

    private func level(_ remaining: Double, warning: Int, alert: Int) -> Int {
        Self.level(remaining, warning: warning, alert: alert)
    }
}

enum CodexClient {
    static func findExecutable(override: String) -> String? {
        if !override.isEmpty { return FileManager.default.isExecutableFile(atPath: override) ? override : nil }
        let envPaths = (ProcessInfo.processInfo.environment["PATH"] ?? "").split(separator: ":").map(String.init)
        let paths = envPaths + ["/opt/homebrew/bin", "/usr/local/bin",
                                NSHomeDirectory() + "/.local/bin",
                                NSHomeDirectory() + "/.npm-global/bin"]
        for directory in paths {
            let candidate = (directory as NSString).appendingPathComponent("codex")
            if FileManager.default.isExecutableFile(atPath: candidate) { return candidate }
        }
        return nil
    }

    static func fetch(executable: String) throws -> UsageSnapshot {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = ["app-server", "--listen", "stdio://"]
        let input = Pipe()
        let output = Pipe()
        process.standardInput = input
        process.standardOutput = output
        process.standardError = FileHandle.nullDevice
        try process.run()

        let semaphore = DispatchSemaphore(value: 0)
        let lock = NSLock()
        var answer: Result<UsageSnapshot, Error>?
        let reader = Thread {
            var buffer = Data()
            while true {
                let chunk = output.fileHandleForReading.availableData
                if chunk.isEmpty { break }
                buffer.append(chunk)
                if buffer.count > 4 * 1024 * 1024 {
                    lock.lock(); answer = .failure(UsageError.invalidResponse); lock.unlock()
                    semaphore.signal()
                    return
                }
                while let newline = buffer.firstIndex(of: 10) {
                    let line = Data(buffer[..<newline])
                    buffer.removeSubrange(...newline)
                    guard let message = try? JSONSerialization.jsonObject(with: line) as? [String: Any],
                          let id = message["id"] as? Int else { continue }
                    if message["error"] != nil {
                        lock.lock(); answer = .failure(UsageError.requestFailed); lock.unlock()
                        semaphore.signal()
                        return
                    }
                    if id == 1 {
                        let request = "{\"method\":\"initialized\",\"params\":{}}\n{\"id\":2,\"method\":\"account/rateLimits/read\"}\n"
                        input.fileHandleForWriting.write(Data(request.utf8))
                    } else if id == 2 {
                        let parsed = Result { try UsageParser.parse(line) }
                        lock.lock(); answer = parsed; lock.unlock()
                        semaphore.signal()
                        return
                    }
                }
            }
            lock.lock()
            if answer == nil { answer = .failure(UsageError.requestFailed); semaphore.signal() }
            lock.unlock()
        }
        reader.start()
        let initialize = "{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"citopia_codex_usage_mac\",\"version\":\"1.0.2026.0926\"},\"capabilities\":{\"experimentalApi\":true}}}\n"
        input.fileHandleForWriting.write(Data(initialize.utf8))

        let completed = semaphore.wait(timeout: .now() + 25) == .success
        if process.isRunning { process.terminate() }
        input.fileHandleForWriting.closeFile()
        guard completed else { throw UsageError.timedOut }
        lock.lock(); let result = answer; lock.unlock()
        return try (result ?? .failure(UsageError.invalidResponse)).get()
    }
}