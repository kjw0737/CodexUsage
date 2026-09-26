import AppKit
import SwiftUI

private let accent = Color(red: 239.0 / 255.0, green: 228.0 / 255.0, blue: 128.0 / 255.0)

struct HoverButton: View {
    let title: String
    let compact: Bool
    let action: () -> Void
    @State private var hovered = false

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: compact ? 12 : 11, weight: .medium))
                .frame(minWidth: compact ? 20 : 68, minHeight: 24)
                .padding(.horizontal, compact ? 2 : 6)
                .background(hovered ? Color.accentColor.opacity(0.25) : Color.primary.opacity(compact ? 0 : 0.08))
                .clipShape(RoundedRectangle(cornerRadius: 4))
                .overlay {
                    if hovered {
                        RoundedRectangle(cornerRadius: 4).stroke(Color.accentColor.opacity(0.7))
                    }
                }
        }
        .buttonStyle(.plain)
        .onHover { hovered = $0 }
    }
}

struct QuotaRow: View {
    let title: String
    let quota: QuotaWindow
    let warning: Int
    let alert: Int
    let now: Date

    private var fill: Color {
        guard let remaining = quota.remaining else { return .gray }
        if remaining <= Double(alert) { return .red }
        if remaining <= Double(warning) { return .orange }
        return .green
    }

    private var resetText: String {
        guard let date = quota.resetsAt else { return "Reset: N/A" }
        let seconds = max(0, Int(date.timeIntervalSince(now)))
        let days = seconds / 86_400
        let hours = (seconds % 86_400) / 3_600
        let minutes = (seconds % 3_600) / 60
        if days > 0 { return "Reset in \(days)d \(hours)h" }
        if hours > 0 { return "Reset in \(hours)h \(minutes)m" }
        return "Reset in \(minutes)m \(seconds % 60)s"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                Text(title).frame(width: 54, alignment: .leading)
                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        Rectangle().fill(Color.primary.opacity(0.18))
                        Rectangle().fill(fill).frame(width: geometry.size.width * CGFloat((quota.remaining ?? 0) / 100))
                    }
                }
                .frame(height: 14)
                Text(quota.remaining.map { "\(Int($0.rounded()))%" } ?? "N/A")
                    .frame(width: 36, alignment: .trailing)
            }
            Text(resetText)
                .font(.system(size: 10))
                .foregroundStyle(.secondary)
                .padding(.leading, 64)
        }
        .font(.system(size: 11))
    }
}

struct WidgetView: View {
    @ObservedObject var store: UsageStore
    let onCLI: () -> Void
    let onGUI: () -> Void
    let onSettings: () -> Void
    let onMinimize: () -> Void
    let onClose: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 11) {
            HStack(spacing: 6) {
                HoverButton(title: "Codex CLI", compact: false, action: onCLI)
                HoverButton(title: "Codex GUI", compact: false, action: onGUI)
                HoverButton(title: "Settings", compact: false, action: onSettings)
                Spacer(minLength: 0)
                HoverButton(title: "−", compact: true, action: onMinimize)
                HoverButton(title: "×", compact: true, action: onClose)
            }
            Text("CODEX USAGE")
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(accent)
            QuotaRow(title: "5 Hour", quota: store.snapshot?.short ?? .unavailable,
                     warning: store.preferences.warning, alert: store.preferences.alert,
                     now: store.currentTime)
            QuotaRow(title: "Weekly", quota: store.snapshot?.long ?? .unavailable,
                     warning: store.preferences.warning, alert: store.preferences.alert,
                     now: store.currentTime)
            HStack {
                Text("Resets × \(store.snapshot?.resetCredits.map(String.init) ?? "N/A")")
                Spacer()
                HoverButton(title: "Refresh", compact: false) { store.refresh() }
            }
            .font(.system(size: 11))
            Text("Expires: " + (store.snapshot?.expirations.isEmpty == false
                               ? store.snapshot!.expirations.map { $0.formatted(date: .numeric, time: .omitted) }.joined(separator: ", ")
                               : "N/A"))
                .font(.system(size: 10))
                .lineLimit(1)
            Spacer(minLength: 0)
            HStack(spacing: 4) {
                Text(store.errorMessage ?? "Codex: connected")
                    .foregroundStyle(store.errorMessage == nil ? Color.green : Color.red)
                    .lineLimit(1)
                Spacer(minLength: 2)
                Text("Updated: " + (store.snapshot?.updatedAt.formatted(date: .omitted, time: .standard) ?? "--:--:--") +
                     " (\(store.countdown)s)")
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
            .font(.system(size: 9))
        }
        .padding(14)
        .frame(width: 360, height: 280)
        .background(Color(nsColor: .windowBackgroundColor))
        .clipShape(RoundedRectangle(cornerRadius: 9))
        .overlay(RoundedRectangle(cornerRadius: 9).stroke(Color.primary.opacity(0.13)))
    }
}

