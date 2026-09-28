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
    time_t fiveHourReset = 0;
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
    HoverButton buttons_[6];
    CToolTipCtrl pointTooltip_;
    int hoveredPoint_ = -1;
    std::vector<HistoryPoint> points_;
    time_t origin_ = 0;
    int page_ = 0;
    int range_ = 1;
    CSize minimumSize_;
    bool sizing_ = false;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG*) override;
    afx_msg void OnPaint();
    afx_msg void OnMouseMove(UINT, CPoint);
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg HBRUSH OnCtlColor(CDC*, CWnd*, UINT);
    afx_msg void OnDrawItem(int, LPDRAWITEMSTRUCT);
    afx_msg void OnSettingChange(UINT, LPCTSTR);
    afx_msg void OnSize(UINT, int, int);
    afx_msg void OnEnterSizeMove();
    afx_msg void OnExitSizeMove();
    afx_msg void OnGetMinMaxInfo(MINMAXINFO*);
    void OnRangeChanged();
    void OnFirst();
    void OnPrevious();
    void OnNext();
    void OnLast();
    void OnTokenStatistics();
    CRect PlotRect() const;
    time_t Duration() const;
    int LastPage() const;
    void UpdateNavigation();
    void LayoutControls();
    DECLARE_MESSAGE_MAP()
};
