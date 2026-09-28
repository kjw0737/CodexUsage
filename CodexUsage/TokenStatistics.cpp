#include "pch.h"
#include "Resource.h"
#include "TokenStatistics.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <shlobj.h>
#include <winrt/Windows.Data.Json.h>

using namespace winrt::Windows::Data::Json;

namespace
{
    uint64_t Number(const JsonObject& object, const wchar_t* name)
    {
        const auto value = object.TryLookup(name);
        if (!value || value.ValueType() != JsonValueType::Number) return 0;
        const double number = value.GetNumber();
        return number > 0 ? static_cast<uint64_t>(number) : 0;
    }

    void FinishTurn(std::vector<TurnTokenUsage>& turns, TurnTokenUsage& turn)
    {
        if (!turn.model.empty() && turn.total > 0) turns.push_back(turn);
        turn = TurnTokenUsage();
    }

    void ReadFile(const std::filesystem::path& file, std::vector<TurnTokenUsage>& turns)
    {
        std::ifstream input(file, std::ios::binary);
        if (!input) return;
        TurnTokenUsage turn;
        std::string line;
        while (std::getline(input, line))
        {
            const bool context = line.find("\"type\":\"turn_context\"") != std::string::npos;
            const bool tokens = line.find("\"type\":\"token_count\"") != std::string::npos;
            if (!context && !tokens) continue;
            try
            {
                const auto root = JsonObject::Parse(winrt::to_hstring(line));
                const auto payload = root.GetNamedObject(L"payload", nullptr);
                if (!payload) continue;
                if (context)
                {
                    FinishTurn(turns, turn);
                    turn.model = payload.GetNamedString(L"model", L"").c_str();
                    continue;
                }
                if (turn.model.empty()) continue;
                const auto info = payload.GetNamedObject(L"info", nullptr);
                const auto last = info ? info.GetNamedObject(L"last_token_usage", nullptr) : nullptr;
                if (!last) continue;
                turn.input += Number(last, L"input_tokens");
                turn.cachedInput += Number(last, L"cached_input_tokens");
                turn.output += Number(last, L"output_tokens");
                turn.reasoningOutput += Number(last, L"reasoning_output_tokens");
                turn.total += Number(last, L"total_tokens");
            }
            catch (...) { }
        }
        FinishTurn(turns, turn);
    }

    CString Count(uint64_t value)
    {
        CString raw; raw.Format(L"%llu", static_cast<unsigned long long>(value));
        CString result;
        int first = raw.GetLength() % 3;
        if (first == 0) first = 3;
        for (int i = 0; i < raw.GetLength(); ++i)
        {
            if (i > 0 && (i - first) % 3 == 0) result += L",";
            result += raw[i];
        }
        return result;
    }
}

std::filesystem::path TokenStatistics::DefaultSessionsDirectory()
{
    PWSTR profile = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &profile)))
    {
        result = std::filesystem::path(profile) / L".codex" / L"sessions";
        CoTaskMemFree(profile);
    }
    return result;
}

std::vector<TurnTokenUsage> TokenStatistics::Read(const std::filesystem::path& directory)
{
    std::vector<TurnTokenUsage> turns;
    std::error_code error;
    if (directory.empty() || !std::filesystem::exists(directory, error)) return turns;
    for (std::filesystem::recursive_directory_iterator it(directory, error), end;
         it != end && !error; it.increment(error))
    {
        if (it->is_regular_file(error) && it->path().extension() == L".jsonl")
            ReadFile(it->path(), turns);
    }
    return turns;
}

std::vector<ModelTokenStatistics> TokenStatistics::Summarize(const std::vector<TurnTokenUsage>& turns)
{
    struct Working { ModelTokenStatistics value; std::vector<uint64_t> totals; };
    std::map<std::wstring, Working> grouped;
    for (const auto& turn : turns)
    {
        if (turn.model.empty() || turn.total == 0) continue;
        auto& item = grouped[turn.model];
        item.value.model = turn.model;
        ++item.value.commands;
        item.value.input += turn.input; item.value.cachedInput += turn.cachedInput;
        item.value.output += turn.output; item.value.reasoningOutput += turn.reasoningOutput;
        item.value.total += turn.total; item.totals.push_back(turn.total);
    }
    std::vector<ModelTokenStatistics> result;
    for (auto& pair : grouped)
    {
        auto& working = pair.second;
        std::sort(working.totals.begin(), working.totals.end());
        working.value.minimum = working.totals.front();
        working.value.median = working.totals[working.totals.size() / 2];
        working.value.maximum = working.totals.back();
        result.push_back(working.value);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.commands > b.commands || (a.commands == b.commands && a.model < b.model);
    });
    return result;
}

