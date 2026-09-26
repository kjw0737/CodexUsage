#include "pch.h"
#include "StartupRegistration.h"
bool StartupRegistration::IsRegistered() const
{
    wchar_t command[32768]{}; DWORD bytes = sizeof(command);
    return RegGetValue(HKEY_CURRENT_USER, key_, L"CodexUsage", RRF_RT_REG_SZ,
        nullptr, command, &bytes) == ERROR_SUCCESS && command[0] != L'\0';
}
LSTATUS StartupRegistration::SetEnabled(bool enabled) const
{
    HKEY key = nullptr;
    LSTATUS result;
    if (enabled)
        result = RegCreateKeyEx(HKEY_CURRENT_USER, key_, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    else
        result = RegOpenKeyEx(HKEY_CURRENT_USER, key_, 0, KEY_SET_VALUE, &key);
    if (!enabled && result == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (result != ERROR_SUCCESS) return result;
    if (enabled)
    {
        wchar_t path[32768]{};
        const DWORD length = GetModuleFileName(nullptr, path, _countof(path));
        if (length == 0 || length >= _countof(path))
        { RegCloseKey(key); return ERROR_INSUFFICIENT_BUFFER; }
        // Run commands must quote paths containing spaces; no cmd.exe is involved.
        CString command = L"\"" + CString(path) + L"\"";
        result = RegSetValueEx(key, L"CodexUsage", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.GetString()), (command.GetLength() + 1) * sizeof(wchar_t));
    }
    else
    {
        result = RegDeleteValue(key, L"CodexUsage");
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result;
}
