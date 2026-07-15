// MyQQ 客户端 - Win32 GUI 版
// 编译: g++ client_gui.cpp -o MyQQClient.exe -mwindows -lws2_32 -lcomctl32 -static
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include "connect.h"
#include <richedit.h>

#define WM_USER_LOGIN_RESULT   (WM_USER + 1)
#define WM_USER_REG_RESULT     (WM_USER + 2)
#define WM_USER_RECV_MSG       (WM_USER + 3)
#define WM_USER_DISCONNECTED   (WM_USER + 4)
#define WM_USER_CONN_ERROR     (WM_USER + 5)

#define IDC_USERNAME     101
#define IDC_PASSWORD     102
#define IDC_CONFIRM_PWD  103
#define IDC_LOGIN_BTN    104
#define IDC_SWITCH_BTN   105
#define IDC_STATUS_LBL   106
#define IDC_MSG_INPUT    201
#define IDC_SEND_BTN     202
#define IDC_FRIEND_LIST  203
#define IDC_CHAT_DISPLAY 204
#define IDC_GROUP_BOX   205

HINSTANCE g_hInst = nullptr;
SOCKET    g_sock = INVALID_SOCKET;
std::string g_username;
std::string g_password;
bool g_isLoginMode = true;

LRESULT CALLBACK LoginWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
void DoLogin(HWND);
void DoRegister(HWND);
void DoSendMessage(HWND);
void RecvLoop(HWND);
void SwitchMode(HWND);
void SetStatus(HWND, const wchar_t*);
void AddChatMsg(HWND, const wchar_t*, COLORREF);
void InitRichEdit(HWND);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    g_hInst = hInst;
    InitCommonControls();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LoginWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(240, 242, 245));
    wc.lpszClassName = L"MyQQLoginClass";
    RegisterClassExW(&wc);

    WNDCLASSEXW wc2 = { sizeof(WNDCLASSEXW) };
    wc2.style = CS_HREDRAW | CS_VREDRAW;
    wc2.lpfnWndProc = MainWndProc;
    wc2.hInstance = hInst;
    wc2.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc2.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc2.lpszClassName = L"MyQQMainClass";
    RegisterClassExW(&wc2);

    LoadLibraryW(L"Msftedit.dll");

    HWND hLogin = CreateWindowExW(0, L"MyQQLoginClass", L"MyQQ - 登录",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 420,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(hLogin, nShow);
    UpdateWindow(hLogin);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK LoginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"MyQQ", WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 20, 420, 40, hWnd, nullptr, g_hInst, nullptr);
        HFONT hTitleFont = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessage(GetDlgItem(hWnd, 0), WM_SETFONT, (WPARAM)hTitleFont, TRUE);

        CreateWindowW(L"STATIC", L"用户名", WS_CHILD | WS_VISIBLE,
            60, 80, 60, 20, hWnd, nullptr, g_hInst, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            130, 76, 220, 28, hWnd, (HMENU)IDC_USERNAME, g_hInst, nullptr);

        CreateWindowW(L"STATIC", L"密码", WS_CHILD | WS_VISIBLE,
            60, 120, 60, 20, hWnd, nullptr, g_hInst, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
            130, 116, 220, 28, hWnd, (HMENU)IDC_PASSWORD, g_hInst, nullptr);

        CreateWindowW(L"STATIC", L"确认密码", WS_CHILD | WS_VISIBLE,
            60, 160, 60, 20, hWnd, nullptr, g_hInst, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
            130, 156, 220, 28, hWnd, (HMENU)IDC_CONFIRM_PWD, g_hInst, nullptr);

        CreateWindowW(L"BUTTON", L"登  录", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, 206, 220, 38, hWnd, (HMENU)IDC_LOGIN_BTN, g_hInst, nullptr);

        CreateWindowW(L"BUTTON", L"没有账号？点击注册", WS_CHILD | WS_VISIBLE | BS_FLAT,
            130, 250, 220, 24, hWnd, (HMENU)IDC_SWITCH_BTN, g_hInst, nullptr);
        HFONT hLinkFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, TRUE, TRUE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessage(GetDlgItem(hWnd, IDC_SWITCH_BTN), WM_SETFONT, (WPARAM)hLinkFont, TRUE);

        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
            60, 285, 300, 24, hWnd, (HMENU)IDC_STATUS_LBL, g_hInst, nullptr);

        HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessage(GetDlgItem(hWnd, IDC_USERNAME), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, IDC_PASSWORD), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, IDC_CONFIRM_PWD), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, IDC_LOGIN_BTN), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hWnd, IDC_STATUS_LBL), WM_SETFONT, (WPARAM)hFont, TRUE);

        ShowWindow(GetDlgItem(hWnd, IDC_CONFIRM_PWD), SW_HIDE);
        break;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDC_LOGIN_BTN)
        {
            wchar_t user[64], pass[64];
            GetDlgItemTextW(hWnd, IDC_USERNAME, user, 64);
            GetDlgItemTextW(hWnd, IDC_PASSWORD, pass, 64);
            if (wcslen(user) == 0 || wcslen(pass) == 0)
            {
                SetStatus(hWnd, L"请填写用户名和密码");
                return 0;
            }

            char userA[128], passA[128];
            WideCharToMultiByte(CP_UTF8, 0, user, -1, userA, 128, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, pass, -1, passA, 128, nullptr, nullptr);
            g_username = userA;
            g_password = passA;

            if (g_isLoginMode) {
                SetStatus(hWnd, L"正在登录...");
                EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_BTN), FALSE);
                std::thread t(DoLogin, hWnd);
                t.detach();
            } else {
                wchar_t confirm[64];
                GetDlgItemTextW(hWnd, IDC_CONFIRM_PWD, confirm, 64);
                if (wcscmp(pass, confirm) != 0) {
                    SetStatus(hWnd, L"两次密码不一致");
                    return 0;
                }
                SetStatus(hWnd, L"正在注册...");
                EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_BTN), FALSE);
                std::thread t(DoRegister, hWnd);
                t.detach();
            }
        }
        else if (id == IDC_SWITCH_BTN)
        {
            SwitchMode(hWnd);
        }
        break;
    }
    case WM_USER_LOGIN_RESULT:
    {
        EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_BTN), TRUE);
        const wchar_t* result = (const wchar_t*)lParam;
        if (wcscmp(result, L"ok") == 0)
        {
            SetStatus(hWnd, L"登录成功！正在进入主界面...");
            HWND hMain = CreateWindowExW(0, L"MyQQMainClass", L"MyQQ - 聊天",
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                CW_USEDEFAULT, CW_USEDEFAULT, 960, 680,
                nullptr, nullptr, g_hInst, nullptr);
            std::thread recvThread(RecvLoop, hMain);
            recvThread.detach();
            ShowWindow(hMain, SW_SHOW);
            UpdateWindow(hMain);
            DestroyWindow(hWnd);
        }
        else
        {
            SetStatus(hWnd, L"登录失败：用户名或密码错误");
            closesocket(g_sock);
            WSACleanup();
            g_sock = INVALID_SOCKET;
        }
        delete[] result;
        break;
    }
    case WM_USER_REG_RESULT:
    {
        EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_BTN), TRUE);
        const wchar_t* result = (const wchar_t*)lParam;
        if (wcscmp(result, L"ok") == 0) {
            SetStatus(hWnd, L"注册成功！现在可以登录了");
            SwitchMode(hWnd);
        } else if (wcscmp(result, L"exist") == 0) {
            SetStatus(hWnd, L"注册失败：该用户名已被占用");
        } else {
            SetStatus(hWnd, L"注册失败：服务器错误");
        }
        closesocket(g_sock);
        WSACleanup();
        g_sock = INVALID_SOCKET;
        delete[] result;
        break;
    }
    case WM_USER_CONN_ERROR:
    {
        EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_BTN), TRUE);
        SetStatus(hWnd, L"无法连接到服务器！请检查服务端是否开启");
        closesocket(g_sock);
        WSACleanup();
        g_sock = INVALID_SOCKET;
        delete[] (const wchar_t*)lParam;
        break;
    }
    case WM_DESTROY:
    {
        break;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void SwitchMode(HWND hDlg)
{
    g_isLoginMode = !g_isLoginMode;
    HWND hConfirm = GetDlgItem(hDlg, IDC_CONFIRM_PWD);
    HWND hBtn = GetDlgItem(hDlg, IDC_LOGIN_BTN);
    HWND hSwitchBtn = GetDlgItem(hDlg, IDC_SWITCH_BTN);
    SetDlgItemTextW(hDlg, IDC_USERNAME, L"");
    SetDlgItemTextW(hDlg, IDC_PASSWORD, L"");
    SetDlgItemTextW(hDlg, IDC_CONFIRM_PWD, L"");
    SetStatus(hDlg, L"");

    if (g_isLoginMode) {
        ShowWindow(hConfirm, SW_HIDE);
        SetWindowTextW(hBtn, L"登  录");
        SetWindowTextW(hSwitchBtn, L"没有账号？点击注册");
    } else {
        ShowWindow(hConfirm, SW_SHOW);
        SetWindowTextW(hBtn, L"注  册");
        SetWindowTextW(hSwitchBtn, L"已有账号？点击登录");
    }
}

void SetStatus(HWND hDlg, const wchar_t* text)
{
    SetDlgItemTextW(hDlg, IDC_STATUS_LBL, text);
}

void DoLogin(HWND hDlg)
{
    wchar_t* result = new wchar_t[32];
    g_sock = ConnectServer();
    if (g_sock == INVALID_SOCKET) {
        wcscpy_s(result, 32, L"conn_err");
        PostMessage(hDlg, WM_USER_CONN_ERROR, 0, (LPARAM)result);
        return;
    }
    char sendBuf[256];
    sprintf_s(sendBuf, "login|%s|%s", g_username.c_str(), g_password.c_str());
    send(g_sock, sendBuf, (int)strlen(sendBuf), 0);
    char recvBuf[32] = {0};
    int len = recv(g_sock, recvBuf, 31, 0);
    if (len <= 0) {
        wcscpy_s(result, 32, L"err");
        PostMessage(hDlg, WM_USER_LOGIN_RESULT, 0, (LPARAM)result);
        return;
    }
    recvBuf[len] = '\0';
    if (strcmp(recvBuf, "login_ok") == 0 || strcmp(recvBuf, "ok") == 0)
        wcscpy_s(result, 32, L"ok");
    else
        wcscpy_s(result, 32, L"fail");
    PostMessage(hDlg, WM_USER_LOGIN_RESULT, 0, (LPARAM)result);
}

void DoRegister(HWND hDlg)
{
    wchar_t* result = new wchar_t[32];
    g_sock = ConnectServer();
    if (g_sock == INVALID_SOCKET) {
        wcscpy_s(result, 32, L"conn_err");
        PostMessage(hDlg, WM_USER_CONN_ERROR, 0, (LPARAM)result);
        return;
    }
    char sendBuf[256];
    sprintf_s(sendBuf, "register|%s|%s", g_username.c_str(), g_password.c_str());
    send(g_sock, sendBuf, (int)strlen(sendBuf), 0);
    char recvBuf[32] = {0};
    int len = recv(g_sock, recvBuf, 31, 0);
    if (len <= 0) {
        wcscpy_s(result, 32, L"err");
        PostMessage(hDlg, WM_USER_REG_RESULT, 0, (LPARAM)result);
        return;
    }
    recvBuf[len] = '\0';
    if (strcmp(recvBuf, "ok") == 0)
        wcscpy_s(result, 32, L"ok");
    else if (strcmp(recvBuf, "exist") == 0)
        wcscpy_s(result, 32, L"exist");
    else
        wcscpy_s(result, 32, L"fail");
    PostMessage(hDlg, WM_USER_REG_RESULT, 0, (LPARAM)result);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        const int leftW = 220;
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;

        CreateWindowW(L"BUTTON", L"  好 友 列 表",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 8, 8, leftW - 4, h - 16,
            hWnd, nullptr, g_hInst, nullptr);

        HWND hFriendList = CreateWindowW(WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
            16, 30, leftW - 20, h - 60,
            hWnd, (HMENU)IDC_FRIEND_LIST, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(hFriendList, LVS_EX_FULLROWSELECT | LVS_EX_BORDERSELECT);
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_WIDTH;
        lvc.cx = leftW - 24;
        ListView_InsertColumn(hFriendList, 0, &lvc);
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0;
        lvi.pszText = (LPWSTR)L"  [公共聊天]";
        ListView_InsertItem(hFriendList, &lvi);

        HWND hChat = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL |
            ES_AUTOVSCROLL,
            leftW + 4, 8, w - leftW - 12, h - 90,
            hWnd, (HMENU)IDC_CHAT_DISPLAY, g_hInst, nullptr);
        InitRichEdit(hChat);

        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            leftW + 4, h - 62, w - leftW - 84, 54,
            hWnd, (HMENU)IDC_MSG_INPUT, g_hInst, nullptr);

        CreateWindowW(L"BUTTON", L"发 送", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 76, h - 62, 70, 54,
            hWnd, (HMENU)IDC_SEND_BTN, g_hInst, nullptr);

        wchar_t welcome[256];
        swprintf_s(welcome, 256, L"【系统】欢迎使用 MyQQ！已登录为 %hs\n", g_username.c_str());
        AddChatMsg(hChat, welcome, RGB(60, 60, 180));
        break;
    }
    case WM_SIZE:
    {
        const int leftW = 220;
        int w = LOWORD(lParam), h = HIWORD(lParam);
        SetWindowPos(GetDlgItem(hWnd, IDC_FRIEND_LIST), nullptr,
            16, 30, leftW - 20, h - 60, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hWnd, IDC_CHAT_DISPLAY), nullptr,
            leftW + 4, 8, w - leftW - 12, h - 90, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hWnd, IDC_MSG_INPUT), nullptr,
            leftW + 4, h - 62, w - leftW - 84, 54, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hWnd, IDC_SEND_BTN), nullptr,
            w - 76, h - 62, 70, 54, SWP_NOZORDER);
        InvalidateRect(hWnd, nullptr, TRUE);
        break;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDC_SEND_BTN) DoSendMessage(hWnd);
        break;
    }
    case WM_USER_RECV_MSG:
    {
        const wchar_t* msg = (const wchar_t*)lParam;
        if (msg && wcslen(msg) > 0) {
            // 解析 chat_msg|发送者|内容
            wchar_t* sender = nullptr;
            // 复制到可变缓冲区以便解析
            size_t msgLen = wcslen(msg);
            wchar_t* msgCopy = new wchar_t[msgLen + 1];
            wcscpy_s(msgCopy, msgLen + 1, msg);

            if (wcsncmp(msg, L"chat_msg|", 9) == 0) {
                wchar_t* rest = msgCopy + 9;
                wchar_t* pipe1 = wcschr(rest, L'|');
                if (pipe1) {
                    *pipe1 = L'\0';
                    sender = rest;
                    wchar_t* content = pipe1 + 1;
                    // 去除末尾换行
                    size_t clen = wcslen(content);
                    while (clen > 0 && (content[clen-1] == L'\n' || content[clen-1] == L'\r'))
                        content[--clen] = L'\0';

                    if (wcscmp(sender, L"系统") == 0) {
                        wchar_t display[2048];
                        swprintf_s(display, 2048, L"【系统】%s", content);
                        AddChatMsg(GetDlgItem(hWnd, IDC_CHAT_DISPLAY), display, RGB(120, 80, 180));
                    } else {
                        wchar_t display[2048];
                        swprintf_s(display, 2048, L"[%s] %s", sender, content);
                        AddChatMsg(GetDlgItem(hWnd, IDC_CHAT_DISPLAY), display, RGB(0, 0, 180));
                    }
                }
            } else {
                wchar_t display[2048];
                swprintf_s(display, 2048, L"[服务器] %s", msg);
                AddChatMsg(GetDlgItem(hWnd, IDC_CHAT_DISPLAY), display, RGB(0, 0, 0));
            }
            delete[] msg;
        }
        break;
    }
    case WM_USER_DISCONNECTED:
    {
        AddChatMsg(GetDlgItem(hWnd, IDC_CHAT_DISPLAY),
            L"【系统】与服务器的连接已断开", RGB(200, 50, 50));
        EnableWindow(GetDlgItem(hWnd, IDC_SEND_BTN), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDC_MSG_INPUT), FALSE);
        break;
    }
    case WM_DESTROY:
    {
        if (g_sock != INVALID_SOCKET) {
            closesocket(g_sock);
            WSACleanup();
            g_sock = INVALID_SOCKET;
        }
        PostQuitMessage(0);
        break;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void DoSendMessage(HWND hMain)
{
    HWND hInput = GetDlgItem(hMain, IDC_MSG_INPUT);
    wchar_t buf[1024];
    GetWindowTextW(hInput, buf, 1024);
    if (wcslen(buf) == 0) return;

    wchar_t display[2048];
    swprintf_s(display, 2048, L"[%hs] %s", g_username.c_str(), buf);
    AddChatMsg(GetDlgItem(hMain, IDC_CHAT_DISPLAY), display, RGB(0, 100, 0));
    SetWindowTextW(hInput, L"");
    SetFocus(hInput);

    char sendBuf[2048], msgA[1024];
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, msgA, 1024, nullptr, nullptr);
    if (g_sock != INVALID_SOCKET) {
        sprintf_s(sendBuf, "chat|%s|%s", g_username.c_str(), msgA);
        send(g_sock, sendBuf, (int)strlen(sendBuf), 0);
    }
}