struct SettingsView: View {
    @State var draft: WidgetPreferences
    let onSave: (WidgetPreferences) -> Void
    let onCancel: () -> Void
    @State private var validation: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("CodexUsage Settings").font(.headline)
            numberRow("Opacity (%)", value: $draft.opacity)
            numberRow("Refresh interval (seconds)", value: $draft.interval)
            numberRow("Warning remaining (%)", value: $draft.warning)
            numberRow("Alert remaining (%)", value: $draft.alert)
            Toggle("Always on top", isOn: $draft.topmost)
            Toggle("Run at login", isOn: $draft.runAtLogin)
            Toggle("Start in menu bar", isOn: $draft.startInTray)
            Text("Codex CLI path (blank: auto-detect)").font(.caption)
            TextField("/opt/homebrew/bin/codex", text: $draft.cliPath)
            Text("Codex GUI .app path (blank: auto-detect)").font(.caption)
            TextField("/Applications/Codex.app", text: $draft.guiPath)
            if let validation {
                Text(validation).font(.caption).foregroundStyle(.red)
            }
            HStack {
                Spacer()
                Button("Cancel", action: onCancel)
                Button("Save") {
                    guard (20...100).contains(draft.opacity),
                          (10...3600).contains(draft.interval),
                          (1...100).contains(draft.warning),
                          (0..<draft.warning).contains(draft.alert) else {
                        validation = "범위: 투명도 20~100, 주기 10~3600, 0 ≤ Alert < Warning ≤ 100"
                        return
                    }
                    onSave(draft)
                }
                .keyboardShortcut(.defaultAction)
            }
        }
        .padding(18)
        .frame(width: 410)
    }

    private func numberRow(_ label: String, value: Binding<Int>) -> some View {
        HStack {
            Text(label)
            Spacer()
            TextField(label, value: value, format: .number)
                .frame(width: 76)
                .multilineTextAlignment(.trailing)
        }
    }
}

final class WidgetWindow: NSWindow {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let store = UsageStore()
    private var window: WidgetWindow!
    private var settingsWindow: NSWindow?
    private var statusItem: NSStatusItem!

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        let view = WidgetView(
            store: store,
            onCLI: { [weak self] in self?.openCLI() },
            onGUI: { [weak self] in self?.openGUI() },
            onSettings: { [weak self] in self?.showSettings() },
            onMinimize: { [weak self] in self?.hideWindow() },
            onClose: { NSApp.terminate(nil) }
        )
        window = WidgetWindow(contentRect: NSRect(x: 0, y: 0, width: 360, height: 280),
                              styleMask: [.borderless], backing: .buffered, defer: false)
        window.contentView = NSHostingView(rootView: view)
        window.backgroundColor = .clear
        window.isOpaque = false
        window.hasShadow = true
        window.isMovableByWindowBackground = true
        window.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        window.center()
        updateWindow()

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let icon = Bundle.main.resourceURL?.appendingPathComponent("CodexUsage.png"),
           let image = NSImage(contentsOf: icon) {
            image.size = NSSize(width: 19, height: 19)
            image.isTemplate = false
            statusItem.button?.image = image
        } else {
            statusItem.button?.image = NSImage(systemSymbolName: "chart.bar.fill", accessibilityDescription: "CodexUsage")
        }
        statusItem.button?.target = self
        statusItem.button?.action = #selector(statusClicked)
        statusItem.button?.sendAction(on: [.leftMouseUp, .rightMouseUp])
        store.onUpdate = { [weak self] in self?.statusItem.button?.toolTip = self?.store.tooltip }
        statusItem.button?.toolTip = store.tooltip

        if !store.preferences.startInTray { showWindow() }
        store.start()
    }

    func applicationWillTerminate(_ notification: Notification) { store.stop() }

    private func updateWindow() {
        window.level = store.preferences.topmost ? .floating : .normal
        window.alphaValue = CGFloat(store.preferences.opacity) / 100
    }

    private func showWindow() {
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func hideWindow() { window.orderOut(nil) }

    @objc private func statusClicked() {
        if NSApp.currentEvent?.type == .rightMouseUp {
            let menu = NSMenu()
            menu.addItem(withTitle: "Show CodexUsage", action: #selector(showFromMenu), keyEquivalent: "")
            menu.addItem(withTitle: "Refresh", action: #selector(refreshFromMenu), keyEquivalent: "")
            menu.addItem(.separator())
            menu.addItem(withTitle: "Quit", action: #selector(quitFromMenu), keyEquivalent: "")
            for item in menu.items { item.target = self }
            if let button = statusItem.button {
                menu.popUp(positioning: nil, at: NSPoint(x: 0, y: button.bounds.height), in: button)
            }
        } else {
            window.isVisible ? hideWindow() : showWindow()
        }
    }

    @objc private func showFromMenu() { showWindow() }
    @objc private func refreshFromMenu() { store.refresh() }
    @objc private func quitFromMenu() { NSApp.terminate(nil) }

    private func showSettings() {
        if let settingsWindow {
            settingsWindow.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }
        let settings = SettingsView(
            draft: store.preferences,
            onSave: { [weak self] value in
                guard let self else { return }
                do {
                    try self.store.save(value)
                    self.updateWindow()
                    self.settingsWindow?.close()
                    self.settingsWindow = nil
                } catch {
                    self.showError(error.localizedDescription)
                }
            },
            onCancel: { [weak self] in
                self?.settingsWindow?.close()
                self?.settingsWindow = nil
            }
        )
        let panel = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 410, height: 455),
                             styleMask: [.titled, .closable], backing: .buffered, defer: false)
        panel.title = "CodexUsage Settings"
        panel.contentView = NSHostingView(rootView: settings)
        panel.center()
        panel.isReleasedWhenClosed = false
        settingsWindow = panel
        panel.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func openCLI() {
        do { try MacLauncher.openCLI(path: store.preferences.cliPath) }
        catch { showError(error.localizedDescription) }
    }

    private func openGUI() {
        do { try MacLauncher.openGUI(path: store.preferences.guiPath) }
        catch { showError(error.localizedDescription) }
    }

    private func showError(_ message: String) {
        let alert = NSAlert()
        alert.messageText = "CodexUsage"
        alert.informativeText = message
        alert.runModal()
    }
}

@main
enum CodexUsageMacMain {
    @MainActor static func main() {
        let app = NSApplication.shared
        let delegate = AppDelegate()
        app.delegate = delegate
        app.run()
    }
}