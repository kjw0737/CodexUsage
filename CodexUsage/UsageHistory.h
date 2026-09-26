#pragma once
#include "UsageClient.h"
#include "WidgetSettings.h"
#include "afxdialogex.h"
#include <vector>

struct HistoryPoint
{
    time_t when = 0;
    double fiveHour = 0;
    double weekly = 0;
};

class UsageHistory
{
public:
    static bool Append(const UsageSnapshot& snapshot);
    static std::vector<HistoryPoint> Read();
};

class HistoryDialog : public CDialogEx
{
public:
    explicit HistoryDialog(CWnd* parent);
private:
    WidgetTheme theme_;
    HoverButton buttons_[5];
    CToolTipCtrl pointTooltip_;
    int hoveredPoint_ = -1;
    std::vector<HistoryPoint> points_;
    time_t origin_ = 0;
    int page_ = 0;
    int range_ = 0;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG*) override;
    afx_msg void OnPaint();
    afx_msg void OnMouseMove(UINT, CPoint);
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg HBRUSH OnCtlColor(CDC*, CWnd*, UINT);
    afx_msg void OnDrawItem(int, LPDRAWITEMSTRUCT);
    afx_msg void OnSettingChange(UINT, LPCTSTR);
    void OnRangeChanged();
    void OnFirst();
    void OnPrevious();
    void OnNext();
    void OnLast();
    CRect PlotRect() const;
    time_t Duration() const;
    int LastPage() const;
    void UpdateNavigation();
    DECLARE_MESSAGE_MAP()
};
