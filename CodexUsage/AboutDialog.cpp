#include "pch.h"
#include "Resource.h"
#include "AboutDialog.h"
#include <vector>
#include <winver.h>
#pragma comment(lib, "version.lib")

namespace
{
    CString ExecutableVersion()
    {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, _countof(path))) return L"Unknown";
        const DWORD size = GetFileVersionInfoSizeW(path, nullptr);
        if (!size) return L"Unknown";
        std::vector<BYTE> info(size);
        if (!GetFileVersionInfoW(path, 0, size, info.data())) return L"Unknown";
        void* value = nullptr;
        UINT length = 0;
        if (VerQueryValueW(info.data(), L"\\StringFileInfo\\041204B0\\ProductVersion", &value, &length)
            && value && length > 1)
            return static_cast<LPCWSTR>(value);
        VS_FIXEDFILEINFO* fixed = nullptr;
        if (VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &length)
            && fixed && length >= sizeof(VS_FIXEDFILEINFO))
        {
            CString version;
            version.Format(L"%u.%u.%u.%04u", HIWORD(fixed->dwProductVersionMS),
                LOWORD(fixed->dwProductVersionMS), HIWORD(fixed->dwProductVersionLS),
                LOWORD(fixed->dwProductVersionLS));
            return version;
        }
        return L"Unknown";
    }
}

AboutDialog::AboutDialog(CWnd* parent) : CDialogEx(IDD_ABOUT, parent) {}

BEGIN_MESSAGE_MAP(AboutDialog, CDialogEx)
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
    ON_WM_SETTINGCHANGE()
END_MESSAGE_MAP()

BOOL AboutDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    theme_.Refresh(m_hWnd);
    closeButton_.SubclassDlgItem(IDOK, this);
    SetDlgItemText(IDC_ABOUT_VERSION, L"Version " + ExecutableVersion());
    HICON icon = static_cast<HICON>(LoadImageW(AfxGetInstanceHandle(),
        MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 32, 32, LR_SHARED));
    GetDlgItem(IDC_ABOUT_ICON)->SendMessage(STM_SETICON, reinterpret_cast<WPARAM>(icon));
    return TRUE;
}

HBRUSH AboutDialog::OnCtlColor(CDC* dc, CWnd* window, UINT type)
{
    if (type == CTLCOLOR_STATIC || type == CTLCOLOR_DLG || type == CTLCOLOR_BTN)
        return theme_.Color(dc, type);
    return CDialogEx::OnCtlColor(dc, window, type);
}

void AboutDialog::OnDrawItem(int id, LPDRAWITEMSTRUCT item)
{
    if (item->CtlType == ODT_BUTTON) theme_.DrawButton(item);
    else CDialogEx::OnDrawItem(id, item);
}

void AboutDialog::OnSettingChange(UINT flags, LPCTSTR section)
{
    CDialogEx::OnSettingChange(flags, section);
    theme_.Refresh(m_hWnd);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}