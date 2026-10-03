import Foundation
import SwiftUI

struct TokenStatisticsView: View {
    let statistics: [ModelTokenStatistics]
    let history: [HistoryPoint]

    private var shown: [ModelTokenStatistics] { Array(statistics.prefix(6)) }
    private var inputMaximum: UInt64 { max(1, shown.map(\.averageInput).max() ?? 1) }
    private var outputMaximum: UInt64 { max(1, shown.map(\.averageOutput).max() ?? 1) }
    private var distributionMaximum: UInt64 { max(1, shown.map(\.maximum).max() ?? 1) }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 14) {
                Text("Average tokens per command").font(.headline)
                legend(.blue, "Uncached input")
                legend(.teal, "Cached input")
                legend(.orange, "Output")
                legend(.purple, "Reasoning")
                Spacer(minLength: 0)
            }
            if shown.isEmpty {
                VStack(spacing: 8) {
                    Image(systemName: "chart.bar.xaxis").font(.largeTitle)
                    Text("No token data").foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, minHeight: 180)
            } else {
                VStack(spacing: 10) {
                    ForEach(shown) { item in
                        TokenModelChart(item: item, inputMaximum: inputMaximum,
                                        outputMaximum: outputMaximum,
                                        distributionMaximum: distributionMaximum)
                    }
                }
                if statistics.count > shown.count {
                    Text("+\(statistics.count - shown.count) models in text details")
                        .font(.caption).foregroundStyle(.secondary)
                        .frame(maxWidth: .infinity, alignment: .trailing)
                }
            }
            Divider()
            ScrollView([.vertical, .horizontal]) {
                Text(TokenStatisticsStore.report(statistics: statistics, history: history))
                    .font(.system(size: 11, design: .monospaced))
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    .padding(8)
            }
            .background(Color(nsColor: .textBackgroundColor))
            .overlay(RoundedRectangle(cornerRadius: 4).stroke(.secondary.opacity(0.35)))
        }
        .padding(14)
        .frame(minWidth: 760, minHeight: 620)
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private func legend(_ color: Color, _ text: String) -> some View {
        HStack(spacing: 4) {
            Rectangle().fill(color).frame(width: 10, height: 10)
            Text(text).lineLimit(1).fixedSize()
        }.font(.caption)
    }
}

private struct TokenModelChart: View {
    let item: ModelTokenStatistics
    let inputMaximum: UInt64
    let outputMaximum: UInt64
    let distributionMaximum: UInt64

    private var lowSample: Bool { item.commands < 5 }
    private var cached: UInt64 { min(item.averageCachedInput, item.averageInput) }
    private var uncached: UInt64 { item.averageInput - cached }
    private var reasoning: UInt64 { min(item.averageReasoningOutput, item.averageOutput) }
    private var regularOutput: UInt64 { item.averageOutput - reasoning }

    var body: some View {
        VStack(spacing: 3) {
            HStack {
                Text("\(item.model) (n=\(item.commands))")
                    .fontWeight(.semibold)
                if lowSample {
                    Text("LOW SAMPLE").font(.caption2).foregroundStyle(.secondary)
                }
                Spacer()
                Text(String(format: "Cache %.1f%%  ·  Median %@",
                            item.cacheRate, TokenStatisticsStore.format(item.median)))
                    .font(.caption).foregroundStyle(.secondary)
            }
            TokenStackedBar(label: "Input", first: uncached, second: cached,
                            total: item.averageInput, maximum: inputMaximum,
                            firstColor: .blue, secondColor: .teal, muted: lowSample)
            TokenStackedBar(label: "Output", first: regularOutput, second: reasoning,
                            total: item.averageOutput, maximum: outputMaximum,
                            firstColor: .orange, secondColor: .purple, muted: lowSample)
            HStack(spacing: 8) {
                Text("Total range").font(.caption).foregroundStyle(.secondary)
                    .frame(width: 78, alignment: .trailing)
                TokenRangeWhisker(minimum: item.minimum, median: item.median,
                                  maximum: item.maximum, scale: distributionMaximum,
                                  muted: lowSample)
                Text("").frame(width: 90)
            }
        }
        .saturation(lowSample ? 0 : 1)
        .opacity(lowSample ? 0.7 : 1)
    }
}

private struct TokenStackedBar: View {
    let label: String
    let first: UInt64
    let second: UInt64
    let total: UInt64
    let maximum: UInt64
    let firstColor: Color
    let secondColor: Color
    let muted: Bool

    var body: some View {
        HStack(spacing: 8) {
            Text(label).font(.caption).foregroundStyle(.secondary)
                .frame(width: 78, alignment: .trailing)
            GeometryReader { geometry in
                let scale = max(1, maximum)
                let firstWidth = geometry.size.width * CGFloat(Double(first) / Double(scale))
                let secondWidth = geometry.size.width * CGFloat(Double(second) / Double(scale))
                ZStack(alignment: .leading) {
                    Rectangle().fill(Color.primary.opacity(0.10))
                    HStack(spacing: 0) {
                        Rectangle().fill(firstColor).frame(width: firstWidth)
                        Rectangle().fill(secondColor).frame(width: secondWidth)
                        Spacer(minLength: 0)
                    }
                }
                .overlay(Rectangle().stroke(muted ? Color.secondary : Color.primary.opacity(0.25),
                                            style: StrokeStyle(lineWidth: 1, dash: muted ? [3, 2] : [])))
            }
            .frame(height: 11)
            Text(TokenStatisticsStore.format(total))
                .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                .frame(width: 90, alignment: .trailing)
        }
    }
}

private struct TokenRangeWhisker: View {
    let minimum: UInt64
    let median: UInt64
    let maximum: UInt64
    let scale: UInt64
    let muted: Bool

    var body: some View {
        GeometryReader { geometry in
            let width = geometry.size.width
            let divisor = Double(max(1, scale))
            let minX = width * CGFloat(Double(minimum) / divisor)
            let medianX = width * CGFloat(Double(median) / divisor)
            let maxX = width * CGFloat(Double(maximum) / divisor)
            Canvas { context, _ in
                var path = Path()
                path.move(to: CGPoint(x: minX, y: 6))
                path.addLine(to: CGPoint(x: maxX, y: 6))
                path.move(to: CGPoint(x: minX, y: 3))
                path.addLine(to: CGPoint(x: minX, y: 9))
                path.move(to: CGPoint(x: maxX, y: 3))
                path.addLine(to: CGPoint(x: maxX, y: 9))
                path.move(to: CGPoint(x: medianX, y: 0))
                path.addLine(to: CGPoint(x: medianX, y: 12))
                context.stroke(path, with: .color(muted ? .secondary : .primary), lineWidth: 1)
            }
        }
        .frame(height: 12)
    }
}
