import Foundation

struct TurnTokenUsage: Equatable {
    var model = ""
    var input: UInt64 = 0
    var cachedInput: UInt64 = 0
    var output: UInt64 = 0
    var reasoningOutput: UInt64 = 0
    var total: UInt64 = 0
}
struct ModelTokenStatistics: Equatable, Identifiable {
    let model: String
    let commands: Int
    let input: UInt64
    let cachedInput: UInt64
    let output: UInt64
    let reasoningOutput: UInt64
    let total: UInt64
    let minimum: UInt64
    let median: UInt64
    let maximum: UInt64
    var id: String { model }
    var averageInput: UInt64 { commands > 0 ? input / UInt64(commands) : 0 }
    var averageCachedInput: UInt64 { commands > 0 ? cachedInput / UInt64(commands) : 0 }
    var averageOutput: UInt64 { commands > 0 ? output / UInt64(commands) : 0 }
    var averageReasoningOutput: UInt64 { commands > 0 ? reasoningOutput / UInt64(commands) : 0 }
    var cacheRate: Double { input > 0 ? 100 * Double(cachedInput) / Double(input) : 0 }
}

enum TokenStatisticsStore {
    static var defaultSessionsDirectory: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent(".codex/sessions", isDirectory: true)
    }

    static func read(directory: URL = defaultSessionsDirectory) throws -> [TurnTokenUsage] {
        guard FileManager.default.fileExists(atPath: directory.path) else { return [] }
        guard let enumerator = FileManager.default.enumerator(
            at: directory, includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]
        ) else { return [] }
        var turns: [TurnTokenUsage] = []
        for case let file as URL in enumerator where file.pathExtension == "jsonl" {
            try read(file: file, into: &turns)
        }
        return turns
    }

    private static func read(file: URL, into turns: inout [TurnTokenUsage]) throws {
        let text = try String(contentsOf: file, encoding: .utf8)
        var turn = TurnTokenUsage()
        func finish() {
            if !turn.model.isEmpty && turn.total > 0 { turns.append(turn) }
            turn = TurnTokenUsage()
        }
        for line in text.split(whereSeparator: \.isNewline) {
            guard line.contains("\"type\":\"turn_context\"") ||
                  line.contains("\"type\":\"token_count\"") else { continue }
            guard let data = String(line).data(using: .utf8),
                  let root = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let payload = root["payload"] as? [String: Any],
                  let rootType = root["type"] as? String else { continue }
            let payloadType = payload["type"] as? String
            if rootType == "turn_context" {
                finish()
                turn.model = payload["model"] as? String ?? ""
            } else if rootType == "event_msg", payloadType == "token_count",
                      !turn.model.isEmpty,
                      let info = payload["info"] as? [String: Any],
                      let usage = info["last_token_usage"] as? [String: Any] {
                turn.input += number(usage["input_tokens"])
                turn.cachedInput += number(usage["cached_input_tokens"])
                turn.output += number(usage["output_tokens"])
                turn.reasoningOutput += number(usage["reasoning_output_tokens"])
                turn.total += number(usage["total_tokens"])
            }
        }
        finish()
    }

    private static func number(_ value: Any?) -> UInt64 {
        guard let number = value as? NSNumber else { return 0 }
        return number.doubleValue > 0 ? number.uint64Value : 0
    }

    static func summarize(_ turns: [TurnTokenUsage]) -> [ModelTokenStatistics] {
        struct Working {
            var commands = 0
            var input: UInt64 = 0, cached: UInt64 = 0, output: UInt64 = 0
            var reasoning: UInt64 = 0, total: UInt64 = 0
            var totals: [UInt64] = []
        }
        var grouped: [String: Working] = [:]
        for turn in turns where !turn.model.isEmpty && turn.total > 0 {
            var item = grouped[turn.model] ?? Working()
            item.commands += 1
            item.input += turn.input
            item.cached += turn.cachedInput
            item.output += turn.output
            item.reasoning += turn.reasoningOutput
            item.total += turn.total
            item.totals.append(turn.total)
            grouped[turn.model] = item
        }
        return grouped.map { model, item in
            let totals = item.totals.sorted()
            return ModelTokenStatistics(
                model: model, commands: item.commands, input: item.input,
                cachedInput: item.cached, output: item.output,
                reasoningOutput: item.reasoning, total: item.total,
                minimum: totals.first ?? 0, median: totals[totals.count / 2],
                maximum: totals.last ?? 0
            )
        }.sorted {
            $0.commands != $1.commands ? $0.commands > $1.commands : $0.model < $1.model
        }
    }

    static func report(statistics: [ModelTokenStatistics], history: [HistoryPoint]) -> String {
        var lines = [
            "Codex command token statistics by model",
            "(all model calls in one command are combined; cached/reasoning are included in input/output)",
            ""
        ]
        if statistics.isEmpty { lines.append("No readable Codex session token records.") }
        for item in statistics {
            lines.append("[\(item.model)]  commands \(item.commands)")
            lines.append("  average total \(format(item.total / UInt64(item.commands))) " +
                         "(input \(format(item.averageInput)), cached \(format(item.averageCachedInput)), " +
                         "output \(format(item.averageOutput)), reasoning \(format(item.averageReasoningOutput)))")
            lines.append("  total distribution  min \(format(item.minimum)) / " +
                         "median \(format(item.median)) / max \(format(item.maximum))")
            lines.append("")
        }
        var five = 0.0, weekly = 0.0
        for (previous, current) in zip(history, history.dropFirst()) {
            five += max(0, previous.fiveHourRemaining - current.fiveHourRemaining)
            weekly += max(0, previous.weeklyRemaining - current.weeklyRemaining)
        }
        lines.append("CSV quota observations")
        if history.count < 2 {
            lines.append("  Not enough CSV samples to compare.")
        } else {
            lines.append(String(format: "  %d samples: 5-hour %.2f%%p, weekly %.2f%%p decrease observed",
                                history.count, five, weekly))
            lines.append("  Quota percentages do not map 1:1 to tokens and are shown for reference only.")
        }
        return lines.joined(separator: "\n")
    }

    static func format(_ value: UInt64) -> String {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        return formatter.string(from: NSNumber(value: value)) ?? String(value)
    }
}
