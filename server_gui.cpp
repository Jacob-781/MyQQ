// MyQQ 服务端 - Win32 GUI 版 (支持聊天转发)
// 编译: g++ server_gui.cpp sqlite3.o -o MyQQServer.exe -mwindows -lws2_32 -lcomctl32 -static
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "sqlite3.h"

#pragma comment(lib, "ws2_32.lib")

#define WM_USER_LOG_MSG      (WM_USER + 1)
#define WM_USER_SRV_STARTED  (WM_USER + 2)
#define WM_USER_SRV_STOPPED  (WM_USER + 3)
#define IDC_LOG_LIST     301
#define IDC_START_BTN    302
#define IDC_PORT_EDIT    303
#define IDC_STATUS_LBL   304

HINSTANCE g_hInst = nullptr;
SOCKET g_serverSock = INVALID_SOCKET;
bool g_running = false;
int g_port = 8888;

// 在线客户端表: username -> socket
std::map<std::string, SOCKET> g_clients;
std::mutex g_clientsMutex;

LRESULT CALLBACK ServerWndProc(HWND, UINT, WPARAM, LPARAM);
void AcceptLoop(HWND hWnd);
void HandleClient(SOCKET clientSock, HWND hWnd, const std::string& clientIP);
void BroadcastMessage(const std::string& sender, const std::string& msg, SOCKET excludeSock);
void RemoveClient(const std::string& username, SOCKET sock);
void LogToUI(HWND hWnd, const wchar_t* text);
std::vector<std::string> SplitString(const std::string& str, char delimiter);
bool IsUserExist(sqlite3* db, const std::string& username);
bool RegisterUser(sqlite3* db, const std::string& username, const std::string& password);
bool ValidateLogin(sqlite3* db, const std::string& username, const std::string& password);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    g_hInst = hInst;
    InitCommonControls();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ServerWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MyQQServerClass";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"MyQQServerClass", L"MyQQ 服务端监控",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 540,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK ServerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"监听端口:", WS_CHILD | WS_VISIBLE,
            15, 15, 80, 24, hWnd, nullptr, g_hInst, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"8888",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
            95, 12, 70, 26, hWnd, (HMENU)IDC_PORT_EDIT, g_hInst, nullptr);
        CreateWindowW(L"BUTTON", L"启动服务", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, 10, 100, 30, hWnd, (HMENU)IDC_START_BTN, g_hInst, nullptr);
        CreateWindowW(L"STATIC", L"● 已停止", WS_CHILD | WS_VISIBLE,
            300, 14, 150, 24, hWnd, (HMENU)IDC_STATUS_LBL, g_hInst, nullptr);

        CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOX, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | LBS_NOINTEGRALHEIGHT,
            10, 50, 700, 440, hWnd, (HMENU)IDC_LOG_LIST, g_hInst, nullptr);

        LogToUI(hWnd, L"服务端程序已启动，正在自动监听...");
        // 自动启动服务
        PostMessage(hWnd, WM_COMMAND, IDC_START_BTN, 0);
        break;
    }
    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        SetWindowPos(GetDlgItem(hWnd, IDC_LOG_LIST), nullptr,
            10, 50, w - 20, h - 60, SWP_NOZORDER);
        break;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDC_START_BTN)
        {
            if (!g_running)
            {
                wchar_t portStr[16];
                GetDlgItemTextW(hWnd, IDC_PORT_EDIT, portStr, 16);
                g_port = _wtoi(portStr);
                if (g_port <= 0 || g_port > 65535) {
                    LogToUI(hWnd, L"端口号无效，请输入 1-65535");
                    break;
                }
                EnableWindow(GetDlgItem(hWnd, IDC_START_BTN), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDC_PORT_EDIT), FALSE);
                std::thread t(AcceptLoop, hWnd);
                t.detach();
            }
        }
        break;
    }
    case WM_USER_LOG_MSG:
    {
        const wchar_t* msg = (const wchar_t*)lParam;
        LogToUI(hWnd, msg);
        delete[] msg;
        break;
    }
    case WM_USER_SRV_STARTED:
    {
        LogToUI(hWnd, L" 服务端启动成功，等待客户端连接...");
        break;
    }
    case WM_USER_SRV_STOPPED:
    {
        g_running = false;
        g_serverSock = INVALID_SOCKET;
        SetDlgItemTextW(hWnd, IDC_STATUS_LBL, L"● 已停止");
        SetWindowTextW(GetDlgItem(hWnd, IDC_START_BTN), L"启动服务");
        EnableWindow(GetDlgItem(hWnd, IDC_PORT_EDIT), TRUE);
        LogToUI(hWnd, L"服务端已关闭");
        break;
    }
    case WM_DESTROY:
    {
        g_running = false;
        if (g_serverSock != INVALID_SOCKET) {
            closesocket(g_serverSock);
            WSACleanup();
        }
        PostQuitMessage(0);
        break;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void LogToUI(HWND hWnd, const wchar_t* msg)
{
    HWND hList = GetDlgItem(hWnd, IDC_LOG_LIST);
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[2048];
    swprintf_s(buf, 2048, L"[%02d:%02d:%02d] %s", st.wHour, st.wMinute, st.wSecond, msg);
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buf);
    int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
    SendMessage(hList, LB_SETTOPINDEX, count - 1, 0);
}

