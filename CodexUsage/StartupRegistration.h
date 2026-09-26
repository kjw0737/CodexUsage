#pragma once

// Current-user registration only. A separate key can be supplied by integration tests.
class StartupRegistration
{
public:
    explicit StartupRegistration(LPCWSTR key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run") : key_(key) {}
    bool IsRegistered() const;
    LSTATUS SetEnabled(bool enabled) const;
private:
    CString key_;
};
