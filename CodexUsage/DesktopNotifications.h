#pragma once
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <roapi.h>
#pragma comment(lib, "windowsapp")
#pragma comment(lib, "propsys")

// Desktop toasts need an AUMID and a shortcut CLSID to persist in Notification Center.
// The stub CLSID supports protocol activation; no COM activation server is registered.
class DesktopNotifications
{
public:
    class Runtime
    {
    public:
        Runtime() : result_(RoInitialize(RO_INIT_SINGLETHREADED)) {}
        ~Runtime() { if (SUCCEEDED(result_)) RoUninitialize(); }
        bool Ready() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }
    private:
        HRESULT result_;
    };
    static constexpr const wchar_t* AppId = L"Citopia.CodexUsage";
    static bool Show(const CString& message, bool warning)
    {
        bool shown = false;
        try
        {
            Register();
            using namespace winrt::Windows::Data::Xml::Dom;
            using namespace winrt::Windows::UI::Notifications;
            XmlDocument xml;
            xml.LoadXml(L"<toast activationType=\"protocol\" launch=\"citopia-codexusage://show\"><visual><binding template=\"ToastGeneric\"><text/><text/></binding></visual></toast>");
            auto texts = xml.GetElementsByTagName(L"text");
            texts.Item(0).AppendChild(xml.CreateTextNode(warning ? L"Codex Usage - Warning / Alert" : L"Codex Usage"));
            texts.Item(1).AppendChild(xml.CreateTextNode(message.GetString()));
            ToastNotification toast(xml);
            GUID eventId{};
            winrt::check_hresult(CoCreateGuid(&eventId));
            wchar_t tag[17]{};
            swprintf_s(tag, L"%08lX%04X%04X", eventId.Data1, eventId.Data2, eventId.Data3);
            toast.Tag(tag);
            toast.Group(L"usage");
            // Unique event tags and no short expiration preserve separate history entries.
            ToastNotificationManager::CreateToastNotifier(AppId).Show(toast);
            shown = true;
        }
        catch (const winrt::hresult_error& error)
        {
            OutputDebugStringW(error.message().c_str());
        }
        return shown;
    }
private:
    static void Register()
    {
        static bool registered = false;
        if (registered) return;
        wchar_t executable[32768]{};
        const DWORD length = GetModuleFileNameW(nullptr, executable, _countof(executable));
        if (!length || length >= _countof(executable)) winrt::throw_hresult(E_FAIL);
        winrt::check_hresult(SetCurrentProcessExplicitAppUserModelID(AppId));

        const CString command = L"\"" + CString(executable) + L"\" --show-from-notification";
        auto setString = [](LPCWSTR key, LPCWSTR name, LPCWSTR value)
        {
            const LSTATUS result = RegSetKeyValueW(HKEY_CURRENT_USER, key, name, REG_SZ,
                value, static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
            winrt::check_hresult(HRESULT_FROM_WIN32(result));
        };
        setString(L"Software\\Classes\\citopia-codexusage", nullptr, L"URL:Codex Usage");
        setString(L"Software\\Classes\\citopia-codexusage", L"URL Protocol", L"");
        setString(L"Software\\Classes\\citopia-codexusage\\shell\\open\\command", nullptr, command);

        PWSTR programs = nullptr;
        winrt::check_hresult(SHGetKnownFolderPath(FOLDERID_Programs, KF_FLAG_CREATE, nullptr, &programs));
        CString path = CString(programs) + L"\\Codex Usage Notifications.lnk";
        CoTaskMemFree(programs);
        auto link = winrt::create_instance<IShellLinkW>(CLSID_ShellLink);
        winrt::check_hresult(link->SetPath(executable));
        winrt::check_hresult(link->SetArguments(L"--show-from-notification"));
        winrt::check_hresult(link->SetIconLocation(executable, 0));
        auto properties = link.as<IPropertyStore>();
        PROPVARIANT id{};
        winrt::check_hresult(InitPropVariantFromString(AppId, &id));
        const HRESULT setId = properties->SetValue(PKEY_AppUserModel_ID, id);
        PropVariantClear(&id);
        winrt::check_hresult(setId);
        CLSID stub{ 0x7d318bd6, 0xd6da, 0x45b6, {0xa0,0x95,0x41,0x77,0x94,0x75,0x1b,0x56} };
        PROPVARIANT clsid{};
        clsid.vt = VT_CLSID; clsid.puuid = &stub;
        winrt::check_hresult(properties->SetValue(PKEY_AppUserModel_ToastActivatorCLSID, clsid));
        winrt::check_hresult(properties->Commit());
        winrt::check_hresult(link.as<IPersistFile>()->Save(path, TRUE));
        registered = true;
    }
};
