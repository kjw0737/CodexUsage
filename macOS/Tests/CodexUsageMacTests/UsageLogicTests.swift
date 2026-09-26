import XCTest
@testable import CodexUsageMac

final class UsageLogicTests: XCTestCase {
    func testMultiBucketPrefersCodexAndConvertsUsedToRemaining() throws {
        let json = """
        {"result":{"rateLimitsByLimitId":{
          "codex":{"primary":{"usedPercent":25,"resetsAt":1800000000,"windowDurationMins":300},
                   "secondary":{"usedPercent":80}},
          "other":{"primary":{"usedPercent":99}}
        },"rateLimitResetCredits":{"availableCount":3,"credits":[
          {"status":"available","expiresAt":1800000000},
          {"status":"used","expiresAt":1800000100}
        ]}}}
        """
        let snapshot = try UsageParser.parse(Data(json.utf8))
        XCTAssertEqual(snapshot.short.remaining, 75)
        XCTAssertEqual(snapshot.long.remaining, 20)
        XCTAssertEqual(snapshot.short.durationMinutes, 300)
        XCTAssertEqual(snapshot.resetCredits, 3)
        XCTAssertEqual(snapshot.expirations.count, 1)
    }

    func testMissingCodexBucketDoesNotUseUnrelatedLimit() {
        let result: [String: Any] = ["rateLimitsByLimitId": ["other": ["primary": ["usedPercent": 90]]]]
        XCTAssertThrowsError(try UsageParser.parseResult(result))
    }

    func testNullAndClamp() throws {
        let result: [String: Any] = ["rateLimits": [
            "primary": ["usedPercent": NSNull()],
            "secondary": ["usedPercent": -40]
        ]]
        let snapshot = try UsageParser.parseResult(result)
        XCTAssertNil(snapshot.short.remaining)
        XCTAssertEqual(snapshot.long.remaining, 100)
    }

    func testSavedPreferencesKeepOldValuesWhenCSVSettingIsMissing() throws {
        let old = Data(#"{"opacity":73,"interval":45,"topmost":true,"warning":40,"alert":15}"#.utf8)
        let preferences = try JSONDecoder().decode(WidgetPreferences.self, from: old)
        XCTAssertEqual(preferences.opacity, 73)
        XCTAssertEqual(preferences.interval, 45)
        XCTAssertTrue(preferences.topmost)
        XCTAssertFalse(preferences.saveCsv)
        let roundTrip = try JSONDecoder().decode(
            WidgetPreferences.self, from: JSONEncoder().encode(preferences)
        )
        XCTAssertEqual(roundTrip, preferences)
    }

    func testHistoryWritesAndReadsDailyCSV() throws {
        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent("CodexUsage-History-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let history = HistoryStore(directory: directory)
        let timestamp = Date(timeIntervalSince1970: 1_800_000_000)
        let snapshot = UsageSnapshot(
            short: QuotaWindow(remaining: 37.25, resetsAt: nil, durationMinutes: 300),
            long: QuotaWindow(remaining: 88.5, resetsAt: nil, durationMinutes: 10_080),
            resetCredits: nil, expirations: [], updatedAt: timestamp
        )
        try history.append(snapshot)
        let files = try FileManager.default.contentsOfDirectory(atPath: directory.path)
        XCTAssertEqual(files.count, 1)
        XCTAssertTrue(files[0].range(of: #"^usage[0-9]{8}\.csv$"#,
                                        options: .regularExpression) != nil)
        let csv = try String(contentsOf: directory.appendingPathComponent(files[0]), encoding: .utf8)
        XCTAssertTrue(csv.hasPrefix("date_time,5_hour_remaining,weekly_remaining\r\n"))
        XCTAssertTrue(csv.contains(",37.25,88.50\r\n"))
        XCTAssertEqual(try history.read(), [
            HistoryPoint(timestamp: timestamp, fiveHourRemaining: 37.25, weeklyRemaining: 88.5)
        ])
    }

    func testHistoryNavigationSkipsEmptyRanges() {
        let day = Calendar.current.startOfDay(for: Date(timeIntervalSince1970: 1_800_000_000))
        func point(_ seconds: TimeInterval) -> HistoryPoint {
            HistoryPoint(timestamp: day.addingTimeInterval(seconds),
                         fiveHourRemaining: 50, weeklyRemaining: 80)
        }
        let timeline = HistoryTimeline(points: [point(60), point(3 * 3_600 + 60)], range: .hour)
        XCTAssertEqual(timeline.pages, [0, 3])
        XCTAssertEqual(timeline.next(after: 0), 3)
        XCTAssertEqual(timeline.previous(before: 3), 0)
        XCTAssertEqual(timeline.visible(on: 1).count, 0)
        XCTAssertEqual(HistoryTimeline(points: timeline.points, range: .day).pages, [0])
    }

    func testAlertTransitionsAndRecovery() {
        let tracker = AlertTracker()
        func snapshot(_ remaining: Double) -> UsageSnapshot {
            UsageSnapshot(
                short: QuotaWindow(remaining: remaining, resetsAt: nil, durationMinutes: 300),
                long: .unavailable, resetCredits: nil, expirations: [], updatedAt: Date()
            )
        }
        XCTAssertTrue(tracker.update(snapshot(60), warning: 50, alert: 20).isEmpty)
        XCTAssertEqual(tracker.update(snapshot(50), warning: 50, alert: 20).count, 1)
        XCTAssertTrue(tracker.update(snapshot(50), warning: 50, alert: 20).isEmpty)
        XCTAssertEqual(tracker.update(snapshot(20), warning: 50, alert: 20).count, 1)
        XCTAssertEqual(tracker.update(snapshot(70), warning: 50, alert: 20).count, 1)
    }
}