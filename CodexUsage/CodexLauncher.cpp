#include "pch.h"
#include "CodexLauncher.h"
#include <shellapi.h>
#include <initializer_list>
#include <shlobj.h>
#include <propkey.h>
#include <appmodel.h>
#include <vector>

namespace
{
    BOOL CALLBACK FindWindow(HWND window, LPARAM context)
    {
        if (GetWindow(window, GW_OWNER) || (GetWindowLongPtr(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) return TRUE;
        DWORD id = 0; GetWindowThreadProcessId(window, &id);
        if (id == GetCurrentProcessId()) return TRUE;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id);
        if (!process) return TRUE;
        wchar_t path[32768]{}; DWORD size = _countof(path);
        const BOOL found = QueryFullProcessImageName(process, 0, path, &size);
        wchar_t appId[512]{}; UINT32 appIdSize = _countof(appId);
        const bool packaged = GetApplicationUserModelId(process, &appIdSize, appId) == ERROR_SUCCESS &&
            wcsncmp(appId, L"OpenAI.Codex_", 13) == 0;
        wchar_t family[512]{}; UINT32 familySize = _countof(family);
        const bool codexPackage = GetPackageFamilyName(process, &familySize, family) == ERROR_SUCCESS &&
            wcsncmp(family, L"OpenAI.Codex_", 13) == 0;
        CloseHandle(process);
        const wchar_t* name = wcsrchr(path, L'\\');
        wchar_t type[128]{}; GetClassName(window, type, _countof(type));
        // Electron GUI only: never activate a CLI console with the same executable name.
        if (wcsstr(type, L"Chrome_WidgetWin") && GetWindowTextLength(window) > 0 &&
            (packaged || codexPackage || (found && wcsstr(path, L"\\WindowsApps\\OpenAI.Codex_")) ||
                (found && name && _wcsicmp(name + 1, L"Codex.exe") == 0)))
        {
            *reinterpret_cast<HWND*>(context) = window;
            // Prefer the visible main window over hidden Electron helper windows.
            return !IsWindowVisible(window);
        }
        return TRUE;
    }
    bool LaunchPackagedGui()
    {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool launched = false;
        {
            CComPtr<IShellItem> folder;
            CComPtr<IEnumShellItems> items;
            if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&folder))) &&
                SUCCEEDED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items))))
            {
                CComPtr<IShellItem> item;
                while (items->Next(1, &item, nullptr) == S_OK)
                {
                    CComPtr<IShellItem2> properties;
                    PWSTR appId = nullptr;
                    if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&properties))))
                        properties->GetString(PKEY_AppUserModel_ID, &appId);
                    if (!appId) item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &appId);
                    if (appId)
                    {
                        if (wcsncmp(appId, L"OpenAI.Codex_", 13) == 0)
                        {
                            CComPtr<IApplicationActivationManager> manager;
                            DWORD processId = 0;
                            if (SUCCEEDED(manager.CoCreateInstance(CLSID_ApplicationActivationManager)))
                                launched = SUCCEEDED(manager->ActivateApplication(appId, nullptr, AO_NONE, &processId));
                        }
                        CoTaskMemFree(appId);
                    }
                    item.Release();
                    if (launched) break;
                }
            }
        }
        if (SUCCEEDED(init)) CoUninitialize();
        return launched;
    }
}
bool CodexLauncher::OpenCli(HWND owner, const CString& executable)
{
    if (executable.IsEmpty()) return false;
    // A widget launched by an automation host can inherit TERM=dumb.
    // Remove only that sentinel from the child's environment; never mutate
    // the widget's environment (the usage worker may be running concurrently).
    LPWCH inherited = GetEnvironmentStringsW();
    if (!inherited) return false;
    std::vector<wchar_t> environment;
    for (const wchar_t* entry = inherited; *entry; entry += wcslen(entry) + 1)
    {
        if (_wcsicmp(entry, L"TERM=dumb") != 0)
            environment.insert(environment.end(), entry, entry + wcslen(entry) + 1);
    }
    FreeEnvironmentStringsW(inherited);
    if (environment.empty()) environment.push_back(L'\0');
    environment.push_back(L'\0');
    // Launch the CLI directly in its own console, avoiding cmd.exe metacharacter expansion.
    CString command = L"\"" + executable + L"\"";
    STARTUPINFO si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcess(executable, command.GetBuffer(), nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &si, &pi);
    command.ReleaseBuffer();
    if (!ok) return false;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
}
bool CodexLauncher::OpenGui(HWND owner, const CString& executable)
{
    HWND existing = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&existing));
    if (existing)
    {
        ShowWindowAsync(existing, SW_RESTORE);
        BringWindowToTop(existing);
        if (!SetForegroundWindow(existing))
        { FLASHWINFO info{ sizeof(info), existing, FLASHW_TRAY, 3, 0 }; FlashWindowEx(&info); }
        return true;
    }
    if (!executable.IsEmpty()) return reinterpret_cast<INT_PTR>(ShellExecute(owner, L"open", executable, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    wchar_t local[32768]{}; GetEnvironmentVariable(L"LOCALAPPDATA", local, _countof(local));
    for (const auto& suffix : { L"\\Programs\\OpenAI\\Codex\\Codex.exe", L"\\Programs\\Codex\\Codex.exe", L"\\OpenAI\\Codex\\Codex.exe" })
    {
        CString path = CString(local) + suffix;
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
            return reinterpret_cast<INT_PTR>(ShellExecute(owner, L"open", path, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    }
    return LaunchPackagedGui();
}
