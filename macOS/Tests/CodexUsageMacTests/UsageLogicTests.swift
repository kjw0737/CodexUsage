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