std::vector<std::string> SplitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

bool IsUserExist(sqlite3* db, const std::string& username)
{
    const char* sql = "SELECT Id FROM Users WHERE NickName = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool exist = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) exist = true;
    }
    sqlite3_finalize(stmt);
    return exist;
}

bool RegisterUser(sqlite3* db, const std::string& username, const std::string& password)
{
    const char* sql = "INSERT INTO Users (NickName, LoginPwd) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) success = true;
    }
    sqlite3_finalize(stmt);
    return success;
}

bool ValidateLogin(sqlite3* db, const std::string& username, const std::string& password)
{
    const char* sql = "SELECT Id FROM Users WHERE NickName = ? AND LoginPwd = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) success = true;
    }
    sqlite3_finalize(stmt);
    return success;
}

void RemoveClient(const std::string& username, SOCKET sock)
{
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    auto it = g_clients.find(username);
    if (it != g_clients.end()) {
        g_clients.erase(it);
    }
    // 也通过socket查找清理（针对未知用户名的情况）
    for (auto it = g_clients.begin(); it != g_clients.end(); ) {
        if (it->second == sock) it = g_clients.erase(it);
        else ++it;
    }
}

void BroadcastMessage(const std::string& sender, const std::string& msg, SOCKET excludeSock)
{
    // 格式: chat_msg|发送者|消息内容
    std::string packet = "chat_msg|" + sender + "|" + msg + "\n";
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    for (auto& pair : g_clients)
    {
        if (pair.second != excludeSock)
        {
            send(pair.second, packet.c_str(), (int)packet.size(), 0);
        }
    }
}

