#include "pch.h"
#include "Resource.h"
#include "UsageHistory.h"
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <uxtheme.h>

namespace
{
    std::filesystem::path ExecutableDirectory()
    {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    }

    CString DayFile(time_t when)
    {
        tm local{};
        localtime_s(&local, &when);
        CString name;
        name.Format(L"usage%04d%02d%02d.csv", local.tm_year + 1900,
                    local.tm_mon + 1, local.tm_mday);
        return name;
    }

    time_t ParseTime(int y, int month, int day, int hour, int minute, int second)
    {
        tm local{};
        local.tm_year = y - 1900;
        local.tm_mon = month - 1;
        local.tm_mday = day;
        local.tm_hour = hour;
        local.tm_min = minute;
        local.tm_sec = second;
        local.tm_isdst = -1;
        return mktime(&local);
    }

    bool UpgradeHeader(const std::filesystem::path& file)
    {
        std::ifstream input(file, std::ios::binary);
        if (!input) return true;
        const std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
        input.close();
        const std::string oldHeader = "date_time,5_hour_remaining,weekly_remaining\r\n";
        const std::string newHeader = "date_time,5_hour_remaining,weekly_remaining,5_hour_reset_at\r\n";
        if (content.rfind(newHeader, 0) == 0) return true;
        if (content.rfind(oldHeader, 0) != 0) return false;
        const auto temporary = file.wstring() + L".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output << newHeader << content.substr(oldHeader.size());
            if (!output) return false;
        }
        return MoveFileExW(temporary.c_str(), file.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
}

bool UsageHistory::Append(const UsageSnapshot& snapshot)
{
    if (!snapshot.connected || !snapshot.primary.available || !snapshot.secondary.available)
        return false;
    const time_t stamp = snapshot.updated ? snapshot.updated : time(nullptr);
    const auto file = ExecutableDirectory() / static_cast<LPCWSTR>(DayFile(stamp));
    if (!UpgradeHeader(file)) return false;
    HANDLE handle = CreateFileW(file.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    const bool empty = GetFileSizeEx(handle, &size) && size.QuadPart == 0;
    tm local{};
    localtime_s(&local, &stamp);
    char row[192]{};
    sprintf_s(row, "%04d-%02d-%02d %02d:%02d:%02d,%.2f,%.2f,%lld\r\n",
              local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
              local.tm_hour, local.tm_min, local.tm_sec,
              snapshot.primary.remaining, snapshot.secondary.remaining,
              snapshot.primary.resetAt);
    DWORD written = 0;
    const char header[] = "date_time,5_hour_remaining,weekly_remaining,5_hour_reset_at\r\n";
    bool ok = true;
    if (empty) ok = WriteFile(handle, header, sizeof(header) - 1, &written, nullptr)
                    && written == sizeof(header) - 1;
    if (ok) ok = WriteFile(handle, row, static_cast<DWORD>(strlen(row)), &written, nullptr)
                 && written == strlen(row);
    CloseHandle(handle);
    return ok;
}

std::vector<HistoryPoint> UsageHistory::Read()
{
    std::vector<HistoryPoint> points;
    const auto directory = ExecutableDirectory();
    WIN32_FIND_DATAW find{};
    HANDLE search = FindFirstFileW((directory / L"usage????????.csv").c_str(), &find);
    if (search == INVALID_HANDLE_VALUE) return points;
    do
    {
        std::ifstream input(directory / find.cFileName, std::ios::binary);
        std::string line;
        while (std::getline(input, line))
        {
            int y=0, month=0, day=0, h=0, minute=0, second=0;
            double five=0, weekly=0;
            long long fiveHourReset = 0;
            const int fields = sscanf_s(line.c_str(), "%d-%d-%d %d:%d:%d,%lf,%lf,%lld",
                         &y, &month, &day, &h, &minute, &second, &five, &weekly,
                         &fiveHourReset);
            if (fields != 8 && fields != 9)
                continue;
            if (y < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
                h > 23 || minute > 59 || second > 59 ||
                !std::isfinite(five) || !std::isfinite(weekly) ||
                five < 0 || five > 100 || weekly < 0 || weekly > 100)
                continue;
            const time_t when = ParseTime(y, month, day, h, minute, second);
            if (when != -1) points.push_back({when, five, weekly,
                fields == 9 && fiveHourReset > 0 ? static_cast<time_t>(fiveHourReset) : 0});
        }
    } while (FindNextFileW(search, &find));
    FindClose(search);
    std::sort(points.begin(), points.end(),
              [](const HistoryPoint& a, const HistoryPoint& b) { return a.when < b.when; });
    return points;
}

HistoryDialog::HistoryDialog(CWnd* parent) : CDialogEx(IDD_HISTORY, parent) {}

BEGIN_MESSAGE_MAP(HistoryDialog, CDialogEx)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
    ON_WM_SETTINGCHANGE()
    ON_WM_SIZE()
    ON_WM_ENTERSIZEMOVE()
    ON_WM_EXITSIZEMOVE()
    ON_WM_GETMINMAXINFO()
    ON_CBN_SELCHANGE(IDC_HISTORY_RANGE, &HistoryDialog::OnRangeChanged)
    ON_BN_CLICKED(IDC_HISTORY_FIRST, &HistoryDialog::OnFirst)
    ON_BN_CLICKED(IDC_HISTORY_PREV, &HistoryDialog::OnPrevious)
    ON_BN_CLICKED(IDC_HISTORY_NEXT, &HistoryDialog::OnNext)
    ON_BN_CLICKED(IDC_HISTORY_LAST, &HistoryDialog::OnLast)
END_MESSAGE_MAP()

BOOL HistoryDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    ModifyStyle(WS_MINIMIZEBOX, WS_MAXIMIZEBOX | WS_THICKFRAME, SWP_FRAMECHANGED);
    if (CMenu* systemMenu = GetSystemMenu(FALSE))
        systemMenu->DeleteMenu(SC_MINIMIZE, MF_BYCOMMAND);
    SetWindowPos(nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    theme_.Refresh(m_hWnd);
    const UINT ids[] = { IDC_HISTORY_FIRST, IDC_HISTORY_PREV, IDC_HISTORY_NEXT,
                         IDC_HISTORY_LAST, IDOK };
    for (int i = 0; i < 5; ++i) buttons_[i].SubclassDlgItem(ids[i], this);
    auto combo = static_cast<CComboBox*>(GetDlgItem(IDC_HISTORY_RANGE));
    combo->AddString(L"1 hour");
    combo->AddString(L"5 hours");
    combo->AddString(L"1 day");
    combo->AddString(L"7 days");
    combo->AddString(L"30 days");
    combo->SetCurSel(range_);
    SetWindowTheme(combo->m_hWnd, L"", L"");
    pointTooltip_.Create(this, TTS_ALWAYSTIP);
    pointTooltip_.SetMaxTipWidth(300);
    pointTooltip_.SetDelayTime(250);
    CRect chart = PlotRect();
    pointTooltip_.AddTool(this, L" ", chart, 1);
    pointTooltip_.Activate(FALSE);
    points_ = UsageHistory::Read();
    if (!points_.empty())
    {
        tm local{};
        localtime_s(&local, &points_.front().when);
        local.tm_hour = local.tm_min = local.tm_sec = 0;
        local.tm_isdst = -1;
        origin_ = mktime(&local);
        page_ = LastPage();
    }
    CRect window; GetWindowRect(&window);
    minimumSize_ = window.Size();
    LayoutControls();
    UpdateNavigation();
    return TRUE;
}

BOOL HistoryDialog::PreTranslateMessage(MSG* message)
{
    if (pointTooltip_.GetSafeHwnd()) pointTooltip_.RelayEvent(message);
    return CDialogEx::PreTranslateMessage(message);
}

CRect HistoryDialog::PlotRect() const
{
    CRect client;
    GetClientRect(&client);
    return CRect(54, 57, client.right - 20, client.bottom - 40);
}

void HistoryDialog::LayoutControls()
{
    if (!GetSafeHwnd()) return;
    if (CWnd* close = GetDlgItem(IDOK); close && close->GetSafeHwnd())
    {
        CRect rect; close->GetWindowRect(&rect); ScreenToClient(&rect);
        CRect client; GetClientRect(&client);
        close->SetWindowPos(nullptr, client.right - 18 - rect.Width(), rect.top,
                            0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (pointTooltip_.GetSafeHwnd())
        pointTooltip_.SetToolRect(this, 1, PlotRect());
}

time_t HistoryDialog::Duration() const
{
    static const time_t durations[] = { 3600, 5 * 3600, 86400, 7 * 86400, 30 * 86400 };
    return durations[(std::clamp)(range_, 0, 4)];
}

int HistoryDialog::LastPage() const
{
    if (points_.empty()) return 0;
    return static_cast<int>((points_.back().when - origin_) / Duration());
}

void HistoryDialog::UpdateNavigation()
{
    hoveredPoint_ = -1;
    if (pointTooltip_.GetSafeHwnd())
    {
        pointTooltip_.Pop();
        pointTooltip_.Activate(FALSE);
    }
    bool before = false, after = false;
    for (const auto& point : points_)
    {
        const int location = static_cast<int>((point.when - origin_) / Duration());
        before = before || location < page_;
        after = after || location > page_;
    }
    GetDlgItem(IDC_HISTORY_FIRST)->EnableWindow(before);
    GetDlgItem(IDC_HISTORY_PREV)->EnableWindow(before);
    GetDlgItem(IDC_HISTORY_NEXT)->EnableWindow(after);
    GetDlgItem(IDC_HISTORY_LAST)->EnableWindow(after);
    Invalidate(FALSE);
}
void HistoryDialog::OnRangeChanged()
{
    range_ = static_cast<CComboBox*>(GetDlgItem(IDC_HISTORY_RANGE))->GetCurSel();
    page_ = LastPage();
    UpdateNavigation();
}
void HistoryDialog::OnFirst() { page_ = 0; UpdateNavigation(); }
void HistoryDialog::OnPrevious()
{
    int previous = -1;
    for (const auto& point : points_)
    {
        const int location = static_cast<int>((point.when - origin_) / Duration());
        if (location < page_) previous = location;
    }
    if (previous >= 0) page_ = previous;
    UpdateNavigation();
}
void HistoryDialog::OnNext()
{
    for (const auto& point : points_)
    {
        const int location = static_cast<int>((point.when - origin_) / Duration());
        if (location > page_) { page_ = location; break; }
    }
    UpdateNavigation();
}
void HistoryDialog::OnLast() { page_ = LastPage(); UpdateNavigation(); }

void HistoryDialog::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, theme_.background);
    CRect plot = PlotRect();
    dc.FillSolidRect(plot, theme_.surface);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(theme_.muted);
    for (int percent = 0; percent <= 100; percent += 25)
    {
        const int y = plot.bottom - MulDiv(percent, plot.Height(), 100);
        const bool guide = percent == 25 || percent == 50 || percent == 75;
        CPen grid(guide ? PS_DASH : PS_SOLID, 1,
                  guide ? (theme_.dark ? RGB(116, 128, 145) : RGB(164, 175, 190))
                        : theme_.border);
        auto old = dc.SelectObject(&grid);
        dc.MoveTo(plot.left, y); dc.LineTo(plot.right, y);
        dc.SelectObject(old);
        CString label; label.Format(L"%d%%", percent);
        dc.TextOutW(plot.left - 40, y - 8, label);
    }
    dc.Draw3dRect(plot, theme_.border, theme_.border);
    if (points_.empty())
    {
        dc.DrawText(L"No saved usage data", plot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    const time_t start = origin_ + static_cast<time_t>(page_) * Duration();
    const time_t end = start + Duration();
    auto dateLabel = [](time_t value)
    {
        tm local{}; localtime_s(&local, &value);
        CString text;
        text.Format(L"%04d-%02d-%02d %02d:%02d", local.tm_year + 1900,
                    local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
        return text;
    };
    dc.TextOutW(plot.left, plot.bottom + 6, dateLabel(start));
    CString endLabel = dateLabel(end);
    CSize endSize = dc.GetTextExtent(endLabel);
    dc.TextOutW(plot.right - endSize.cx, plot.bottom + 6, endLabel);
    std::set<time_t> savedResets;
    for (size_t i = 0; i < points_.size(); ++i)
    {
        const auto& point = points_[i];
        if (point.fiveHourReset >= start && point.fiveHourReset <= end)
            savedResets.insert(point.fiveHourReset);
        if (i > 0 && points_[i - 1].fiveHour < 100.0 && point.fiveHour >= 100.0 &&
            point.when >= start && point.when <= end)
            savedResets.insert(point.when);
    }
    if (!savedResets.empty())
    {
        const COLORREF resetColor = theme_.dark ? RGB(171, 151, 82) : RGB(150, 116, 18);
        CPen resetPen(PS_DOT, 1, resetColor);
        auto oldResetPen = dc.SelectObject(&resetPen);
        int lastLabelRight = plot.left - 80;
        for (const time_t reset : savedResets)
        {
            const int x = plot.left + static_cast<int>(
                (reset - start) * static_cast<long long>(plot.Width()) / Duration());
            dc.MoveTo(x, plot.top); dc.LineTo(x, plot.bottom);
            tm local{}; localtime_s(&local, &reset);
            CString label; label.Format(L"5h reset %02d:%02d", local.tm_hour, local.tm_min);
            const CSize size = dc.GetTextExtent(label);
            const LONG labelX = (std::min<LONG>)(x + 3, plot.right - size.cx);
            if (labelX > lastLabelRight + 8)
            {
                dc.SetTextColor(resetColor);
                dc.TextOutW(labelX, plot.top + 4, label);
                lastLabelRight = labelX + size.cx;
            }
        }
        dc.SelectObject(oldResetPen);
    }
    const COLORREF colors[] = { RGB(40, 135, 218), RGB(230, 139, 37) };
    const wchar_t* names[] = { L"5 hour", L"Weekly" };
    for (int series = 0; series < 2; ++series)
    {
        CPen pen(PS_SOLID, 2, colors[series]);
        CBrush marker(colors[series]);
        auto old = dc.SelectObject(&pen);
        auto oldBrush = dc.SelectObject(&marker);
        bool hasPrevious = false;
        for (const auto& point : points_)
        {
            if (point.when < start || point.when >= end) continue;
            const int x = plot.left + static_cast<int>(
                (point.when - start) * static_cast<long long>(plot.Width()) / Duration());
            const double value = series == 0 ? point.fiveHour : point.weekly;
            const int y = plot.bottom - static_cast<int>(value * plot.Height() / 100);
            if (hasPrevious) dc.LineTo(x, y); else dc.MoveTo(x, y);
            dc.Ellipse(x - 2, y - 2, x + 3, y + 3);
            dc.MoveTo(x, y);
            hasPrevious = true;
        }
        dc.SelectObject(oldBrush);
        dc.SelectObject(old);
        dc.SetTextColor(colors[series]);
        dc.TextOutW(plot.left + series * 100, plot.top - 22, names[series]);
    }
}

void HistoryDialog::OnMouseMove(UINT flags, CPoint cursor)
{
    int nearest = -1;
    int bestDistanceSquared = 9 * 9;
    const CRect plot = PlotRect();
    if (plot.PtInRect(cursor) && !points_.empty())
    {
        const time_t start = origin_ + static_cast<time_t>(page_) * Duration();
        const time_t end = start + Duration();
        for (size_t index = 0; index < points_.size(); ++index)
        {
            const auto& point = points_[index];
            if (point.when < start || point.when >= end) continue;
            const int x = plot.left + static_cast<int>(
                (point.when - start) * static_cast<long long>(plot.Width()) / Duration());
            for (double value : { point.fiveHour, point.weekly })
            {
                const int y = plot.bottom - static_cast<int>(value * plot.Height() / 100);
                const int dx = cursor.x - x, dy = cursor.y - y;
                const int distanceSquared = dx * dx + dy * dy;
                if (distanceSquared <= bestDistanceSquared)
                {
                    nearest = static_cast<int>(index);
                    bestDistanceSquared = distanceSquared;
                }
            }
        }
    }
    if (nearest != hoveredPoint_)
    {
        hoveredPoint_ = nearest;
        pointTooltip_.Pop();
        if (nearest < 0) pointTooltip_.Activate(FALSE);
        else
        {
            const auto& point = points_[nearest];
            tm local{};
            localtime_s(&local, &point.when);
            CString description;
            description.Format(L"%04d-%02d-%02d %02d:%02d:%02d\n5 Hour: %.2f%%\nWeekly: %.2f%%",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                local.tm_hour, local.tm_min, local.tm_sec,
                point.fiveHour, point.weekly);
            pointTooltip_.UpdateTipText(description, this, 1);
            pointTooltip_.Activate(TRUE);
        }
    }
    if (nearest >= 0)
    {
        MSG current = *AfxGetCurrentMessage();
        pointTooltip_.RelayEvent(&current);
    }
    CDialogEx::OnMouseMove(flags, cursor);
}

BOOL HistoryDialog::OnEraseBkgnd(CDC*) { return TRUE; }
HBRUSH HistoryDialog::OnCtlColor(CDC* dc, CWnd*, UINT type)
{
    return theme_.Color(dc, type);
}
void HistoryDialog::OnDrawItem(int id, LPDRAWITEMSTRUCT item)
{
    if (item->CtlType == ODT_BUTTON) theme_.DrawButton(item);
    else CDialogEx::OnDrawItem(id, item);
}
void HistoryDialog::OnSettingChange(UINT flags, LPCTSTR section)
{
    CDialogEx::OnSettingChange(flags, section);
    theme_.Refresh(m_hWnd);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void HistoryDialog::OnSize(UINT type, int width, int height)
{
    CDialogEx::OnSize(type, width, height);
    if (type == SIZE_MINIMIZED) return;
    LayoutControls();
    if (!sizing_) Invalidate(FALSE);
}
void HistoryDialog::OnEnterSizeMove()
{
    sizing_ = true;
    CDialogEx::OnEnterSizeMove();
}
void HistoryDialog::OnExitSizeMove()
{
    CDialogEx::OnExitSizeMove();
    sizing_ = false;
    LayoutControls();
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void HistoryDialog::OnGetMinMaxInfo(MINMAXINFO* info)
{
    CDialogEx::OnGetMinMaxInfo(info);
    if (minimumSize_.cx > 0 && minimumSize_.cy > 0)
    {
        info->ptMinTrackSize.x = minimumSize_.cx;
        info->ptMinTrackSize.y = minimumSize_.cy;
    }
}
