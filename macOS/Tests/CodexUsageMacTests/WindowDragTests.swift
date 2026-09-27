import AppKit
import XCTest
@testable import CodexUsageMac

final class WindowDragTests: XCTestCase {
    @MainActor
    func testDragSurfacePassesThroughButtonsAndConvertsCoordinates() {
        let parent = NSView(frame: NSRect(x: 0, y: 0, width: 500, height: 400))
        let surface = WindowDragView(frame: NSRect(x: 20, y: 30, width: 360, height: 280))
        parent.addSubview(surface)
        surface.excludedRects = [NSRect(x: 14, y: 14, width: 80, height: 24),
                                 NSRect(x: 240, y: 180, width: 90, height: 24)]
        func hit(_ x: CGFloat, _ y: CGFloat) -> NSView? {
            surface.hitTest(surface.convert(NSPoint(x: x, y: y), to: parent))
        }
        XCTAssertNil(hit(30, 20))
        XCTAssertNil(hit(260, 190))
        XCTAssertTrue(hit(100, 70) === surface)
        XCTAssertTrue(hit(5, 5) === surface)
        XCTAssertNil(hit(-1, 70))
        XCTAssertNil(hit(361, 70))
        surface.excludedRects = [NSRect(x: 90, y: 60, width: 80, height: 24)]
        XCTAssertNil(hit(100, 70))
        XCTAssertTrue(hit(30, 20) === surface)
    }
}
