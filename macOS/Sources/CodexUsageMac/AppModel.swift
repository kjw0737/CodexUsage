import AppKit
import Combine
import Foundation
import ServiceManagement
import UserNotifications

struct WidgetPreferences: Codable, Equatable {
    var opacity = 90
    var interval = 60
    var topmost = false
    var runAtLogin = false
    var startInTray = false
    var saveCsv = false
    var warning = 50
    var alert = 20
    var cliPath = ""
    var guiPath = ""

    init() {}

    // Older saved settings have no saveCsv key; keep the other preferences on upgrade.
    init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        opacity = try values.decodeIfPresent(Int.self, forKey: .opacity) ?? 90
        interval = try values.decodeIfPresent(Int.self, forKey: .interval) ?? 60
        topmost = try values.decodeIfPresent(Bool.self, forKey: .topmost) ?? false
        runAtLogin = try values.decodeIfPresent(Bool.self, forKey: .runAtLogin) ?? false
        startInTray = try values.decodeIfPresent(Bool.self, forKey: .startInTray) ?? false
        saveCsv = try values.decodeIfPresent(Bool.self, forKey: .saveCsv) ?? false
        warning = try values.decodeIfPresent(Int.self, forKey: .warning) ?? 50
        alert = try values.decodeIfPresent(Int.self, forKey: .alert) ?? 20
        cliPath = try values.decodeIfPresent(String.self, forKey: .cliPath) ?? ""
        guiPath = try values.decodeIfPresent(String.self, forKey: .guiPath) ?? ""
    }

    private enum CodingKeys: String, CodingKey {
        case opacity, interval, topmost, runAtLogin, startInTray, saveCsv
        case warning, alert, cliPath, guiPath
    }
}

final class ForegroundNotificationDelegate: NSObject, UNUserNotificationCenterDelegate {
    func userNotificationCenter(_ center: UNUserNotificationCenter,
                                willPresent notification: UNNotification,
                                withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void) {
        completionHandler([.banner, .sound])
    }
}
@MainActor
final class UsageStore: ObservableObject {
    @Published private(set) var snapshot: UsageSnapshot?
    @Published private(set) var errorMessage: String?
    @Published private(set) var historyErrorMessage: String?
    @Published private(set) var preferences: WidgetPreferences
    @Published private(set) var nextRefresh = Date()
    @Published private(set) var currentTime = Date()

    var onUpdate: (() -> Void)?
    private var timer: Timer?
    private var busy = false
    private let alerts = AlertTracker()
    private let history = HistoryStore()
    private var csvWriteFailed = false
    private let notificationDelegate = ForegroundNotificationDelegate()
    private let defaultsKey = "CodexUsageMac.Preferences"

    init() {
        if let data = UserDefaults.standard.data(forKey: defaultsKey),
           let saved = try? JSONDecoder().decode(WidgetPreferences.self, from: data) {
            preferences = saved
        } else {
            preferences = WidgetPreferences()
        }
        UNUserNotificationCenter.current().delegate = notificationDelegate
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    func start() {
        nextRefresh = Date()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tick() }
        }
        refresh()
    }

    func stop() {
        timer?.invalidate()
        timer = nil
    }

    private func tick() {
        currentTime = Date()
        if currentTime >= nextRefresh { refresh() }
    }

    func refresh() {
        guard !busy else { return }
        busy = true
        nextRefresh = Date().addingTimeInterval(Double(preferences.interval))
        guard let executable = CodexClient.findExecutable(override: preferences.cliPath) else {
            errorMessage = UsageError.cliMissing.localizedDescription
            busy = false
            onUpdate?()
            return
        }
        Task {
            do {
                let value = try await Task.detached(priority: .utility) {
                    try CodexClient.fetch(executable: executable)
                }.value
                snapshot = value
                errorMessage = nil
                if preferences.saveCsv,
                   value.short.remaining != nil, value.long.remaining != nil {
                    do {
                        try history.append(value)
                        historyErrorMessage = nil
                        csvWriteFailed = false
                    } catch {
                        historyErrorMessage = "CSV 저장 실패: \(error.localizedDescription)"
                        if !csvWriteFailed {
                            csvWriteFailed = true
                            notify("사용량 CSV를 저장하지 못했습니다. 저장 폴더 권한을 확인하세요.")
                        }
                    }
                }
                let messages = alerts.update(value, warning: preferences.warning, alert: preferences.alert)
                if !messages.isEmpty { notify(messages.joined(separator: "\n")) }
            } catch {
                errorMessage = error.localizedDescription
            }
            busy = false
            onUpdate?()
        }
    }

    func save(_ value: WidgetPreferences) throws {
        let previous = preferences
        if value.runAtLogin != previous.runAtLogin {
            if value.runAtLogin {
                try SMAppService.mainApp.register()
            } else {
                try SMAppService.mainApp.unregister()
            }
        }
        preferences = value
        if !value.saveCsv {
            historyErrorMessage = nil
            csvWriteFailed = false
        }
        if value.warning != previous.warning || value.alert != previous.alert {
            alerts.reset()
        }
        UserDefaults.standard.set(try JSONEncoder().encode(value), forKey: defaultsKey)
        nextRefresh = Date()
        onUpdate?()
        refresh()
    }

    var countdown: Int { max(0, Int(ceil(nextRefresh.timeIntervalSince(currentTime)))) }

    var tooltip: String {
        let short = snapshot?.short.remaining.map { "\(Int($0.rounded()))%" } ?? "N/A"
        let long = snapshot?.long.remaining.map { "\(Int($0.rounded()))%" } ?? "N/A"
        return "CodexUsage\n5 Hour: \(short) · Weekly: \(long)\n\(errorMessage ?? historyErrorMessage ?? "연결됨")"
    }

    private func notify(_ message: String) {
        let content = UNMutableNotificationContent()
        content.title = "Codex 사용량"
        content.body = message
        content.sound = .default
        UNUserNotificationCenter.current().add(
            UNNotificationRequest(identifier: UUID().uuidString, content: content, trigger: nil)
        )
    }
}

enum MacLauncher {
    static func openCLI(path: String) throws {
        guard let executable = CodexClient.findExecutable(override: path) else { throw UsageError.cliMissing }
        let escaped = "'" + executable.replacingOccurrences(of: "'", with: "'\\''") + "'"
        let script = "#!/bin/zsh\nrm -f -- \"$0\"\nexport TERM=xterm-256color\nexec \(escaped)\n"
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("CodexUsage-\(UUID().uuidString).command")
        try script.write(to: url, atomically: true, encoding: .utf8)
        try FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: url.path)
        if !NSWorkspace.shared.open(url) {
            try? FileManager.default.removeItem(at: url)
            throw NSError(domain: "CodexUsageMac", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "Terminal에서 Codex CLI를 열지 못했습니다."])
        }
    }

    static func openGUI(path: String) throws {
        let url: URL?
        if !path.isEmpty {
            let candidate = URL(fileURLWithPath: path)
            url = FileManager.default.fileExists(atPath: candidate.path) ? candidate : nil
        } else {
            url = NSWorkspace.shared.urlForApplication(withBundleIdentifier: "com.openai.codex")
                ?? ["/Applications/Codex.app", NSHomeDirectory() + "/Applications/Codex.app"]
                    .map { URL(fileURLWithPath: $0) }
                    .first { FileManager.default.fileExists(atPath: $0.path) }
        }
        guard let url, NSWorkspace.shared.open(url) else {
            throw NSError(domain: "CodexUsageMac", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "Codex GUI를 찾지 못했습니다. 설정에서 .app 경로를 지정하세요."])
        }
    }
}