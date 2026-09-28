#include "pch.h"
#include "Resource.h"
#include "CodexUsageDlg.h"
#include "CodexLauncher.h"
#include "UsageHistory.h"
#include "AboutDialog.h"
#include <ctime>

namespace { constexpr UINT TrayMessage = WM_APP + 10; UINT TaskbarCreated = RegisterWindowMessage(L"TaskbarCreated"); }
namespace
{
    CString Countdown(const UsageWindow& w)
    {
        if (!w.available || !w.resetAt) return L"Reset time unavailable";
        auto s = w.resetAt - static_cast<long long>(time(nullptr));
        if (s <= 0) return L"Reset due · awaiting server";
        CString text;
        if (s >= 86400) text.Format(L"Reset in %lldd %lldh", s / 86400, s % 86400 / 3600);
        else text.Format(L"Reset in %02lld:%02lld:%02lld", s / 3600, s % 3600 / 60, s % 60);
        return text;
    }
    CString Label(const UsageWindow& w, LPCWSTR fallback)
    {
        if (!w.minutes) return CString(fallback);
        if (w.minutes == 10080) return L"Weekly";
        CString label;
        if (w.minutes % 60 == 0) label.Format(L"%lld Hour", w.minutes / 60);
        else label.Format(L"%lld Min", w.minutes);
        return label;
    }
}
CCodexUsageDlg::CCodexUsageDlg(CWnd* parent) : CDialogEx(IDD_CODEXUSAGE_DIALOG, parent) {}
BEGIN_MESSAGE_MAP(CCodexUsageDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETTINGCHANGE()
    ON_WM_DRAWITEM()
    ON_WM_SYSCOMMAND()
    ON_WM_WINDOWPOSCHANGING()
    ON_WM_DESTROY()
    ON_MESSAGE(TrayMessage, &CCodexUsageDlg::OnTray)
    ON_REGISTERED_MESSAGE(TaskbarCreated, &CCodexUsageDlg::OnTaskbarCreated)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CMD_CODEX, IDC_HISTORY, &CCodexUsageDlg::OnAction)
