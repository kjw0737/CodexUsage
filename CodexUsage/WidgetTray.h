#pragma once
#include <shellapi.h>
#include "UsageClient.h"
class WidgetTray
{
public:
    ~WidgetTray() { Remove(); }
    bool Add(HWND owner, HICON icon, UINT message);
    void Remove();
    void Update(const UsageSnapshot& snapshot, bool stale);
    void Notify(const CString& message, bool warning);
private:
    NOTIFYICONDATA data_{};
    bool added_ = false;
};
