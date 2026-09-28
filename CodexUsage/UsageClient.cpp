#include "pch.h"
#include "UsageClient.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <string>
#include <ctime>
#pragma comment(lib, "windowsapp")

using namespace winrt::Windows::Data::Json;
namespace
{
    struct Handle
    {
        HANDLE value = nullptr;
        ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        Handle() = default;
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
    };
    JsonObject Object(const JsonObject& parent, const wchar_t* key)
    {
        auto value = parent.GetNamedValue(key, JsonValue::CreateNullValue());
        return value.ValueType() == JsonValueType::Object ? value.GetObject() : JsonObject();
    }
    double Number(const JsonObject& parent, const wchar_t* key, double fallback = 0)
    {
        auto value = parent.GetNamedValue(key, JsonValue::CreateNullValue());
        return value.ValueType() == JsonValueType::Number ? value.GetNumber() : fallback;
    }
    UsageWindow Window(const JsonObject& parent, const wchar_t* key)
    {
        auto obj = Object(parent, key);
        UsageWindow result;
        auto value = obj.GetNamedValue(L"usedPercent", JsonValue::CreateNullValue());
        if (value.ValueType() != JsonValueType::Number) return result;
        result.available = true;
        result.remaining = (std::clamp)(100.0 - value.GetNumber(), 0.0, 100.0);
        result.minutes = static_cast<long long>(Number(obj, L"windowDurationMins"));
        result.resetAt = static_cast<long long>(Number(obj, L"resetsAt"));
        return result;
    }
    CString Decode(const std::string& utf8)
    {
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        CString result;
        if (size) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.GetBuffer(size), size);
        result.ReleaseBuffer(size);
        return result;
    }
    bool Send(HANDLE pipe, const char* text)
    {
        DWORD written = 0;
        const DWORD length = static_cast<DWORD>(strlen(text));
        return WriteFile(pipe, text, length, &written, nullptr) && written == length;
    }
}

UsageSnapshot UsageClient::ParseResult(const CString& json)
{
    auto root = JsonObject::Parse(static_cast<LPCWSTR>(json));
    UsageSnapshot result;
    auto bucket = Object(root, L"rateLimits");
    auto buckets = Object(root, L"rateLimitsByLimitId");
    if (buckets.Size())
    {
        // Do not silently substitute an unrelated model-specific quota.
        bucket = Object(buckets, L"codex");
        if (!bucket.Size())
        {
            result.error = L"Codex 기본 한도가 응답에 없습니다.";
            return result;
        }
    }
    result.bucket = L"codex";
    result.primary = Window(bucket, L"primary");
    result.secondary = Window(bucket, L"secondary");
    auto credits = Object(root, L"rateLimitResetCredits");
    result.resetCredits = static_cast<int>(Number(credits, L"availableCount", -1));
    auto rows = credits.GetNamedValue(L"credits", JsonValue::CreateNullValue());
    if (rows.ValueType() == JsonValueType::Array)
    {
        for (auto row : rows.GetArray())
        {
            if (row.ValueType() != JsonValueType::Object) continue;
            auto obj = row.GetObject();
            if (obj.GetNamedString(L"status", L"") != L"available") continue;
            if (!result.expirations.IsEmpty()) result.expirations += L", ";
            auto expires = static_cast<time_t>(Number(obj, L"expiresAt"));
            tm local{};
            if (expires > 0 && localtime_s(&local, &expires) == 0)
            {
                wchar_t date[32]{};
                wcsftime(date, _countof(date), L"%Y-%m-%d", &local);
                result.expirations += date;
            }
            else result.expirations += L"만료일 없음";
        }
    }
    if (result.expirations.IsEmpty()) result.expirations = L"상세 정보 없음";
    result.connected = true;
    result.updated = time(nullptr);
    return result;
}

CString UsageClient::FindExecutable()
{
    wchar_t path[32768]{}, search[32768]{};
    GetEnvironmentVariable(L"PATH", search, _countof(search));
    if (SearchPath(search, L"codex.exe", nullptr, _countof(path), path, nullptr)) return CString(path);
    // Explorer-launched apps may not inherit a newly installed CLI's PATH.
    wchar_t local[32768]{};
    GetEnvironmentVariable(L"LOCALAPPDATA", local, _countof(local));
    CString base = CString(local) + L"\\OpenAI\\Codex\\bin\\";
    WIN32_FIND_DATA entry{};
    HANDLE find = FindFirstFile(base + L"*", &entry);
    CString newest;
    FILETIME newestTime{};
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || entry.cFileName[0] == L'.') continue;
            CString candidate = base + entry.cFileName + L"\\codex.exe";
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (GetFileAttributesEx(candidate, GetFileExInfoStandard, &data) &&
                (newest.IsEmpty() || CompareFileTime(&data.ftLastWriteTime, &newestTime) > 0))
            { newest = candidate; newestTime = data.ftLastWriteTime; }
        } while (FindNextFile(find, &entry));
        FindClose(find);
    }
    if (!newest.IsEmpty()) return newest;
    CString bundled = CString(local) + L"\\Programs\\OpenAI\\Codex\\bin\\codex.exe";
    if (GetFileAttributes(bundled) != INVALID_FILE_ATTRIBUTES) return bundled;
    return CString();
}

