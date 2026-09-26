#pragma once
class CodexLauncher
{
public:
    static bool OpenCli(HWND owner, const CString& executable);
    static bool OpenGui(HWND owner, const CString& executable);
};
