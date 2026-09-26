import AppKit
import SwiftUI

struct HistoryView: View {
    let points: [HistoryPoint]
    @State private var range: HistoryRange = .day
    @State private var page = 0

    private var timeline: HistoryTimeline { HistoryTimeline(points: points, range: range) }
    private var currentPage: Int { timeline.pages.contains(page) ? page : (timeline.pages.last ?? 0) }

    var body: some View {
        let line = timeline
        let selected = currentPage
        let interval = line.interval(for: selected)
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                Picker("Range", selection: $range) {
                    ForEach(HistoryRange.allCases) { option in
                        Text(option.title).tag(option)
                    }
                }
                .frame(width: 178)
                .onChange(of: range) { changed in
                    page = HistoryTimeline(points: points, range: changed).pages.last ?? 0
                }
                Button("|<") { page = line.pages.first ?? 0 }
                    .disabled(line.previous(before: selected) == nil)
                Button("<") { page = line.previous(before: selected) ?? selected }
                    .disabled(line.previous(before: selected) == nil)
                Button(">") { page = line.next(after: selected) ?? selected }
                    .disabled(line.next(after: selected) == nil)
                Button(">|") { page = line.pages.last ?? 0 }
                    .disabled(line.next(after: selected) == nil)
                Spacer(minLength: 0)
            }
            HStack(spacing: 18) {
                Label("5 Hour", systemImage: "circle.fill").foregroundStyle(.blue)
                Label("Weekly", systemImage: "circle.fill").foregroundStyle(.orange)
                Spacer()
                Text("\(interval.start.formatted(date: .numeric, time: .shortened)) – \(interval.end.formatted(date: .numeric, time: .shortened))")
                    .foregroundStyle(.secondary)
            }
            .font(.system(size: 11))
            HistoryPlot(points: line.visible(on: selected),
                        start: interval.start, duration: range.seconds)
                .id("\(range.rawValue)-\(selected)")
                .frame(minHeight: 260)
        }
        .padding(16)
        .frame(minWidth: 530, minHeight: 340)
        .background(Color(nsColor: .windowBackgroundColor))
        .onAppear { page = timeline.pages.last ?? 0 }
    }
}

private struct HistoryPlot: View {
    let displayed: [HistoryPoint]
    let start: Date
    let duration: TimeInterval
    @State private var hovered: HistoryPoint?
    @State private var hoverLocation: CGPoint = .zero

    init(points: [HistoryPoint], start: Date, duration: TimeInterval) {
        displayed = Self.simplify(points)
        self.start = start
        self.duration = duration
    }

    private static func simplify(_ points: [HistoryPoint]) -> [HistoryPoint] {
        guard points.count > 4_800 else { return points }
        let bucketSize = max(1, (points.count + 799) / 800)
        var selected: [HistoryPoint] = []
        for lower in stride(from: 0, to: points.count, by: bucketSize) {
            let upper = min(points.count, lower + bucketSize)
            var firstMin = lower, firstMax = lower
            var secondMin = lower, secondMax = lower
            for index in lower..<upper {
                if points[index].fiveHourRemaining < points[firstMin].fiveHourRemaining { firstMin = index }
                if points[index].fiveHourRemaining > points[firstMax].fiveHourRemaining { firstMax = index }
                if points[index].weeklyRemaining < points[secondMin].weeklyRemaining { secondMin = index }
                if points[index].weeklyRemaining > points[secondMax].weeklyRemaining { secondMax = index }
            }
            let indices = Set([lower, upper - 1, firstMin, firstMax, secondMin, secondMax]).sorted()
            selected.append(contentsOf: indices.map { points[$0] })
        }
        return selected
    }

    private func plotRect(_ size: CGSize) -> CGRect {
        CGRect(x: 47, y: 15, width: max(1, size.width - 62),
               height: max(1, size.height - 51))
    }

    private func location(of point: HistoryPoint, remaining: Double,
                          in plot: CGRect) -> CGPoint {
        let progress = point.timestamp.timeIntervalSince(start) / duration
        return CGPoint(x: plot.minX + plot.width * CGFloat(progress),
                       y: plot.maxY - plot.height * CGFloat(remaining) / 100)
    }

