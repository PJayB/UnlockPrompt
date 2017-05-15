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
    ExitManageBdeFailed
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

int wmain(int argc, TCHAR** argv)
{
    if (argc != 2)
    {
        wprintf(L"Usage: %s <drive:>\n", argv[0]);
        return ExitInvalidArgs;
    }

    if (!IsElevated())
    {
        wprintf(L"Please run this command in elevated mode.\n");
        return ExitNeedPrivileges;
    }

    std::wstring commandLineForUnlock = L"C:\\Windows\\System32\\manage-bde.exe -unlock \"";
    commandLineForUnlock += argv[1];
    commandLineForUnlock += L"\" -password";
    
    wprintf(L"Command line: %s\n", commandLineForUnlock.c_str());

    STARTUPINFO startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    
    PROCESS_INFORMATION processInfo = {};

    if (!CreateProcess(
        nullptr,
        (LPWSTR) commandLineForUnlock.c_str(),
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
    default:
        // HRESULT of failure
        wprintf(L"Press any key to continue...\n");
        _getch();
        return ExitManageBdeFailed;
    }
}
