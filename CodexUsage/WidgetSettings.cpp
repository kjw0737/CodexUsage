#include "pch.h"
#include "Resource.h"
#include "WidgetSettings.h"
#include "StartupRegistration.h"
#include <algorithm>
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "uxtheme")

void WidgetSettings::Load()
{
    auto app = AfxGetApp();
    opacity = (std::clamp)(static_cast<int>(app->GetProfileInt(L"Widget", L"Opacity", 90)), 20, 100);
    interval = (std::clamp)(static_cast<int>(app->GetProfileInt(L"Widget", L"Interval", 60)), 10, 3600);
    topmost = app->GetProfileInt(L"Widget", L"Topmost", 0) != 0;
    autoStart = StartupRegistration().IsRegistered();
    startInTray = app->GetProfileInt(L"Widget", L"StartInTray", 0) != 0;
    warning = (std::clamp)(static_cast<int>(app->GetProfileInt(L"Widget", L"Warning", 50)), 1, 100);
    alert = (std::clamp)(static_cast<int>(app->GetProfileInt(L"Widget", L"Alert", 20)), 0, warning - 1);
    cliPath = app->GetProfileString(L"Widget", L"CliPath");
    guiPath = app->GetProfileString(L"Widget", L"GuiPath");
}
void WidgetSettings::Save() const
{
    auto app = AfxGetApp();
    app->WriteProfileInt(L"Widget", L"Opacity", opacity);
    app->WriteProfileInt(L"Widget", L"Interval", interval);
    app->WriteProfileInt(L"Widget", L"Topmost", topmost);
    app->WriteProfileInt(L"Widget", L"StartInTray", startInTray);
    app->WriteProfileInt(L"Widget", L"Warning", warning);
    app->WriteProfileInt(L"Widget", L"Alert", alert);
    app->WriteProfileString(L"Widget", L"CliPath", cliPath);
    app->WriteProfileString(L"Widget", L"GuiPath", guiPath);
}
void WidgetTheme::Refresh(HWND window)
{
    DWORD light = 1, size = sizeof(light);
    RegGetValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    dark = light == 0;
    HIGHCONTRAST hc{ sizeof(hc) };
    SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
    const bool highContrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    background = dark ? RGB(25, 28, 33) : RGB(248, 249, 251);
    foreground = dark ? RGB(235, 239, 245) : RGB(28, 34, 43);
    muted = dark ? RGB(153, 164, 180) : RGB(90, 101, 117);
    surface = dark ? RGB(46, 52, 62) : RGB(232, 237, 243);
    border = dark ? RGB(75, 84, 99) : RGB(181, 190, 204);
    if (highContrast)
    {
        background = GetSysColor(COLOR_WINDOW); foreground = GetSysColor(COLOR_WINDOWTEXT);
        surface = GetSysColor(COLOR_BTNFACE); border = foreground; muted = foreground;
    }
    brush.DeleteObject(); brush.CreateSolidBrush(background);
    fieldBrush.DeleteObject(); fieldBrush.CreateSolidBrush(surface);
    BOOL useDark = dark;
    DwmSetWindowAttribute(window, 20, &useDark, sizeof(useDark));
}
HBRUSH WidgetTheme::Color(CDC* dc, UINT type)
{
    dc->SetTextColor(foreground);
    const bool field = type == CTLCOLOR_EDIT || type == CTLCOLOR_LISTBOX;
    dc->SetBkColor(field ? surface : background);
    return static_cast<HBRUSH>((field ? fieldBrush : brush).GetSafeHandle());
}
void WidgetTheme::DrawButton(LPDRAWITEMSTRUCT item)
{
    CDC dc; dc.Attach(item->hDC);
    CRect rect(item->rcItem);
    const bool selected = (item->itemState & ODS_SELECTED) != 0;
    const bool enabled = (item->itemState & ODS_DISABLED) == 0;
    POINT cursor{}; GetCursorPos(&cursor);
    const bool hot = enabled && WindowFromPoint(cursor) == item->hwndItem;
    const bool caption = item->CtlID == IDC_MINIMIZE || item->CtlID == IDC_CLOSE_WIDGET;
    const bool close = item->CtlID == IDC_CLOSE_WIDGET;
    const COLORREF hover = close ? RGB(205,55,65) : dark ? RGB(65,79,99) : RGB(208,225,247);
    dc.FillSolidRect(rect, selected ? (close ? RGB(170,35,45) : border) : hot ? hover : caption ? background : surface);
    if (!caption || hot || selected)
        dc.Draw3dRect(rect, hot ? (close ? hover : RGB(105,158,219)) : border, hot ? (close ? hover : RGB(105,158,219)) : border);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(!enabled ? muted : close && (hot || selected) ? RGB(255,255,255) : foreground);
    auto old = dc.SelectObject(CFont::FromHandle(reinterpret_cast<HFONT>(::SendMessage(item->hwndItem, WM_GETFONT, 0, 0))));
    CString label; ::GetWindowText(item->hwndItem, label.GetBuffer(256), 256); label.ReleaseBuffer();
    CRect labelRect(rect); if (selected) labelRect.OffsetRect(1, 1);
    dc.DrawText(label, labelRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS) { rect.DeflateRect(3, 3); dc.DrawFocusRect(rect); }
    dc.SelectObject(old); dc.Detach();
}

SettingsDialog::SettingsDialog(const WidgetSettings& value, CWnd* parent)
    : CDialogEx(IDD_SETTINGS, parent), settings(value) {}