void HandleClient(SOCKET clientSock, HWND hWnd, const std::string& clientIP)
{
    char buf[2048];
    std::string loggedInUser;

    // 先接收第一条（注册或登录）
    int len = recv(clientSock, buf, 2047, 0);
    if (len <= 0) { closesocket(clientSock); return; }
    buf[len] = '\0';
    std::string req(buf);
    std::vector<std::string> parts = SplitString(req, '|');
    std::string reply = "fail";

    sqlite3* db = nullptr;
    sqlite3_open("myqq.db", &db);

    if (parts.size() >= 3 && db)
    {
        std::string cmd = parts[0];
        std::string username = parts[1];
        std::string password = parts[2];

        if (cmd == "register")
        {
            if (IsUserExist(db, username)) reply = "exist";
            else if (RegisterUser(db, username, password)) {
                reply = "ok";
                wchar_t log[256];
                swprintf_s(log, 256, L" 用户 %hs 注册成功 [%hs]", username.c_str(), clientIP.c_str());
                wchar_t* lp = new wchar_t[wcslen(log)+1];
                wcscpy_s(lp, wcslen(log)+1, log);
                PostMessage(hWnd, WM_USER_LOG_MSG, 0, (LPARAM)lp);
            }
            send(clientSock, reply.c_str(), (int)reply.length(), 0);
            sqlite3_close(db);
            closesocket(clientSock);
            return;
        }
        else if (cmd == "login")
        {
            if (ValidateLogin(db, username, password)) {
                reply = "login_ok";
                loggedInUser = username;
                // 加入在线列表
                {
                    std::lock_guard<std::mutex> lock(g_clientsMutex);
                    g_clients[username] = clientSock;
                }
                wchar_t log[256];
                swprintf_s(log, 256, L" 用户 %hs 登录成功 [%hs]", username.c_str(), clientIP.c_str());
                wchar_t* lp = new wchar_t[wcslen(log)+1];
                wcscpy_s(lp, wcslen(log)+1, log);
                PostMessage(hWnd, WM_USER_LOG_MSG, 0, (LPARAM)lp);

                // 广播上线通知
                BroadcastMessage("系统", username + " 上线了", clientSock);  // 排除自己，防止TCP粘包
            } else {
                reply = "fail";
                wchar_t log[256];
                swprintf_s(log, 256, L" 用户 %hs 登录失败 [%hs]", username.c_str(), clientIP.c_str());
                wchar_t* lp = new wchar_t[wcslen(log)+1];
                wcscpy_s(lp, wcslen(log)+1, log);
                PostMessage(hWnd, WM_USER_LOG_MSG, 0, (LPARAM)lp);
                send(clientSock, reply.c_str(), (int)reply.length(), 0);
                sqlite3_close(db);
                closesocket(clientSock);
                return;
            }
        }
    }
    sqlite3_close(db);

    // 发送登录结果
    send(clientSock, reply.c_str(), (int)reply.length(), 0);

    if (loggedInUser.empty()) {
        closesocket(clientSock);
        return;
    }

    // ====== 聊天消息循环 ======
    while (g_running)
    {
        int n = recv(clientSock, buf, 2047, 0);
        if (n <= 0) break;

        buf[n] = '\0';
        std::string data(buf);
        std::vector<std::string> msgParts = SplitString(data, '|');

        if (msgParts.size() >= 3 && msgParts[0] == "chat")
        {
            std::string sender = msgParts[1];
            // 重建消息内容（可能有多个|）
            std::string content;
            for (size_t i = 2; i < msgParts.size(); i++) {
                if (i > 2) content += "|";
                content += msgParts[i];
            }
            // 去除末尾换行
            while (!content.empty() && (content.back() == '\n' || content.back() == '\r'))
                content.pop_back();

            // 服务器日志
            wchar_t log[512];
            swprintf_s(log, 512, L"  %hs: %hs", sender.c_str(), content.c_str());
            wchar_t* lp = new wchar_t[wcslen(log)+1];
            wcscpy_s(lp, wcslen(log)+1, log);
            PostMessage(hWnd, WM_USER_LOG_MSG, 0, (LPARAM)lp);

            // 广播给所有其他客户端
            BroadcastMessage(sender, content, INVALID_SOCKET);
        }
    }

    // 用户断开
    RemoveClient(loggedInUser, clientSock);
    BroadcastMessage("系统", loggedInUser + " 下线了", INVALID_SOCKET);

    wchar_t log[256];
    swprintf_s(log, 256, L" 用户 %hs 断开连接", loggedInUser.c_str());
    wchar_t* lp = new wchar_t[wcslen(log)+1];
    wcscpy_s(lp, wcslen(log)+1, log);
    PostMessage(hWnd, WM_USER_LOG_MSG, 0, (LPARAM)lp);

    closesocket(clientSock);
}

void AcceptLoop(HWND hWnd)
{
    g_running = true;
    SetWindowTextW(GetDlgItem(hWnd, IDC_START_BTN), L"停止服务");
    SetDlgItemTextW(hWnd, IDC_STATUS_LBL, L" 正在监听");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        PostMessage(hWnd, WM_USER_SRV_STOPPED, 0, 0);
        return;
    }
    g_serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_serverSock == INVALID_SOCKET) { WSACleanup(); PostMessage(hWnd, WM_USER_SRV_STOPPED, 0, 0); return; }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)g_port);

    if (bind(g_serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(g_serverSock); WSACleanup(); g_serverSock = INVALID_SOCKET;
        PostMessage(hWnd, WM_USER_SRV_STOPPED, 0, 0); return;
    }
    if (listen(g_serverSock, 10) == SOCKET_ERROR) {
        closesocket(g_serverSock); WSACleanup(); g_serverSock = INVALID_SOCKET;
        PostMessage(hWnd, WM_USER_SRV_STOPPED, 0, 0); return;
    }

    PostMessage(hWnd, WM_USER_SRV_STARTED, 0, 0);

    while (g_running)
    {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(g_serverSock, (sockaddr*)&clientAddr, &addrLen);
        if (clientSock != INVALID_SOCKET)
        {
            char ip[INET_ADDRSTRLEN] = "unknown";
            inet_ntop(AF_INET, &clientAddr.sin_addr, ip, INET_ADDRSTRLEN);
            std::string clientIP(ip);
            std::thread t(HandleClient, clientSock, hWnd, clientIP);
            t.detach();
        }
    }

    // 清理所有在线客户端
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto& pair : g_clients) closesocket(pair.second);
        g_clients.clear();
    }
    closesocket(g_serverSock);
    WSACleanup();
    g_serverSock = INVALID_SOCKET;
    PostMessage(hWnd, WM_USER_SRV_STOPPED, 0, 0);
}