void RecvLoop(HWND hMain)
{
    char buf[2048];
    while (g_sock != INVALID_SOCKET)
    {
        int len = recv(g_sock, buf, 2047, 0);
        if (len <= 0) {
            PostMessage(hMain, WM_USER_DISCONNECTED, 0, 0);
            break;
        }
        buf[len] = '\0';
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
        wchar_t* wbuf = new wchar_t[wlen];
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, wlen);
        PostMessage(hMain, WM_USER_RECV_MSG, 0, (LPARAM)wbuf);
    }
}

void InitRichEdit(HWND hRich)
{
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.crTextColor = RGB(30, 30, 30);
    cf.yHeight = 200;
    wcscpy_s(cf.szFaceName, L"Segoe UI");
    SendMessage(hRich, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SendMessage(hRich, EM_SETBKGNDCOLOR, 0, RGB(248, 248, 248));
    SendMessage(hRich, EM_SETEVENTMASK, 0, ENM_NONE);
}

void AddChatMsg(HWND hRich, const wchar_t* msg, COLORREF color)
{
    int nLen = GetWindowTextLengthW(hRich);
    SendMessage(hRich, EM_SETSEL, (WPARAM)nLen, (LPARAM)nLen);

    std::wstring full(msg);
    full += L"\r\n";
    SendMessage(hRich, EM_REPLACESEL, FALSE, (LPARAM)full.c_str());

    int newLen = GetWindowTextLengthW(hRich);
    SendMessage(hRich, EM_SETSEL, nLen, newLen);

    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR | CFM_BOLD;
    cf.crTextColor = color;
    cf.dwEffects = 0;
    if (wcsstr(msg, L"【系统】") != nullptr) cf.dwEffects = CFE_BOLD;
    SendMessage(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    SendMessage(hRich, EM_SETSEL, newLen, newLen);
    SendMessage(hRich, EM_SCROLL, SB_BOTTOM, 0);
}