    private func updateHover(_ position: CGPoint, size: CGSize) {
        let plot = plotRect(size)
        guard plot.contains(position) else { hovered = nil; return }
        var closest: HistoryPoint?
        var distanceSquared: CGFloat = 10 * 10
        for point in displayed {
            for value in [point.fiveHourRemaining, point.weeklyRemaining] {
                let marker = location(of: point, remaining: value, in: plot)
                let dx = marker.x - position.x
                let dy = marker.y - position.y
                let distance = dx * dx + dy * dy
                if distance <= distanceSquared {
                    closest = point
                    distanceSquared = distance
                }
            }
        }
        hovered = closest
        hoverLocation = position
    }

    var body: some View {
        GeometryReader { geometry in
            Canvas { context, size in
                let plot = plotRect(size)
                context.fill(Path { $0.addRect(plot) }, with: .color(Color(nsColor: .controlBackgroundColor)))
                for percent in stride(from: 0, through: 100, by: 25) {
                    let y = plot.maxY - plot.height * CGFloat(percent) / 100
                    var guide = Path()
                    guide.move(to: CGPoint(x: plot.minX, y: y))
                    guide.addLine(to: CGPoint(x: plot.maxX, y: y))
                    context.stroke(guide, with: .color(.primary.opacity(percent == 0 || percent == 100 ? 0.35 : 0.55)),
                                   style: StrokeStyle(lineWidth: 1,
                                                      dash: percent == 0 || percent == 100 ? [] : [4, 4]))
                    context.draw(Text("\(percent)%").font(.system(size: 10)).foregroundColor(.secondary),
                                 at: CGPoint(x: plot.minX - 6, y: y), anchor: .trailing)
                }
                context.stroke(Path { $0.addRect(plot) }, with: .color(.primary.opacity(0.35)), lineWidth: 1)
                let series: [(Color, KeyPath<HistoryPoint, Double>)] = [
                    (.blue, \.fiveHourRemaining), (.orange, \.weeklyRemaining)
                ]
                for (color, field) in series {
                    var line = Path()
                    for (index, point) in displayed.enumerated() {
                        let marker = location(of: point, remaining: point[keyPath: field], in: plot)
                        if index == 0 { line.move(to: marker) }
                        else { line.addLine(to: marker) }
                    }
                    context.stroke(line, with: .color(color), lineWidth: 2)
                    for point in displayed {
                        let marker = location(of: point, remaining: point[keyPath: field], in: plot)
                        context.fill(Path(ellipseIn: CGRect(x: marker.x - 2.5, y: marker.y - 2.5,
                                                           width: 5, height: 5)), with: .color(color))
                    }
                }
                if displayed.isEmpty {
                    context.draw(Text("No saved usage data").foregroundColor(.secondary),
                                 at: CGPoint(x: plot.midX, y: plot.midY))
                }
                let end = start.addingTimeInterval(duration)
                context.draw(Text(start.formatted(date: .numeric, time: .shortened))
                                .font(.system(size: 10)).foregroundColor(.secondary),
                             at: CGPoint(x: plot.minX, y: plot.maxY + 14), anchor: .leading)
                context.draw(Text(end.formatted(date: .numeric, time: .shortened))
                                .font(.system(size: 10)).foregroundColor(.secondary),
                             at: CGPoint(x: plot.maxX, y: plot.maxY + 14), anchor: .trailing)
            }
            .onContinuousHover { phase in
                switch phase {
                case .active(let position): updateHover(position, size: geometry.size)
                case .ended: hovered = nil
                }
            }
            .overlay(alignment: .topLeading) {
                if let point = hovered {
                    Text("\(point.timestamp.formatted(date: .numeric, time: .standard))\n5 Hour: \(point.fiveHourRemaining.formatted(.number.precision(.fractionLength(2))))%\nWeekly: \(point.weeklyRemaining.formatted(.number.precision(.fractionLength(2))))%")
                        .font(.system(size: 11))
                        .padding(8)
                        .background(.regularMaterial)
                        .clipShape(RoundedRectangle(cornerRadius: 6))
                        .shadow(radius: 4)
                        .offset(x: min(hoverLocation.x + 12, max(0, geometry.size.width - 175)),
                                y: min(hoverLocation.y + 12, max(0, geometry.size.height - 65)))
                        .allowsHitTesting(false)
                }
            }
        }
    }
}
