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

TokenStatisticsDialog::TokenStatisticsDialog(
    const CString& report,
    const std::vector<ModelTokenStatistics>& statistics,
    CWnd* parent)
    : CDialogEx(IDD_TOKEN_STATS, parent), report_(report), statistics_(statistics) {}

BEGIN_MESSAGE_MAP(TokenStatisticsDialog, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_WM_SETTINGCHANGE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL TokenStatisticsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    theme_.Refresh(m_hWnd);
    SetDlgItemText(IDC_TOKEN_STATS_TEXT, report_);
    CRect window; GetWindowRect(&window); minimumSize_ = window.Size();
    CRect client; GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());
    return TRUE;
}

CRect TokenStatisticsDialog::ChartRect() const
{
    CRect client; GetClientRect(&client);
    return CRect(8, 8, (std::max)(8, static_cast<int>(client.right) - 8),
                 (std::max)(120, (std::min)(360, static_cast<int>(client.bottom) - 120)));
}

void TokenStatisticsDialog::DrawChart(CDC& dc, const CRect& chart)
{
    dc.FillSolidRect(chart, theme_.surface);
    dc.Draw3dRect(chart, theme_.border, theme_.border);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(theme_.foreground);

    CRect title(chart.left + 10, chart.top + 5, chart.right - 10, chart.top + 23);
    dc.DrawText(L"Average tokens per command", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (statistics_.empty())
    {
        CRect empty = chart; empty.top += 30;
        dc.SetTextColor(theme_.muted);
        dc.DrawText(L"No token data available", empty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const COLORREF colors[] = { RGB(51, 132, 220), RGB(52, 181, 155),
                                RGB(232, 145, 48), RGB(164, 102, 214) };
    const COLORREF lowColors[] = { RGB(103, 112, 126), RGB(132, 141, 153),
                                   RGB(117, 121, 128), RGB(149, 143, 157) };
    const wchar_t* labels[] = { L"Uncached input", L"Cached input",
                                L"Output", L"Reasoning" };
    const int titleWidth = static_cast<int>(dc.GetTextExtent(L"Average tokens per command").cx);
    int legendX = chart.left + (std::max)(250, titleWidth + 30);
    for (int i = 0; i < 4; ++i)
    {
        CRect swatch(legendX, chart.top + 8, legendX + 10, chart.top + 18);
        dc.FillSolidRect(swatch, colors[i]);
        const int labelWidth = static_cast<int>(dc.GetTextExtent(labels[i]).cx) + 8;
        CRect label(swatch.right + 4, chart.top + 4,
                    swatch.right + 4 + labelWidth, chart.top + 22);
        dc.SetTextColor(theme_.muted);
        dc.DrawText(labels[i], label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        legendX = label.right + 14;
    }

    const int availableRows = (std::max)(1, (chart.Height() - 50) / 56);
    const size_t count = (std::min<size_t>)(statistics_.size(),
                                             static_cast<size_t>((std::min)(6, availableRows)));
    uint64_t inputMaximum = 1, outputMaximum = 1, distributionMaximum = 1;
    int valueWidth = 94;
    for (size_t i = 0; i < count; ++i)
    {
        const auto& item = statistics_[i];
        if (!item.commands) continue;
        const uint64_t commands = static_cast<uint64_t>(item.commands);
        const uint64_t input = item.input / commands;
        const uint64_t output = item.output / commands;
        inputMaximum = (std::max)(inputMaximum, input);
        outputMaximum = (std::max)(outputMaximum, output);
        distributionMaximum = (std::max)(distributionMaximum, item.maximum);
        const int inputWidth = static_cast<int>(dc.GetTextExtent(Count(input)).cx);
        const int outputWidth = static_cast<int>(dc.GetTextExtent(Count(output)).cx);
        valueWidth = (std::max)(valueWidth, (std::max)(inputWidth, outputWidth) + 24);
    }

    const int rowsTop = chart.top + 32;
    const int rowHeight = (std::max)(56, (chart.Height() - 38) / static_cast<int>(count));
    const int labelWidth = (std::min)(155, (std::max)(112, chart.Width() / 6));
    const int barLeft = chart.left + 10 + labelWidth;
    const int barRight = (std::max)(barLeft + 40, static_cast<int>(chart.right) - valueWidth);
    const int barWidth = barRight - barLeft;

    for (size_t i = 0; i < count; ++i)
    {
        const auto& item = statistics_[i];
        if (!item.commands) continue;
        const uint64_t commands = static_cast<uint64_t>(item.commands);
        const uint64_t inputTotal = item.input / commands;
        const uint64_t cached = (std::min)(item.cachedInput / commands, inputTotal);
        const uint64_t uncached = inputTotal - cached;
        const uint64_t outputTotal = item.output / commands;
        const uint64_t reasoning = (std::min)(item.reasoningOutput / commands, outputTotal);
        const uint64_t regularOutput = outputTotal - reasoning;
        const double cacheRate = item.input ? 100.0 * static_cast<double>(item.cachedInput) / item.input : 0.0;
        const bool lowSample = item.commands < 5;
        const COLORREF* palette = lowSample ? lowColors : colors;
        const int top = rowsTop + static_cast<int>(i) * rowHeight;

        CString model;
        model.Format(L"%s (n=%llu)%s", item.model.c_str(),
                     static_cast<unsigned long long>(commands),
                     lowSample ? L"  LOW SAMPLE" : L"");
        CRect modelRect(chart.left + 10, top, barLeft - 6, top + 16);
        dc.SetTextColor(lowSample ? theme_.muted : theme_.foreground);
        dc.DrawText(model, modelRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        CString summary;
        summary.Format(L"Cache %.1f%%  ·  Median %s", cacheRate, Count(item.median).GetString());
        CRect summaryRect(barLeft, top, chart.right - 10, top + 16);
        dc.SetTextColor(lowSample ? theme_.muted : theme_.foreground);
        dc.DrawText(summary, summaryRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        auto drawBar = [&](int y, const wchar_t* name, uint64_t first, uint64_t second,
                           uint64_t total, uint64_t scale, int firstColor, int secondColor)
        {
            CRect labelRect(chart.left + 10, y - 3, barLeft - 7, y + 12);
            dc.SetTextColor(theme_.muted);
            dc.DrawText(name, labelRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            CRect bar(barLeft, y, barRight, y + 10);
            dc.FillSolidRect(bar, theme_.background);
            dc.Draw3dRect(bar, lowSample ? theme_.muted : theme_.border,
                               lowSample ? theme_.muted : theme_.border);
            const int firstEnd = barLeft + static_cast<int>(first * barWidth / scale);
            const int totalEnd = barLeft + static_cast<int>(total * barWidth / scale);
            if (firstEnd > barLeft)
                dc.FillSolidRect(CRect(barLeft + 1, y + 1, (std::min)(firstEnd, barRight), y + 10),
                                 palette[firstColor]);
            if (totalEnd > firstEnd)
                dc.FillSolidRect(CRect((std::max)(firstEnd, barLeft + 1), y + 1,
                                       (std::min)(totalEnd, barRight), y + 10),
                                 palette[secondColor]);
            CRect value(barRight + 8, y - 3, chart.right - 10, y + 12);
            dc.SetTextColor(theme_.muted);
            dc.DrawText(Count(total), value, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        };

        drawBar(top + 18, L"Input", uncached, cached, inputTotal, inputMaximum, 0, 1);
        drawBar(top + 33, L"Output", regularOutput, reasoning, outputTotal, outputMaximum, 2, 3);

        const int rangeY = top + 50;
        CRect rangeLabel(chart.left + 10, rangeY - 7, barLeft - 7, rangeY + 7);
        dc.SetTextColor(theme_.muted);
        dc.DrawText(L"Total range", rangeLabel, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        const int minimumX = barLeft + static_cast<int>(item.minimum * barWidth / distributionMaximum);
        const int medianX = barLeft + static_cast<int>(item.median * barWidth / distributionMaximum);
        const int maximumX = barLeft + static_cast<int>(item.maximum * barWidth / distributionMaximum);
        CPen rangePen(PS_SOLID, 1, lowSample ? theme_.muted : theme_.foreground);
        auto oldPen = dc.SelectObject(&rangePen);
        dc.MoveTo(minimumX, rangeY); dc.LineTo(maximumX, rangeY);
        dc.MoveTo(minimumX, rangeY - 3); dc.LineTo(minimumX, rangeY + 4);
        dc.MoveTo(maximumX, rangeY - 3); dc.LineTo(maximumX, rangeY + 4);
        dc.MoveTo(medianX, rangeY - 6); dc.LineTo(medianX, rangeY + 7);
        dc.SelectObject(oldPen);
    }

    if (statistics_.size() > count)
    {
        CString more; more.Format(L"+%llu models in text details",
            static_cast<unsigned long long>(statistics_.size() - count));
        CRect note(chart.left + 10, chart.bottom - 18, chart.right - 10, chart.bottom - 3);
        dc.SetTextColor(theme_.muted);
        dc.DrawText(more, note, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void TokenStatisticsDialog::OnPaint()
{
    CPaintDC paint(this);
    CRect client; GetClientRect(&client);
    CDC buffer; buffer.CreateCompatibleDC(&paint);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&paint, client.Width(), client.Height());
    auto oldBitmap = buffer.SelectObject(&bitmap);
    buffer.FillSolidRect(client, theme_.background);
    DrawChart(buffer, ChartRect());
    paint.BitBlt(0, 0, client.Width(), client.Height(), &buffer, 0, 0, SRCCOPY);
    buffer.SelectObject(oldBitmap);
}

BOOL TokenStatisticsDialog::OnEraseBkgnd(CDC*) { return TRUE; }

HBRUSH TokenStatisticsDialog::OnCtlColor(CDC* dc, CWnd*, UINT type)
{
    return theme_.Color(dc, type);
}

void TokenStatisticsDialog::OnSettingChange(UINT flags, LPCTSTR section)
{
    CDialogEx::OnSettingChange(flags, section);
    theme_.Refresh(m_hWnd);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void TokenStatisticsDialog::LayoutControls(int width, int height)
{
    if (!GetSafeHwnd()) return;
    const CRect chart = ChartRect();
    const int textTop = chart.bottom + 8;
    if (CWnd* text = GetDlgItem(IDC_TOKEN_STATS_TEXT))
        text->SetWindowPos(nullptr, 8, textTop, (std::max)(0, width - 16),
                           (std::max)(0, height - textTop - 30),
                           SWP_NOZORDER | SWP_NOACTIVATE);
    if (CWnd* close = GetDlgItem(IDOK))
        close->SetWindowPos(nullptr, (std::max)(8, width - 58), (std::max)(8, height - 26),
                            50, 18, SWP_NOZORDER | SWP_NOACTIVATE);
    Invalidate(FALSE);
}

void TokenStatisticsDialog::OnSize(UINT type, int width, int height)
{
    CDialogEx::OnSize(type, width, height);
    if (type != SIZE_MINIMIZED) LayoutControls(width, height);
}

void TokenStatisticsDialog::OnGetMinMaxInfo(MINMAXINFO* info)
{
    CDialogEx::OnGetMinMaxInfo(info);
    if (minimumSize_.cx > 0) info->ptMinTrackSize.x = minimumSize_.cx;
    if (minimumSize_.cy > 0) info->ptMinTrackSize.y = minimumSize_.cy;
}
