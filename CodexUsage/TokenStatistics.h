#pragma once
#include "UsageHistory.h"
#include "afxdialogex.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct TurnTokenUsage
{
    std::wstring model;
    uint64_t input = 0, cachedInput = 0, output = 0, reasoningOutput = 0, total = 0;
};

struct ModelTokenStatistics
{
    std::wstring model;
    size_t commands = 0;
    uint64_t input = 0, cachedInput = 0, output = 0, reasoningOutput = 0, total = 0;
    uint64_t minimum = 0, median = 0, maximum = 0;
};

class TokenStatistics
{
public:
    static std::filesystem::path DefaultSessionsDirectory();
    static std::vector<TurnTokenUsage> Read(const std::filesystem::path& directory);
    static std::vector<ModelTokenStatistics> Summarize(const std::vector<TurnTokenUsage>& turns);
    static CString BuildReport(const std::vector<ModelTokenStatistics>& statistics,
                               const std::vector<HistoryPoint>& history);
};

class TokenStatisticsDialog : public CDialogEx
{
public:
    TokenStatisticsDialog(const CString& report, CWnd* parent);
private:
    CString report_;
    CSize minimumSize_;
    BOOL OnInitDialog() override;
    afx_msg void OnSize(UINT, int, int);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO*);
    DECLARE_MESSAGE_MAP()
};