CString TokenStatistics::BuildReport(const std::vector<ModelTokenStatistics>& statistics,
                                     const std::vector<HistoryPoint>& history)
{
    CString report = L"Codex 명령 1회당 모델별 토큰 통계\r\n"
                     L"(한 명령 안의 모든 모델 호출을 합산, cached/reasoning은 각각 input/output에 포함)\r\n\r\n";
    if (statistics.empty()) report += L"읽을 수 있는 Codex 세션 토큰 기록이 없습니다.\r\n";
    for (const auto& item : statistics)
    {
        const uint64_t commands = static_cast<uint64_t>(item.commands);
        CString block; block.Format(L"[%s]  명령 %llu회\r\n", item.model.c_str(),
                                    static_cast<unsigned long long>(commands));
        report += block;
        report += L"  평균 합계 " + Count(item.total / commands) + L"  (입력 " +
                  Count(item.input / commands) + L", 캐시 입력 " + Count(item.cachedInput / commands) +
                  L", 출력 " + Count(item.output / commands) + L", 추론 출력 " +
                  Count(item.reasoningOutput / commands) + L")\r\n";
        report += L"  합계 분포  최소 " + Count(item.minimum) + L" / 중앙값 " +
                  Count(item.median) + L" / 최대 " + Count(item.maximum) + L"\r\n\r\n";
    }
    report += L"CSV 한도 관측\r\n";
    if (history.size() < 2) report += L"  비교 가능한 CSV 측정값이 부족합니다.\r\n";
    else
    {
        double fiveConsumed = 0, weeklyConsumed = 0;
        for (size_t i = 1; i < history.size(); ++i)
        {
            if (history[i].fiveHourReset == history[i - 1].fiveHourReset)
                fiveConsumed += (std::max)(0.0, history[i - 1].fiveHour - history[i].fiveHour);
            weeklyConsumed += (std::max)(0.0, history[i - 1].weekly - history[i].weekly);
        }
        CString csv;
        csv.Format(L"  %llu개 측정점에서 5시간 한도 %.2f%%p, 주간 한도 %.2f%%p 감소 관측\r\n"
                   L"  ※ 한도 비율은 토큰과 1:1 환산되지 않으므로 참고값으로만 표시합니다.\r\n",
                   static_cast<unsigned long long>(history.size()), fiveConsumed, weeklyConsumed);
        report += csv;
    }
    return report;
}

TokenStatisticsDialog::TokenStatisticsDialog(const CString& report, CWnd* parent)
    : CDialogEx(IDD_TOKEN_STATS, parent), report_(report) {}

BEGIN_MESSAGE_MAP(TokenStatisticsDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL TokenStatisticsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetDlgItemText(IDC_TOKEN_STATS_TEXT, report_);
    CRect window; GetWindowRect(&window); minimumSize_ = window.Size();
    return TRUE;
}

void TokenStatisticsDialog::OnSize(UINT type, int width, int height)
{
    CDialogEx::OnSize(type, width, height);
    if (type == SIZE_MINIMIZED || !GetSafeHwnd()) return;
    if (CWnd* text = GetDlgItem(IDC_TOKEN_STATS_TEXT))
        text->SetWindowPos(nullptr, 8, 8, (std::max)(0, width - 16), (std::max)(0, height - 38),
                           SWP_NOZORDER | SWP_NOACTIVATE);
    if (CWnd* close = GetDlgItem(IDOK))
        close->SetWindowPos(nullptr, (std::max)(8, width - 58), (std::max)(8, height - 26),
                            50, 18, SWP_NOZORDER | SWP_NOACTIVATE);
}

void TokenStatisticsDialog::OnGetMinMaxInfo(MINMAXINFO* info)
{
    CDialogEx::OnGetMinMaxInfo(info);
    if (minimumSize_.cx > 0) info->ptMinTrackSize.x = minimumSize_.cx;
    if (minimumSize_.cy > 0) info->ptMinTrackSize.y = minimumSize_.cy;
}
