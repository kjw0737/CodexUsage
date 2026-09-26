#pragma once
#include "UsageClient.h"

// Tracks each quota independently; missing/error responses must not imply recovery.
class UsageAlerts
{
public:
    static int Level(double remaining, int warning, int alert)
    { return remaining <= alert ? 2 : remaining <= warning ? 1 : 0; }
    void Reset() { known_[0] = known_[1] = false; }
    CString Update(const UsageSnapshot& snapshot, int warning, int alert, bool& danger)
    {
        CString messages;
        danger = false;
        if (!snapshot.connected) return messages;
        const UsageWindow windows[] = { snapshot.primary, snapshot.secondary };
        for (int i = 0; i < 2; ++i)
        {
            const auto& current = windows[i];
            if (!current.available) continue;
            const int level = Level(current.remaining, warning, alert);
            if (known_[i])
            {
                const int previous = Level(last_[i].remaining, warning, alert);
                CString event;
                if (level > previous)
                { event = level == 2 ? L"Alert: 위험 기준 도달" : L"Warning: 경고 기준 도달"; danger = true; }
                else if (level < previous || (current.remaining > last_[i].remaining && current.resetAt > last_[i].resetAt && last_[i].resetAt > 0))
                    event = L"남은 사용량 회복";
                if (!event.IsEmpty())
                {
                    CString row;
                    row.Format(L"%s: %s (%.0f%% 남음)", i == 0 ? L"단기 한도" : L"장기 한도", event.GetString(), current.remaining);
                    if (!messages.IsEmpty()) messages += L"\n";
                    messages += row;
                }
            }
            last_[i] = current; known_[i] = true;
        }
        return messages;
    }
private:
    bool known_[2] = { false, false };
    UsageWindow last_[2];
};