BEGIN_MESSAGE_MAP(SettingsDialog, CDialogEx)
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
    ON_WM_SETTINGCHANGE()
    ON_BN_CLICKED(IDC_BROWSE_CLI, &SettingsDialog::OnBrowseCli)
    ON_BN_CLICKED(IDC_BROWSE_GUI, &SettingsDialog::OnBrowseGui)
END_MESSAGE_MAP()
BOOL SettingsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    const UINT ids[] = { IDC_BROWSE_CLI, IDC_BROWSE_GUI, IDOK, IDCANCEL };
    for (int i = 0; i < 4; ++i) buttons_[i].SubclassDlgItem(ids[i], this);
    theme_.Refresh(m_hWnd);
    SetDlgItemInt(IDC_OPACITY, settings.opacity);
    SetDlgItemInt(IDC_INTERVAL, settings.interval);
    SetDlgItemInt(IDC_WARNING, settings.warning);
    SetDlgItemInt(IDC_ALERT, settings.alert);
    CheckDlgButton(IDC_TOPMOST, settings.topmost ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_AUTOSTART, settings.autoStart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_START_TRAY, settings.startInTray ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemText(IDC_CLI_PATH, settings.cliPath);
    SetDlgItemText(IDC_GUI_PATH, settings.guiPath);
    auto opacity = static_cast<CSpinButtonCtrl*>(GetDlgItem(IDC_OPACITY_SPIN));
    opacity->SetBuddy(GetDlgItem(IDC_OPACITY)); opacity->SetRange32(20, 100); opacity->SetPos32(settings.opacity);
    auto interval = static_cast<CSpinButtonCtrl*>(GetDlgItem(IDC_INTERVAL_SPIN));
    interval->SetBuddy(GetDlgItem(IDC_INTERVAL)); interval->SetRange32(10, 3600); interval->SetPos32(settings.interval);
    SetWindowTheme(GetDlgItem(IDC_TOPMOST)->m_hWnd, L"", L"");
    SetWindowTheme(GetDlgItem(IDC_AUTOSTART)->m_hWnd, L"", L"");
    SetWindowTheme(GetDlgItem(IDC_START_TRAY)->m_hWnd, L"", L"");
    return TRUE;
}
void SettingsDialog::OnOK()
{
    BOOL validOpacity = FALSE, validInterval = FALSE;
    int opacity = GetDlgItemInt(IDC_OPACITY, &validOpacity, FALSE);
    int interval = GetDlgItemInt(IDC_INTERVAL, &validInterval, FALSE);
    if (!validOpacity || opacity < 20 || opacity > 100 || !validInterval || interval < 10 || interval > 3600)
    { AfxMessageBox(L"투명도는 20~100%, 조회 주기는 10~3600초로 입력하세요."); return; }
    CString cli, gui;
    BOOL validWarning = FALSE, validAlert = FALSE;
    int warning = GetDlgItemInt(IDC_WARNING, &validWarning, FALSE);
    int alert = GetDlgItemInt(IDC_ALERT, &validAlert, FALSE);
    if (!validWarning || !validAlert || alert < 0 || warning > 100 || alert >= warning)
    { AfxMessageBox(L"남은 비율 기준은 0 ≤ Alert < Warning ≤ 100으로 입력하세요."); return; }
    GetDlgItemText(IDC_CLI_PATH, cli); GetDlgItemText(IDC_GUI_PATH, gui);
    cli.Trim(); gui.Trim();
    for (const auto& path : { cli, gui })
    {
        if (path.IsEmpty()) continue;
        const DWORD attr = GetFileAttributes(path);
        if (path.Find(L'"') >= 0 || path.Right(4).CompareNoCase(L".exe") != 0 ||
            attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        { AfxMessageBox(L"실행 파일은 존재하는 .exe 경로로 지정하세요. 자동 검색은 빈칸으로 두세요."); return; }
    }
    const bool autoStart = IsDlgButtonChecked(IDC_AUTOSTART) == BST_CHECKED;
    const LSTATUS registration = StartupRegistration().SetEnabled(autoStart);
    if (registration != ERROR_SUCCESS)
    {
        CString message;
        message.Format(L"시작 프로그램 등록을 변경하지 못했습니다. 오류 코드: %ld\n설정이 저장되지 않았습니다.", registration);
        AfxMessageBox(message); return;
    }
    settings.autoStart = autoStart;
    settings.startInTray = IsDlgButtonChecked(IDC_START_TRAY) == BST_CHECKED;
    settings.opacity = opacity; settings.interval = interval;
    settings.warning = warning; settings.alert = alert;
    settings.topmost = IsDlgButtonChecked(IDC_TOPMOST) == BST_CHECKED;
    settings.cliPath = cli; settings.guiPath = gui;
    CDialogEx::OnOK();
}
HBRUSH SettingsDialog::OnCtlColor(CDC* dc, CWnd*, UINT type) { return theme_.Color(dc, type); }
void SettingsDialog::OnDrawItem(int id, LPDRAWITEMSTRUCT item)
{
    if (item->CtlType == ODT_BUTTON) theme_.DrawButton(item); else CDialogEx::OnDrawItem(id, item);
}
void SettingsDialog::OnSettingChange(UINT flags, LPCTSTR section)
{
    CDialogEx::OnSettingChange(flags, section); theme_.Refresh(m_hWnd); RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void SettingsDialog::Browse(UINT control)
{
    CFileDialog dialog(TRUE, L"exe", nullptr, OFN_FILEMUSTEXIST, L"Programs (*.exe)|*.exe||", this);
    if (dialog.DoModal() == IDOK) SetDlgItemText(control, dialog.GetPathName());
}
void SettingsDialog::OnBrowseCli() { Browse(IDC_CLI_PATH); }
void SettingsDialog::OnBrowseGui() { Browse(IDC_GUI_PATH); }
