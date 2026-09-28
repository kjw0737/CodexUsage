#include "pch.h"
#include "UsageClient.h"
#include "UsageAlerts.h"
#include "UsageHistory.h"
#include "TokenStatistics.h"
#include "StartupRegistration.h"
#include <winrt/base.h>
#include <iostream>
#include <stdexcept>
#include <chrono>

void Check(bool value, const char* label)
{
    if (!value) throw std::runtime_error(label);
    std::cout << "PASS: " << label << std::endl;
}
int wmain(int argc, wchar_t** argv)
{
    AfxWinInit(GetModuleHandle(nullptr), nullptr, GetCommandLine(), 0);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    try
    {
        auto value = UsageClient::ParseResult(LR"({"rateLimits":{"primary":{"usedPercent":90}},"rateLimitsByLimitId":{"codex":{"primary":{"usedPercent":8,"windowDurationMins":300,"resetsAt":1900000000},"secondary":{"usedPercent":32,"windowDurationMins":10080}},"other":{"primary":{"usedPercent":100}}},"rateLimitResetCredits":{"availableCount":3,"credits":null}})");
        Check(value.connected && value.primary.remaining == 92 && value.secondary.remaining == 68, "prefer codex multi-bucket; convert used to remaining");
        Check(value.primary.minutes == 300 && value.secondary.minutes == 10080 && value.primary.resetAt == 1900000000, "window duration and reset timestamp");
        Check(value.resetCredits == 3 && !value.expirations.IsEmpty(), "count-only reset credits");
        value = UsageClient::ParseResult(LR"({"rateLimits":{"primary":null,"secondary":{"resetsAt":null,"usedPercent":0}},"rateLimitResetCredits":null})");
        Check(!value.primary.available && value.secondary.available && value.secondary.remaining == 100 && value.secondary.resetAt == 0 && value.resetCredits == -1, "null is unavailable; zero usage is 100 percent remaining");
        value = UsageClient::ParseResult(LR"({"rateLimits":{"primary":{"usedPercent":150},"secondary":{"usedPercent":-1}}})");
        Check(value.primary.remaining == 0 && value.secondary.remaining == 100, "clamp remaining percentages");
        value = UsageClient::ParseResult(LR"({"rateLimits":{"primary":{"usedPercent":7}},"rateLimitsByLimitId":{"other":{"primary":{"usedPercent":5}}}})");
        Check(!value.connected && !value.error.IsEmpty(), "never show unrelated bucket as Codex quota");
        value = UsageClient::ParseResult(LR"({"rateLimits":{},"rateLimitResetCredits":{"availableCount":2,"credits":[{"status":"available","expiresAt":1900000000},{"status":"redeemed","expiresAt":1900000000},{"status":"available","expiresAt":null}]}})");
        Check(value.resetCredits == 2 && value.expirations.Find(L"2030") >= 0 && value.expirations.Find(L"만료일 없음") >= 0, "available credit dates and null expiration");
        bool rejected = false;
        try { UsageClient::ParseResult(L"{broken"); } catch (...) { rejected = true; }
        Check(rejected, "reject malformed JSON");
        UsageAlerts alerts;
        UsageSnapshot quota; quota.connected = true; quota.primary.available = true;
        quota.primary.remaining = 75; quota.primary.resetAt = 1000;
        bool danger = false;
        Check(alerts.Update(quota, 50, 20, danger).IsEmpty(), "first sample establishes baseline");
        UsageAlerts firstSampleAlerts;
        UsageSnapshot firstSample; firstSample.connected = true; firstSample.primary.available = true;
        firstSample.primary.remaining = 40;
        Check(firstSampleAlerts.Update(firstSample, 50, 20, danger).Find(L"Warning") >= 0 && danger, "warning on first sample already below threshold");
        Check(firstSampleAlerts.Update(firstSample, 50, 20, danger).IsEmpty(), "no duplicate initial warning");
        firstSample.primary.remaining = 10;
        Check(firstSampleAlerts.Update(firstSample, 50, 20, danger).Find(L"Alert") >= 0 && danger, "alert after initial warning");
        UsageAlerts initialAlert;
        Check(initialAlert.Update(firstSample, 50, 20, danger).Find(L"Alert") >= 0 && danger, "alert on first sample already below threshold");
        quota.primary.remaining = 50;
        Check(!alerts.Update(quota, 50, 20, danger).IsEmpty() && danger, "warning at exact threshold");
        Check(alerts.Update(quota, 50, 20, danger).IsEmpty(), "no duplicate warning");
        quota.primary.remaining = 20;
        Check(!alerts.Update(quota, 50, 20, danger).IsEmpty() && danger, "alert at exact threshold");
        quota.connected = false; quota.primary.remaining = 100;
        Check(alerts.Update(quota, 50, 20, danger).IsEmpty(), "disconnection does not imply recovery");
        quota.connected = true;
        Check(!alerts.Update(quota, 50, 20, danger).IsEmpty() && !danger, "recovery notification");
        quota.primary.remaining = 70; alerts.Update(quota, 50, 20, danger);
        quota.primary.remaining = 100; quota.primary.resetAt = 2000;
        Check(!alerts.Update(quota, 50, 20, danger).IsEmpty() && !danger, "quota reset recovery within normal band");
        quota.secondary.available = true; quota.secondary.remaining = 70;
        alerts.Update(quota, 50, 20, danger);
        quota.secondary.remaining = 10;
        Check(!alerts.Update(quota, 50, 20, danger).IsEmpty() && danger, "weekly quota tracked independently");
        alerts.Reset();
        Check(alerts.Update(quota, 60, 30, danger).Find(L"Alert") >= 0 && danger, "settings change reevaluates current alert level");
        Check(UsageAlerts::Level(51,50,20) == 0 && UsageAlerts::Level(50,50,20) == 1 && UsageAlerts::Level(20,50,20) == 2, "graph and alerts share thresholds");
        CString testKey; testKey.Format(L"Software\\Citopia\\CodexUsageTests\\Startup-%lu", GetCurrentProcessId());
        StartupRegistration startup(testKey);
        Check(!startup.IsRegistered(), "startup initially absent in isolated test key");
        Check(startup.SetEnabled(true) == ERROR_SUCCESS && startup.IsRegistered(), "startup registration and detection");
        wchar_t command[32768]{}; DWORD commandBytes = sizeof(command);
        RegGetValue(HKEY_CURRENT_USER, testKey, L"CodexUsage", RRF_RT_REG_SZ, nullptr, command, &commandBytes);
        wchar_t executable[32768]{}; GetModuleFileName(nullptr, executable, _countof(executable));
        Check(CString(command) == L"\"" + CString(executable) + L"\"", "startup executable path is quoted exactly");
        HKEY other = nullptr;
        RegOpenKeyEx(HKEY_CURRENT_USER, testKey, 0, KEY_SET_VALUE, &other);
        DWORD marker = 7; RegSetValueEx(other, L"Unrelated", 0, REG_DWORD, reinterpret_cast<BYTE*>(&marker), sizeof(marker)); RegCloseKey(other);
        Check(startup.SetEnabled(false) == ERROR_SUCCESS && !startup.IsRegistered(), "startup removal");
        DWORD markerBytes = sizeof(marker); marker = 0;
        RegGetValue(HKEY_CURRENT_USER, testKey, L"Unrelated", RRF_RT_REG_DWORD, nullptr, &marker, &markerBytes);
        Check(marker == 7 && startup.SetEnabled(false) == ERROR_SUCCESS, "startup removal preserves other entries and is idempotent");
        RegDeleteTree(HKEY_CURRENT_USER, testKey);
        UsageSnapshot historySample;
        historySample.connected = true;
        historySample.updated = time(nullptr);
        historySample.primary.available = true;
        historySample.primary.remaining = 37.25;
        historySample.primary.resetAt = historySample.updated + 5 * 60 * 60;
        historySample.secondary.available = true;
        historySample.secondary.remaining = 88.5;
        Check(UsageHistory::Append(historySample), "append daily CSV sample");
        auto savedHistory = UsageHistory::Read();
        Check(!savedHistory.empty() &&
              savedHistory.back().fiveHour == 37.25 &&
              savedHistory.back().fiveHourReset == historySample.primary.resetAt &&
              savedHistory.back().weekly == 88.5,
              "read daily CSV sample");
        historySample.secondary.available = false;
        Check(!UsageHistory::Append(historySample), "incomplete usage is not recorded");
        std::vector<TurnTokenUsage> tokenTurns = {
            {L"gpt-test", 100, 60, 20, 5, 120},
            {L"gpt-test", 300, 200, 40, 10, 340},
            {L"gpt-other", 50, 0, 10, 0, 60}
        };
        const auto tokenStats = TokenStatistics::Summarize(tokenTurns);
        Check(tokenStats.size() == 2 && tokenStats[0].model == L"gpt-test" &&
              tokenStats[0].commands == 2 && tokenStats[0].total == 460 &&
              tokenStats[0].minimum == 120 && tokenStats[0].median == 340 &&
              tokenStats[0].maximum == 340, "summarize token usage per command and model");        UsageClient client;
        client.Start(L"Z:\\missing-codex.exe");
        auto deadline = GetTickCount64() + 3000;
        bool received = false;
        while (!(received = client.Take(value)) && GetTickCount64() < deadline) Sleep(20);
        Check(received && !value.connected && !value.error.IsEmpty(), "missing executable fails asynchronously");
        if (argc > 1 && wcscmp(argv[1], L"--live") == 0)
        {
            CString exe = UsageClient::FindExecutable();
            Check(!exe.IsEmpty(), "discover installed CLI");
            client.Start(exe);
            Check(!client.Start(exe), "reject concurrent refresh");
            deadline = GetTickCount64() + 30000;
            received = false;
            while (!(received = client.Take(value)) && GetTickCount64() < deadline) Sleep(40);
            Check(received && value.connected, "live initialize and account/rateLimits/read");
            Check(value.primary.available || value.secondary.available, "live quota window available");
            client.Start(exe); Sleep(150);
            auto before = GetTickCount64(); client.Stop();
            Check(GetTickCount64() - before < 2000, "cancel and close owned app-server promptly");
        }
        std::cout << "All tests passed." << std::endl;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << std::endl; return 1; }
    catch (...) { std::cerr << "FAIL: unexpected exception" << std::endl; return 1; }
    winrt::uninit_apartment();
    return 0;
}