END_MESSAGE_MAP()
CRect CCodexUsageDlg::Rect(int x, int y, int width, int height) const
{
    CRect client; GetClientRect(&client);
    return CRect(MulDiv(x, client.Width(), 360), MulDiv(y, client.Height(), 280),
        MulDiv(x + width, client.Width(), 360), MulDiv(y + height, client.Height(), 280));
}
BOOL CCodexUsageDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    ModifyStyle(WS_CAPTION | WS_THICKFRAME, 0, SWP_FRAMECHANGED);
    SetWindowPos(nullptr, 0, 0, 360, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RestoreWindowPosition();
    SetWindowText(L"Codex Usage");
    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
    settings_.Load(); theme_.Refresh(m_hWnd);
    LOGFONT font{}; font.lfHeight = -12; wcscpy_s(font.lfFaceName, L"Segoe UI");
    font_.CreateFontIndirect(&font); font.lfHeight = -16; font.lfWeight = FW_SEMIBOLD;
    titleFont_.CreateFontIndirect(&font);
    const wchar_t* labels[] = { L"Codex CLI", L"Codex GUI", L"Settings", L"−", L"×", L"Refresh", L"" };
    const CRect rectangles[] = { Rect(16,10,82,28), Rect(104,10,82,28), Rect(192,10,76,28),
        Rect(296,10,24,28), Rect(322,10,24,28), Rect(258,199,82,28), Rect(225,199,28,28) };
    for (int i = 0; i < 7; ++i)
    {
        buttons_[i].Create(labels[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            rectangles[i], this, IDC_CMD_CODEX + i);
        buttons_[i].SetFont(&font_);
    }
    tooltip_.Create(this, TTS_ALWAYSTIP); tooltip_.SetMaxTipWidth(650);
    tooltip_.SetDelayTime(TTDT_INITIAL, 250);
    tooltip_.SetDelayTime(TTDT_AUTOPOP, 15000);
    CRect info = Rect(18, 202, 230, 26); tooltip_.AddTool(this, L"리셋 크레딧 만료일", info, 1);
    CRect status = Rect(18, 238, 180, 26); tooltip_.AddTool(this, L"Codex 연결 상태", status, 2);
    tooltip_.AddTool(&buttons_[3], L"트레이로 숨기기"); tooltip_.AddTool(&buttons_[4], L"종료");
    tooltip_.AddTool(&buttons_[6], L"저장된 사용량 그래프");
    const bool trayReady = tray_.Add(m_hWnd, AfxGetApp()->LoadIcon(IDR_MAINFRAME), TrayMessage);
    startupHidden_ = settings_.startInTray && trayReady && CString(AfxGetApp()->m_lpCmdLine).Find(L"--show-from-notification") < 0;
    ApplySettings(); SetTimer(1, 250, nullptr);
    if (CString(AfxGetApp()->m_lpCmdLine).Find(L"--test-notification") >= 0)
        tray_.Notify(L"알림 센터 보관 테스트입니다. 배너가 사라진 뒤 Win+N으로 확인하세요.", false);
    Refresh(); return TRUE;
}
BOOL CCodexUsageDlg::PreTranslateMessage(MSG* msg)
{
    if (tooltip_.GetSafeHwnd()) tooltip_.RelayEvent(msg);
    return CDialogEx::PreTranslateMessage(msg);
}
void CCodexUsageDlg::ApplySettings()
{
    ModifyStyleEx(0, WS_EX_LAYERED);
    SetLayeredWindowAttributes(0, static_cast<BYTE>(settings_.opacity * 255 / 100), LWA_ALPHA);
    SetWindowPos(settings_.topmost ? &wndTopMost : &wndNoTopMost, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    nextRefresh_ = GetTickCount64() + static_cast<ULONGLONG>(settings_.interval) * 1000;
}
void CCodexUsageDlg::Refresh()
{
    if (client_.Busy()) return;
    client_.Start(settings_.cliPath.IsEmpty() ? UsageClient::FindExecutable() : settings_.cliPath);
    buttons_[5].EnableWindow(FALSE); Invalidate(FALSE);
}
void CCodexUsageDlg::OnTimer(UINT_PTR id)
{
    if (id == 1)
    {
        UsageSnapshot value;
        if (client_.Take(value))
        {
            CString notice;
            bool warning = false;
            stale_ = !value.connected; error_ = value.error;
            if (value.connected)
            {
                notice = alerts_.Update(value, settings_.warning, settings_.alert, warning);
                snapshot_ = value;
                if (settings_.saveCsv && value.primary.available && value.secondary.available &&
                    !UsageHistory::Append(value) && !csvErrorShown_)
                {
                    csvErrorShown_ = true;
                    AfxMessageBox(L"사용량 CSV를 프로그램 실행 폴더에 저장하지 못했습니다. 폴더 쓰기 권한을 확인하세요.");
                }
            }
            tray_.Update(snapshot_, stale_);
            tray_.Notify(notice, warning);
            tooltip_.UpdateTipText(snapshot_.expirations.IsEmpty() ? L"상세 정보 없음" : snapshot_.expirations, this, 1);
            CString statusTip;
            if (client_.Busy()) statusTip = L"Codex: refreshing…";
            else if (stale_)
            {
                statusTip = L"Codex: disconnected";
                if (error_.IsEmpty()) statusTip += L"\n원인 정보가 없습니다.";
                else { statusTip += L"\n"; statusTip += error_; }
            }
            else statusTip = L"Codex: connected\nCodex CLI 로그인 계정의 남은 사용량을 표시합니다.";
            tooltip_.UpdateTipText(statusTip, this, 2);
            buttons_[5].EnableWindow(TRUE);
            nextRefresh_ = GetTickCount64() + static_cast<ULONGLONG>(settings_.interval) * 1000;
        }
        if (GetTickCount64() >= nextRefresh_ && !client_.Busy()) Refresh();
        Invalidate(FALSE);
    }
    CDialogEx::OnTimer(id);
}
void CCodexUsageDlg::OnPaint()
{
    CPaintDC paint(this); CRect bounds; GetClientRect(&bounds);
    CDC dc; dc.CreateCompatibleDC(&paint);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&paint, bounds.Width(), bounds.Height());
    auto oldBitmap = dc.SelectObject(&bitmap);
    dc.FillSolidRect(bounds, theme_.background); dc.Draw3dRect(bounds, theme_.border, theme_.border);
    dc.SetBkMode(TRANSPARENT); auto oldFont = dc.SelectObject(&font_);
    auto text = [&](const CString& value, CRect rect, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS)
    { dc.SetTextColor(color); dc.DrawText(value, rect, flags); };
    dc.SelectObject(&titleFont_); text(L"CODEX USAGE", Rect(18,46,200,24), RGB(245,200,35));
    dc.SelectObject(&font_); text(L"Remaining", Rect(258,49,82,20), theme_.muted, DT_RIGHT | DT_SINGLELINE);
    const UsageWindow windows[] = { snapshot_.primary, snapshot_.secondary };
    for (int i = 0; i < 2; ++i)
    {
        const auto& w = windows[i]; const int y = 84 + i * 56;
        text(Label(w, i == 0 ? L"5 Hour" : L"Weekly"), Rect(18,y-4,62,20), theme_.foreground);
        CRect bar = Rect(82,y,200,14); dc.FillSolidRect(bar, theme_.surface);
        if (w.available)
        {
            CRect fill = bar; fill.right = fill.left + static_cast<int>(bar.Width() * w.remaining / 100);
            const int level = UsageAlerts::Level(w.remaining, settings_.warning, settings_.alert);
            dc.FillSolidRect(fill, stale_ ? theme_.muted : level == 2 ? RGB(215,76,76) : level == 1 ? RGB(235,145,40) : RGB(39,151,79));
        }
        dc.Draw3dRect(bar, theme_.border, theme_.border);
        CString percent = L"N/A"; if (w.available) percent.Format(L"%.0f%%", w.remaining);
        text(percent, Rect(292,y-4,50,20), theme_.foreground);
        text(Countdown(w), Rect(82,y+18,258,20), theme_.muted);
    }
    CString credits = L"Reset credits: unavailable";
    if (snapshot_.resetCredits >= 0) credits.Format(L"Reset x %d", snapshot_.resetCredits);
    text(credits, Rect(18,181,230,20), theme_.foreground);
    text(L"Expires: " + (snapshot_.expirations.IsEmpty() ? CString(L"—") : snapshot_.expirations), Rect(18,203,200,20), theme_.muted);
    CString updated = L"Updated: —";
    if (snapshot_.updated)
    {
        tm local{}; localtime_s(&local, &snapshot_.updated);
        updated.Format(L"Updated: %02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    }
        CString next;
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG seconds = nextRefresh_ > now ? (nextRefresh_ - now + 999) / 1000 : 0;
    if (client_.Busy()) next = L" (checking)";
    else next.Format(L" (%llus)", seconds);
    updated += next;
    text(updated, Rect(202,241,140,20), theme_.muted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    CString status = client_.Busy() ? L"Codex: refreshing…" : stale_ ? L"Codex: disconnected · " + error_ : L"Codex: connected";
    text(status, Rect(18,241,178,20), stale_ ? RGB(215,120,60) : theme_.dark ? RGB(89,192,123) : RGB(24,120,63));
    paint.BitBlt(0,0,bounds.Width(),bounds.Height(),&dc,0,0,SRCCOPY);
    dc.SelectObject(oldFont); dc.SelectObject(oldBitmap);
}
BOOL CCodexUsageDlg::OnEraseBkgnd(CDC*) { return TRUE; }
void CCodexUsageDlg::OnLButtonDown(UINT flags, CPoint point)
{
    CDialogEx::OnLButtonDown(flags, point); ReleaseCapture(); SendMessage(WM_NCLBUTTONDOWN, HTCAPTION, 0);
}
void CCodexUsageDlg::OnSettingChange(UINT flags, LPCTSTR section)
{
    CDialogEx::OnSettingChange(flags, section); theme_.Refresh(m_hWnd);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void CCodexUsageDlg::OnDrawItem(int id, LPDRAWITEMSTRUCT item)
{
    if (item->CtlType == ODT_BUTTON) theme_.DrawButton(item); else CDialogEx::OnDrawItem(id, item);
}
void CCodexUsageDlg::OnAction(UINT id)
{
    switch (id)
    {
    case IDC_CMD_CODEX:
        if (!CodexLauncher::OpenCli(m_hWnd, settings_.cliPath.IsEmpty() ? UsageClient::FindExecutable() : settings_.cliPath))
            AfxMessageBox(L"Codex CLI를 실행하지 못했습니다. Settings에서 codex.exe를 지정하세요.");
        break;
    case IDC_GUI_CODEX:
        if (!CodexLauncher::OpenGui(m_hWnd, settings_.guiPath))
            AfxMessageBox(L"Codex GUI를 찾을 수 없습니다. Settings에서 GUI 실행 파일을 지정하세요.");
        break;
    case IDC_SETTINGS:
        {
            SettingsDialog dialog(settings_, this);
            if (dialog.DoModal() == IDOK)
            {
                const bool changed = settings_.cliPath != dialog.settings.cliPath;
                if (changed || settings_.warning != dialog.settings.warning || settings_.alert != dialog.settings.alert) alerts_.Reset();
                settings_ = dialog.settings; settings_.Save(); ApplySettings();
                csvErrorShown_ = false;
                if (changed) { client_.Stop(); snapshot_ = UsageSnapshot(); }
                Refresh();
            }
        }
        break;
    case IDC_MINIMIZE: HideToTray(); break;
    case IDC_CLOSE_WIDGET: OnCancel(); break;
    case IDC_REFRESH: Refresh(); break;
    case IDC_HISTORY:
        if (!settings_.saveCsv)
            AfxMessageBox(L"그래프를 보려면 Settings에서 수집 데이터를 CSV로 저장을 활성화하세요.");
        else { HistoryDialog dialog(this); dialog.DoModal(); }
        break;
    }
}
void CCodexUsageDlg::OnCancel()
{
    KillTimer(1); tray_.Remove(); client_.Stop(); CDialogEx::OnCancel();
}
void CCodexUsageDlg::RestoreWindowPosition()
{
    auto app = AfxGetApp();
    if (!app->GetProfileInt(L"Widget", L"WindowPositionSaved", 0)) return;

    CRect window; GetWindowRect(&window);
    const int savedX = static_cast<int>(app->GetProfileInt(L"Widget", L"WindowX", window.left));
    const int savedY = static_cast<int>(app->GetProfileInt(L"Widget", L"WindowY", window.top));
    CRect target(savedX, savedY, savedX + window.Width(), savedY + window.Height());
    HMONITOR monitor = MonitorFromRect(&target, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof(info) };
    if (GetMonitorInfo(monitor, &info))
    {
        const LONG dx = (std::max<LONG>)(info.rcWork.left - target.left,
            (std::min<LONG>)(0, info.rcWork.right - target.right));
        const LONG dy = (std::max<LONG>)(info.rcWork.top - target.top,
            (std::min<LONG>)(0, info.rcWork.bottom - target.bottom));
        target.OffsetRect(dx, dy);
    }
    SetWindowPos(nullptr, target.left, target.top, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void CCodexUsageDlg::OnDestroy()
{
    SaveWindowPosition();
    CDialogEx::OnDestroy();
}void CCodexUsageDlg::SaveWindowPosition() const
{
    if (!GetSafeHwnd()) return;
    CRect window; GetWindowRect(&window);
    auto app = AfxGetApp();
    app->WriteProfileInt(L"Widget", L"WindowX", window.left);
    app->WriteProfileInt(L"Widget", L"WindowY", window.top);
    app->WriteProfileInt(L"Widget", L"WindowPositionSaved", 1);
}
void CCodexUsageDlg::HideToTray()
{
    // Re-add before hiding, so an unavailable notification area cannot strand the window.
    tray_.Remove();
    if (tray_.Add(m_hWnd, AfxGetApp()->LoadIcon(IDR_MAINFRAME), TrayMessage))
    { tray_.Update(snapshot_, stale_); ShowWindow(SW_HIDE); }
    else ShowWindow(SW_MINIMIZE);
}
void CCodexUsageDlg::ShowWidget()
{
    startupHidden_ = false;
    ShowWindow(SW_RESTORE); SetForegroundWindow();
}
void CCodexUsageDlg::OnWindowPosChanging(WINDOWPOS* position)
{
    CDialogEx::OnWindowPosChanging(position);
    // DoModal normally shows its window after OnInitDialog; suppress that first
    // show as well, so starting in the tray does not flash a dialog on screen.
    if (startupHidden_) position->flags &= ~SWP_SHOWWINDOW;
}
void CCodexUsageDlg::OnSysCommand(UINT id, LPARAM param)
{
    if ((id & 0xfff0) == SC_MINIMIZE) HideToTray();
    else CDialogEx::OnSysCommand(id, param);
}
LRESULT CCodexUsageDlg::OnTaskbarCreated(WPARAM, LPARAM)
{
    if (!tray_.Add(m_hWnd, AfxGetApp()->LoadIcon(IDR_MAINFRAME), TrayMessage)) ShowWidget();
    tray_.Update(snapshot_, stale_); return 0;
}
LRESULT CCodexUsageDlg::OnTray(WPARAM, LPARAM event)
{
    if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK || event == NIN_BALLOONUSERCLICK) ShowWidget();
    else if (event == WM_RBUTTONUP)
    {
        CMenu menu; menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 1, L"창 표시");
        menu.AppendMenu(MF_STRING, 2, L"새로 고침");
        menu.AppendMenu(MF_STRING, 4, L"알림 테스트");
        menu.AppendMenu(MF_STRING, 5, L"About");
        menu.AppendMenu(MF_STRING, 3, L"종료");
        POINT point; GetCursorPos(&point); SetForegroundWindow();
        UINT action = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, this);
        PostMessage(WM_NULL);
        if (action == 1) ShowWidget(); else if (action == 2) Refresh(); else if (action == 3) OnCancel();
        else if (action == 4) tray_.Notify(L"알림 센터 보관 테스트입니다. 배너가 사라진 뒤 Win+N으로 확인하세요.", false);
        else if (action == 5) { AboutDialog about(this); about.DoModal(); }
    }
    return 0;
}
