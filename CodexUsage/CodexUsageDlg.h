#pragma once
#include "afxdialogex.h"
#include "UsageClient.h"
#include "WidgetSettings.h"
#include "WidgetTray.h"
#include "UsageAlerts.h"
class CCodexUsageDlg : public CDialogEx
{
public:
    CCodexUsageDlg(CWnd* parent = nullptr);
private:
    WidgetSettings settings_;
    WidgetTheme theme_;
    UsageClient client_;
    UsageSnapshot snapshot_;
    WidgetTray tray_;
    UsageAlerts alerts_;
    HoverButton buttons_[7];
    CFont font_, titleFont_;
    CToolTipCtrl tooltip_;
    CString error_;
    ULONGLONG nextRefresh_ = 0;
    bool stale_ = false;
    bool startupHidden_ = false;
    bool csvErrorShown_ = false;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG*) override;
    void OnOK() override {}
    void OnCancel() override;
    void Refresh();
    void ApplySettings();
    void ShowWidget();
    void HideToTray();
    afx_msg LRESULT OnTray(WPARAM, LPARAM);
    afx_msg LRESULT OnTaskbarCreated(WPARAM, LPARAM);
    afx_msg void OnSysCommand(UINT, LPARAM);
    afx_msg void OnWindowPosChanging(WINDOWPOS*);
    CRect Rect(int x, int y, int width, int height) const;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg void OnTimer(UINT_PTR);
    afx_msg void OnLButtonDown(UINT, CPoint);
    afx_msg void OnSettingChange(UINT, LPCTSTR);
    afx_msg void OnDrawItem(int, LPDRAWITEMSTRUCT);
    afx_msg void OnAction(UINT);
    DECLARE_MESSAGE_MAP()
};
