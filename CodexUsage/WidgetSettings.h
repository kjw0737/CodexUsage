#pragma once
#include "afxdialogex.h"
#include "HoverButton.h"

struct WidgetSettings
{
    int opacity = 90;
    int interval = 60;
    int warning = 50;
    int alert = 20;
    bool topmost = false;
    bool autoStart = false;
    bool startInTray = false;
    bool saveCsv = false;
    CString cliPath, guiPath;
    void Load();
    void Save() const;
};

class WidgetTheme
{
public:
    bool dark = true;
    COLORREF background = 0, foreground = 0, muted = 0, surface = 0, border = 0;
    CBrush brush, fieldBrush;
    void Refresh(HWND window);
    HBRUSH Color(CDC* dc, UINT type);
    void DrawButton(LPDRAWITEMSTRUCT item);
};

class SettingsDialog : public CDialogEx
{
public:
    SettingsDialog(const WidgetSettings& value, CWnd* parent);
    WidgetSettings settings;
private:
    WidgetTheme theme_;
    HoverButton buttons_[4];
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg HBRUSH OnCtlColor(CDC*, CWnd*, UINT);
    afx_msg void OnDrawItem(int, LPDRAWITEMSTRUCT);
    afx_msg void OnSettingChange(UINT, LPCTSTR);
    afx_msg void OnBrowseCli();
    afx_msg void OnBrowseGui();
    void Browse(UINT control);
    DECLARE_MESSAGE_MAP()
};
