#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "sqlite3.h" // 链接本地数据库头文件

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024
const int PORT = 8888;

// 辅助函数：切分字符串（解析 register|username|password）
std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// 检查用户名在数据库中是否已存在
bool IsUserExist(sqlite3* db, const std::string& username) {
    const char* sql = "SELECT Id FROM Users WHERE NickName = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool exist = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exist = true;
        }
    }
    sqlite3_finalize(stmt);
    return exist;
}

// 注册新用户到数据库
bool RegisterUser(sqlite3* db, const std::string& username, const std::string& password) {
    const char* sql = "INSERT INTO Users (NickName, LoginPwd) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = true;
        }
    }
    sqlite3_finalize(stmt);
    return success;
}

// 验证用户登录
bool ValidateLogin(sqlite3* db, const std::string& username, const std::string& password) {
    const char* sql = "SELECT Id FROM Users WHERE NickName = ? AND LoginPwd = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            success = true;
        }
    }
    sqlite3_finalize(stmt);
    return success;
}

// 处理每个连入客户端的独立线程函数
void HandleClient(SOCKET clientSock) {
    // 1. 发送欢迎语，兼容客户端过滤欢迎语的机制
    std::string welcomeMsg = "【MyQQ Server】：恭喜！你已成功连接上队长的中央服务器！\n";
    send(clientSock, welcomeMsg.c_str(), welcomeMsg.length(), 0);

    // 2. 打开 SQLite 数据库句柄（每个线程独立打开，防止并发写冲突导致锁死）
    sqlite3* db = nullptr;
    if (sqlite3_open("myqq.db", &db) != SQLITE_OK) {
        std::cerr << "❌ 线程打开数据库失败！" << std::endl;
        closesocket(clientSock);
        return;
    }

    char buf[BUF_SIZE] = {0};
    while (true) {
        // 接收客户端业务数据包
        int len = recv(clientSock, buf, BUF_SIZE - 1, 0);
        if (len <= 0) {
            std::cout << "ℹ️ 客户端主动断开了连接。" << std::endl;
            break;
        }
        buf[len] = '\0';
        std::string req(buf);
        std::cout << "📩 收到封包: " << req << std::endl;

        // 解析网络协议
        std::vector<std::string> parts = SplitString(req, '|');
        if (parts.size() < 3) {
            std::string err = "protocol_error";
            send(clientSock, err.c_str(), err.length(), 0);
            continue;
        }

        std::string cmd = parts[0];
        std::string username = parts[1];
        std::string password = parts[2];
        std::string reply = "fail";

        // 执行对应的业务逻辑
        if (cmd == "register") {
            if (IsUserExist(db, username)) {
                reply = "exist";
                std::cout << "⚠️ 用户 " << username << " 已存在，拒绝注册！" << std::endl;
            } else {
                if (RegisterUser(db, username, password)) {
                    reply = "ok";
                    std::cout << "✅ 用户 " << username << " 注册成功并录入数据库！" << std::endl;
                }
            }
            // 注册属于短连接，处理完直接返回并断开
            send(clientSock, reply.c_str(), reply.length(), 0);
            break; 
        } 
        else if (cmd == "login") {
            if (ValidateLogin(db, username, password)) {
                reply = "login_ok";
                std::cout << "🔓 用户 " << username << " 密码验证通过，登录成功！" << std::endl;
                send(clientSock, reply.c_str(), reply.length(), 0);
                // 登录属于长连接，保持 Socket 通道通畅，以便后续进行聊天消息推送
                // 在这里可以继续循环等待接收该用户的聊天指令...
            } else {
                reply = "fail";
                std::cout << "❌ 用户 " << username << " 登录验证失败！" << std::endl;
                send(clientSock, reply.c_str(), reply.length(), 0);
                break; // 登录失败断开连接
            }
        }
    }

    sqlite3_close(db);
    closesocket(clientSock);
}

int main() {
    system("CHCP 65001 > nul");
    std::cout << "⚙️ 正在初始化 TCP 网络协议栈..." << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ 网络库初始化失败！" << std::endl;
        return -1;
    }

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock == INVALID_SOCKET) {
        std::cerr << "❌ 无法创建监听套接字！" << std::endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // 自动绑定本地所有的局域网网卡
    addr.sin_port = htons(PORT);

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "❌ 端口 " << PORT << " 绑定失败！可能被占用。" << std::endl;
        closesocket(serverSock);
        WSACleanup();
        return -1;
    }

    if (listen(serverSock, 10) == SOCKET_ERROR) {
        std::cerr << "❌ 开启端口监听失败！" << std::endl;
        closesocket(serverSock);
        WSACleanup();
        return -1;
    }

    std::cout << "\n=============================================" << std::endl;
    std::cout << "🟢 【MyQQ 多并发并发服务器】成功启动，开始死守！" << std::endl;
    std::cout << "📍 正在稳定监听端口: " << PORT << std::endl;
    std::cout << "📁 数据库后台绑定: myqq.db" << std::endl;
    std::cout << "=============================================\n" << std::endl;

    // 循环死等客户端连入
    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSock != INVALID_SOCKET) {
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::cout << "🔔 发现连接！局域网内有机器接入，IP: " << clientIP << std::endl;

            // 关键：每连入一个客户端，启动一个后台线程专门伺候，主线程立刻回去继续等连接
            std::thread clientThread(HandleClient, clientSock);
            clientThread.detach(); // 脱离主线程独立运行，防止卡死
        }
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}