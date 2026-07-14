#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "sqlite3.h"

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024
const int PORT = 8888;

// 保存局域网所有在线用户的通讯录映射：Username -> SOCKET 套接字句柄
std::map<std::string, SOCKET> online_users;
std::mutex users_mutex;

std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// SQLite 验证
bool IsUserExist(sqlite3* db, const std::string& username) {
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

bool RegisterUser(sqlite3* db, const std::string& username, const std::string& password) {
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

bool ValidateLogin(sqlite3* db, const std::string& username, const std::string& password) {
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

void HandleClient(SOCKET clientSock) {
    sqlite3* db = nullptr;
    if (sqlite3_open("myqq.db", &db) != SQLITE_OK) {
        closesocket(clientSock);
        return;
    }

    std::string logged_username = "";
    char buf[BUF_SIZE] = {0};

    while (true) {
        int len = recv(clientSock, buf, BUF_SIZE - 1, 0);
        if (len <= 0) {
            std::cout << "ℹ️ 连接断开。" << std::endl;
            break;
        }
        buf[len] = '\0';
        std::string req(buf);
        std::cout << "📩 收到封包: " << req << std::endl;

        std::vector<std::string> parts = SplitString(req, '|');
        if (parts.empty()) continue;

        std::string cmd = parts[0];

        if (cmd == "register" && parts.size() >= 3) {
            std::string username = parts[1];
            std::string password = parts[2];
            std::string reply = "fail";

            if (IsUserExist(db, username)) {
                reply = "exist";
            } else if (RegisterUser(db, username, password)) {
                reply = "ok";
            }
            send(clientSock, reply.c_str(), reply.length(), 0);
            break; // 注册完断开连接（短连接规范）
        }
        else if (cmd == "login" && parts.size() >= 3) {
            std::string username = parts[1];
            std::string password = parts[2];
            std::string reply = "fail";

            if (ValidateLogin(db, username, password)) {
                reply = "login_ok";
                logged_username = username;
                
                // 登记入在线映射，保持长连接
                std::lock_guard<std::mutex> lock(users_mutex);
                online_users[username] = clientSock;
                std::cout << "🟢 用户 " << username << " 已登记在线通讯录。" << std::endl;
            }
            send(clientSock, reply.c_str(), reply.length(), 0);
        }
        else if (cmd == "message" && parts.size() >= 4) {
            // 协议：message|发送者|接收者|内容
            std::string from_user = parts[1];
            std::string to_user = parts[2];
            std::string content = parts[3];

            // 存入 SQLite 消息表中
            const char* sql = "INSERT INTO Messages (FromUserId, ToUserId, Message, MessageTime, MessageState) "
                              "VALUES ((SELECT Id FROM Users WHERE NickName = ?), (SELECT Id FROM Users WHERE NickName = ?), ?, datetime('now', 'localtime'), 0);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, from_user.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, to_user.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmt);
            }
            sqlite3_finalize(stmt);

            // 实时路由投递消息给目标在线用户
            std::lock_guard<std::mutex> lock(users_mutex);
            auto it = online_users.find(to_user);
            if (it != online_users.end()) {
                std::string routeBuf = "msg|" + from_user + "|" + content;
                send(it->second, routeBuf.c_str(), routeBuf.length(), 0);
                std::cout << "⚡ 消息已实时路由给: " << to_user << std::endl;
            } else {
                std::cout << "😴 " << to_user << " 不在线，消息已安全存入 SQLite 未读数据库缓存。" << std::endl;
            }
        }
    }

    if (!logged_username.empty()) {
        std::lock_guard<std::mutex> lock(users_mutex);
        online_users.erase(logged_username);
        std::cout << "🔴 用户 " << logged_username << " 离开了在线映射。" << std::endl;
    }

    sqlite3_close(db);
    closesocket(clientSock);
}

int main() {
    system("CHCP 65001 > nul");
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return -1;
    if (listen(serverSock, 10) == SOCKET_ERROR) return -1;

    std::cout << "🟢 【MyQQ 多并发并发服务器】完全体启动，开始监听端口: " << PORT << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSock != INVALID_SOCKET) {
            std::thread clientThread(HandleClient, clientSock);
            clientThread.detach();
        }
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}