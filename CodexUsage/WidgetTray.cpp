#include "pch.h"
#include "WidgetTray.h"
#include "Resource.h"
bool WidgetTray::Add(HWND owner, HICON icon, UINT message)
{
    data_ = {};
    data_.cbSize = sizeof(data_); data_.hWnd = owner; data_.uID = 1;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    // Select the small resource frame directly instead of shrinking LoadIcon's 32px frame.
    data_.hIcon = static_cast<HICON>(LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    if (!data_.hIcon) data_.hIcon = icon;
    data_.uCallbackMessage = message;
    wcscpy_s(data_.szTip, L"Codex Usage: 조회 대기");
    added_ = Shell_NotifyIcon(NIM_ADD, &data_) != FALSE;
    // Legacy callback format preserves standard multiline hover tooltips.
    return added_;
}
void WidgetTray::Remove()
{
    if (added_) Shell_NotifyIcon(NIM_DELETE, &data_);
    added_ = false;
}
void WidgetTray::Update(const UsageSnapshot& snapshot, bool stale)
{
    if (!added_) return;
    CString first = L"N/A", second = L"N/A", tooltip;
    if (snapshot.primary.available) first.Format(L"%.0f%%", snapshot.primary.remaining);
    if (snapshot.secondary.available) second.Format(L"%.0f%%", snapshot.secondary.remaining);
    tooltip.Format(L"Codex Usage%s\n단기 한도: %s 남음\n장기 한도: %s 남음\n클릭: 창 표시 / 오른쪽 클릭: 메뉴",
        stale ? L" (연결 끊김 · 마지막 값)" : L"", first.GetString(), second.GetString());
    wcsncpy_s(data_.szTip, tooltip, _TRUNCATE);
    data_.uFlags = NIF_TIP;
    Shell_NotifyIcon(NIM_MODIFY, &data_);
}
void WidgetTray::Notify(const CString& message, bool warning)
{
    if (!added_ || message.IsEmpty()) return;
    data_.uFlags = NIF_INFO;
    wcscpy_s(data_.szInfoTitle, L"Codex Usage");
    wcsncpy_s(data_.szInfo, message, _TRUNCATE);
    data_.dwInfoFlags = warning ? NIIF_WARNING : NIIF_INFO;
    Shell_NotifyIcon(NIM_MODIFY, &data_);
}
