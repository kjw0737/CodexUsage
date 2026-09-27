#pragma once
#include "afxdialogex.h"
#include "WidgetSettings.h"

class AboutDialog : public CDialogEx
{
public:
    explicit AboutDialog(CWnd* parent = nullptr);
private:
    WidgetTheme theme_;
    HoverButton closeButton_;
    BOOL OnInitDialog() override;
    afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* window, UINT type);
    afx_msg void OnDrawItem(int id, LPDRAWITEMSTRUCT item);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    DECLARE_MESSAGE_MAP()
};