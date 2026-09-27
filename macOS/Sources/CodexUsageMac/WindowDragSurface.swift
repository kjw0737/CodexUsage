import AppKit
import SwiftUI

struct WidgetButtonBoundsKey: PreferenceKey {
    static var defaultValue: [Anchor<CGRect>] { [] }

    static func reduce(value: inout [Anchor<CGRect>], nextValue: () -> [Anchor<CGRect>]) {
        value.append(contentsOf: nextValue())
    }
}

/// Routes button events through to SwiftUI and uses native window dragging elsewhere.
struct WindowDragSurface: NSViewRepresentable {
    let excludedRects: [CGRect]

    func makeNSView(context: Context) -> WindowDragView {
        WindowDragView()
    }

    func updateNSView(_ view: WindowDragView, context: Context) {
        view.excludedRects = excludedRects
    }
}

final class WindowDragView: NSView {
    var excludedRects: [CGRect] = []

    override var isFlipped: Bool { true }
    override var mouseDownCanMoveWindow: Bool { false }

    override func hitTest(_ point: NSPoint) -> NSView? {
        let localPoint = convert(point, from: superview)
        guard bounds.contains(localPoint),
              !excludedRects.contains(where: { $0.contains(localPoint) }) else { return nil }
        return self
    }

    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func mouseDown(with event: NSEvent) {
        window?.performDrag(with: event)
    }
}
