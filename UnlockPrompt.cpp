// UnlockPrompt.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

enum ExitErrors
{
    ExitOK,
    ExitInvalidArgs,
    ExitNeedPrivileges,
    ExitFailedToSpawn,
    ExitFailedToWait,
    ExitManageBdeFailed,
    ExitIncorrectPassword
};

// from: http://stackoverflow.com/questions/8046097/how-to-check-if-a-process-has-the-administrative-rights
bool IsElevated()
{
    bool isElevated = false;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(elevation);
        if (GetTokenInformation(token, TokenElevation, &elevation, cbSize, &cbSize)) 
        {
            isElevated = elevation.TokenIsElevated != 0;
        }
    }
    if (token)
    {
        CloseHandle(token);
    }
    return isElevated;
}

int UnlockDrive(const TCHAR* drive)
{
    std::wstring commandLineForUnlock = L"C:\\Windows\\System32\\manage-bde.exe -unlock \"";
    commandLineForUnlock += drive;
    commandLineForUnlock += L"\" -password";

    wprintf(L"Command line: %s\n", commandLineForUnlock.c_str());

    STARTUPINFO startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo = {};

    if (!CreateProcess(
        nullptr,
        (LPWSTR)commandLineForUnlock.c_str(),
        nullptr,
        nullptr,
        false,
        NORMAL_PRIORITY_CLASS,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo))
    {
        wprintf(L"Failed to run manage-bde. Error %d.\n", GetLastError());
        return ExitFailedToSpawn;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    // Wait for the process to complete
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode))
    {
        wprintf(L"Waiting on manage-bde failed. Error %d.\n", GetLastError());
        return ExitFailedToWait;
    }

    wprintf(L"manage-bde exited with code 0x%08x.\n", exitCode);

    // If the process failed, let's see what happened:
    switch (exitCode)
    {
    case 0:
        // No problems encountered
        return ExitOK;
    case -1:
        // Already unlocked
        return ExitOK;
    case FVE_E_FAILED_AUTHENTICATION:
        return ExitIncorrectPassword;
    default:
        // HRESULT of failure
        return ExitManageBdeFailed;
    }
}

void Info(const TCHAR* text, ...)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 10);
    va_list v;
    va_start(v, text);
    vwprintf_s(text, v);
    va_end(v);
    SetConsoleTextAttribute(hConsole, 15);
}

void Error(const TCHAR* text, ...)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 12);
    va_list v;
    va_start(v, text);
    vwprintf_s(text, v);
    va_end(v);
    SetConsoleTextAttribute(hConsole, 15);
}

void WaitForInput()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 10);
    wprintf(_T("Press any key to continue...\n"));
    _getch();
    SetConsoleTextAttribute(hConsole, 15);
}

int wmain(int argc, TCHAR** argv)
{
    if (argc < 2)
    {
        Error(L"Usage: %s <drive:> [<drive2:>...]\n", argv[0]);
        return ExitInvalidArgs;
    }

    if (!IsElevated())
    {
        Error(L"Please run this command in elevated mode.\n");
        return ExitNeedPrivileges;
    }

    bool waitForInput = false;
    bool retryOnIncorrectPassword = true;
    std::vector<std::wstring> drives;

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (_wcsicmp(L"-wait", argv[i]) == 0)
                waitForInput = true;
            else if (_wcsicmp(L"-noretry", argv[i]) == 0)
                retryOnIncorrectPassword = false;
            else
            {
                Error(_T("Unknown switch: '%s'.\n"), argv[i]);
                return ExitInvalidArgs;
            }
        }
        else
        {
            drives.push_back(argv[i]);
        }
    }

    for (auto drive : drives)
    {
        Info(_T("Unlocking %s...\n"), drive.c_str());
        while (true)
        {
            int r = UnlockDrive(drive.c_str());
            if (r == ExitIncorrectPassword && retryOnIncorrectPassword)
                continue;
            else if (r != ExitOK)
            {
                Error(_T("Unlocking %s failed (%d).\n"), drive.c_str(), r);
                if (waitForInput) WaitForInput();
                return r;
            }
            break;
        }
    }

    if (waitForInput) WaitForInput();
    return ExitOK;
}