UsageClient::~UsageClient() { Stop(); }
void UsageClient::Stop()
{
    stop_ = true;
    if (worker_.joinable()) worker_.join();
    busy_ = false;
}
bool UsageClient::Start(const CString& executable)
{
    if (busy_) return false;
    if (worker_.joinable()) worker_.join();
    stop_ = false;
    busy_ = true;
    { std::lock_guard<std::mutex> lock(mutex_); ready_ = false; }
    worker_ = std::thread([this, executable] {
        UsageSnapshot value;
        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            try { value = Fetch(executable); }
            catch (...) { value.error = L"Codex 응답을 해석하지 못했습니다. CLI 버전을 확인하세요."; }
            winrt::uninit_apartment();
        }
        catch (...) { value.error = L"사용량 조회 초기화에 실패했습니다."; }
        { std::lock_guard<std::mutex> lock(mutex_); result_ = value; ready_ = true; }
        busy_ = false;
    });
    return true;
}
bool UsageClient::Take(UsageSnapshot& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ready_) return false;
    result = result_;
    ready_ = false;
    return true;
}

UsageSnapshot UsageClient::Fetch(const CString& executable)
{
    UsageSnapshot failed;
    if (executable.IsEmpty() || GetFileAttributes(executable) == INVALID_FILE_ATTRIBUTES)
    {
        failed.error = L"Codex CLI를 찾을 수 없습니다. Settings에서 codex.exe를 지정하세요.";
        return failed;
    }
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    Handle outputRead, outputWrite, inputRead, inputWrite, errorFile, process, thread, job;
    if (!CreatePipe(&outputRead.value, &outputWrite.value, &sa, 0) ||
        !CreatePipe(&inputRead.value, &inputWrite.value, &sa, 0))
    { failed.error = L"통신 파이프를 만들지 못했습니다."; return failed; }
    SetHandleInformation(outputRead.value, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inputWrite.value, HANDLE_FLAG_INHERIT, 0);
    errorFile.value = CreateFile(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    job.value = CreateJobObject(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
    { failed.error = L"조회 프로세스 관리 초기화 실패"; return failed; }
    STARTUPINFO si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = inputRead.value; si.hStdOutput = outputWrite.value; si.hStdError = errorFile.value;
    PROCESS_INFORMATION pi{};
    CString command = L"\"" + executable + L"\" app-server --listen stdio://";
    BOOL created = CreateProcess(executable, command.GetBuffer(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
    command.ReleaseBuffer();
    if (!created) { failed.error = L"Codex CLI 실행 실패. 실행 파일 경로를 확인하세요."; return failed; }
    process.value = pi.hProcess; thread.value = pi.hThread;
    if (!AssignProcessToJobObject(job.value, process.value))
    { TerminateProcess(process.value, 1); failed.error = L"조회 프로세스를 관리할 수 없습니다."; return failed; }
    ResumeThread(thread.value);
    CloseHandle(outputWrite.value); outputWrite.value = nullptr;
    CloseHandle(inputRead.value); inputRead.value = nullptr;
    if (!Send(inputWrite.value, "{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"citopia_codex_usage\",\"version\":\"1.0.2026.0929\"},\"capabilities\":{\"experimentalApi\":true}}}\n"))
    { failed.error = L"Codex 초기화 요청 실패"; return failed; }
    std::string buffer;
    const ULONGLONG deadline = GetTickCount64() + 25000;
    while (!stop_ && GetTickCount64() < deadline)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(outputRead.value, nullptr, 0, nullptr, &available, nullptr)) break;
        if (!available)
        {
            if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) break;
            Sleep(40); continue;
        }
        char chunk[8192]; DWORD received = 0;
        if (!ReadFile(outputRead.value, chunk, (std::min)(available, static_cast<DWORD>(sizeof(chunk))), &received, nullptr)) break;
        buffer.append(chunk, received);
        if (buffer.size() > 4 * 1024 * 1024) { failed.error = L"Codex 응답 크기가 제한을 초과했습니다."; return failed; }
        size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos)
        {
            const auto line = buffer.substr(0, newline); buffer.erase(0, newline + 1);
            JsonObject message;
            if (!JsonObject::TryParse(static_cast<LPCWSTR>(Decode(line)), message)) continue;
            const int id = static_cast<int>(Number(message, L"id", -1));
            if (id != 1 && id != 2) continue;
            if (message.HasKey(L"error"))
            {
                failed.error = L"조회 실패: Codex CLI의 ChatGPT 로그인 및 네트워크를 확인하세요.";
                return failed;
            }
            if (id == 1)
            {
                if (!Send(inputWrite.value, "{\"method\":\"initialized\",\"params\":{}}\n{\"id\":2,\"method\":\"account/rateLimits/read\"}\n"))
                { failed.error = L"사용량 요청 전송 실패"; return failed; }
            }
            else
            {
                auto result = Object(message, L"result");
                if (!result.HasKey(L"rateLimits") && !result.HasKey(L"rateLimitsByLimitId"))
                { failed.error = L"사용량 응답 형식이 올바르지 않습니다."; return failed; }
                return ParseResult(CString(result.Stringify().c_str()));
            }
        }
    }
    failed.error = stop_ ? L"조회 취소" : L"Codex 응답 없음 (25초 제한). 로그인/네트워크를 확인하세요.";
    return failed;
}
