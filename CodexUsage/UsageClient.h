#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

struct UsageWindow
{
    bool available = false;
    double remaining = 0;
    long long resetAt = 0;
    long long minutes = 0;
};
struct UsageSnapshot
{
    bool connected = false;
    UsageWindow primary, secondary;
    int resetCredits = -1;
    CString expirations;
    CString error;
    CString bucket;
    time_t updated = 0;
};

// Owns only the short-lived app-server it creates; never executes a model turn.
class UsageClient
{
public:
    ~UsageClient();
    bool Start(const CString& executable);
    bool Take(UsageSnapshot& result);
    void Stop();
    bool Busy() const { return busy_; }
    static CString FindExecutable();
    static UsageSnapshot ParseResult(const CString& json);
private:
    UsageSnapshot Fetch(const CString& executable);
    std::thread worker_;
    std::atomic_bool stop_{ false }, busy_{ false };
    std::mutex mutex_;
    UsageSnapshot result_;
    bool ready_ = false;
};
