import Foundation

struct HistoryPoint: Equatable {
    let timestamp: Date
    let fiveHourRemaining: Double
    let weeklyRemaining: Double
}

struct HistoryStore {
    let directory: URL

    init(directory: URL? = nil) {
        self.directory = directory ?? FileManager.default
            .urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("CodexUsage", isDirectory: true)
    }

    private static func formatter() -> DateFormatter {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.calendar = Calendar(identifier: .gregorian)
        formatter.timeZone = .current
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        formatter.isLenient = false
        return formatter
    }

    private static func filename(for date: Date) -> String {
        let formatter = formatter()
        formatter.dateFormat = "'usage'yyyyMMdd'.csv'"
        return formatter.string(from: date)
    }

    func append(_ snapshot: UsageSnapshot) throws {
        guard let fiveHour = snapshot.short.remaining,
              let weekly = snapshot.long.remaining else { return }
        let manager = FileManager.default
        try manager.createDirectory(at: directory, withIntermediateDirectories: true)
        let file = directory.appendingPathComponent(Self.filename(for: snapshot.updatedAt))
        if !manager.fileExists(atPath: file.path) {
            try Data("date_time,5_hour_remaining,weekly_remaining\r\n".utf8)
                .write(to: file, options: .atomic)
        }
        let timestamp = Self.formatter().string(from: snapshot.updatedAt)
        let locale = Locale(identifier: "en_US_POSIX")
        let row = timestamp + ","
            + String(format: "%.2f", locale: locale, fiveHour) + ","
            + String(format: "%.2f", locale: locale, weekly) + "\r\n"
        let handle = try FileHandle(forWritingTo: file)
        defer { try? handle.close() }
        try handle.seekToEnd()
        try handle.write(contentsOf: Data(row.utf8))
    }

    func read() throws -> [HistoryPoint] {
        let manager = FileManager.default
        guard manager.fileExists(atPath: directory.path) else { return [] }
        let files = try manager.contentsOfDirectory(
            at: directory, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]
        ).filter {
            $0.lastPathComponent.range(of: #"^usage[0-9]{8}\.csv$"#,
                                        options: .regularExpression) != nil
        }
        let formatter = Self.formatter()
        var points: [HistoryPoint] = []
        for file in files {
            let text = try String(contentsOf: file, encoding: .utf8)
            for line in text.components(separatedBy: .newlines) {
                let fields = line.split(separator: ",", omittingEmptySubsequences: false)
                guard fields.count == 3,
                      let date = formatter.date(from: String(fields[0])),
                      let fiveHour = Double(fields[1]),
                      let weekly = Double(fields[2]),
                      fiveHour.isFinite, weekly.isFinite,
                      (0...100).contains(fiveHour), (0...100).contains(weekly) else {
                    continue
                }
                points.append(HistoryPoint(timestamp: date,
                                           fiveHourRemaining: fiveHour,
                                           weeklyRemaining: weekly))
            }
        }
        return points.sorted { $0.timestamp < $1.timestamp }
    }
}

enum HistoryRange: Int, CaseIterable, Identifiable {
    case hour = 3_600
    case fiveHours = 18_000
    case day = 86_400
    case week = 604_800
    case thirtyDays = 2_592_000

    var id: Int { rawValue }
    var seconds: TimeInterval { TimeInterval(rawValue) }
    var title: String {
        switch self {
        case .hour: "1 hour"
        case .fiveHours: "5 hours"
        case .day: "1 day"
        case .week: "7 days"
        case .thirtyDays: "30 days"
        }
    }
}

struct HistoryTimeline {
    let points: [HistoryPoint]
    let range: HistoryRange
    let origin: Date

    init(points: [HistoryPoint], range: HistoryRange) {
        let sorted = points.sorted { $0.timestamp < $1.timestamp }
        self.points = sorted
        self.range = range
        self.origin = Calendar.current.startOfDay(for: sorted.first?.timestamp ?? Date())
    }

    func page(of point: HistoryPoint) -> Int {
        Int(floor(point.timestamp.timeIntervalSince(origin) / range.seconds))
    }

    var pages: [Int] {
        Array(Set(points.map { page(of: $0) })).sorted()
    }

    var resetEvents: [Date] {
        guard points.count > 1 else { return [] }
        return zip(points, points.dropFirst()).compactMap { pair in
            let (previous, current) = pair
            return previous.fiveHourRemaining < 100 && current.fiveHourRemaining >= 100
                ? current.timestamp : nil
        }
    }

    func visible(on page: Int) -> [HistoryPoint] {
        points.filter { self.page(of: $0) == page }
    }

    func interval(for page: Int) -> (start: Date, end: Date) {
        let start = origin.addingTimeInterval(Double(page) * range.seconds)
        return (start, start.addingTimeInterval(range.seconds))
    }

    func previous(before page: Int) -> Int? { pages.last { $0 < page } }
    func next(after page: Int) -> Int? { pages.first { $0 > page } }
}
