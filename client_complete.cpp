#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024
const std::string SERVER_IP = "192.168.43.73"; // 局域网联调时改成服务端的实际内网 IP
const int SERVER_PORT = 8888;

bool is_online = false;

// 负责在后台死循环“偷听”服务器发来的即时消息
void ReceiveThread(SOCKET sock) {
    char recvBuf[BUF_SIZE] = {0};
    while (is_online) {
        int len = recv(sock, recvBuf, BUF_SIZE - 1, 0);
        if (len > 0) {
            recvBuf[len] = '\0';
            std::string msg(recvBuf);
            
            // 收到即时聊天消息，直接打印出来
            if (msg.rfind("msg|", 0) == 0) {
                // 协议格式：msg|发送者|消息内容
                size_t first = msg.find('|');
                size_t second = msg.find('|', first + 1);
                std::string from_user = msg.substr(first + 1, second - first - 1);
                std::string content = msg.substr(second + 1);
                
                std::cout << "\n🔔 [新消息] " << from_user << " 说: " << content << std::endl;
                std::cout << "请输入操作或继续发送: ";
                std::cout.flush();
            } else {
                std::cout << "\n📢 [系统广播]: " << msg << std::endl;
            }
        } else {
            std::cout << "\n⚠️ 提示: 与中央服务器的连接断开。" << std::endl;
            is_online = false;
            break;
        }
    }
}

// 建立连接
SOCKET ConnectServer() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

int main() {
    system("CHCP 65001 > nul");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "❌ 初始化网络失败！" << std::endl;
        return -1;
    }

    int choice;
    SOCKET clientSock = INVALID_SOCKET;

    while (true) {
        std::cout << "\n=================================" << std::endl;
        std::cout << "        🌟 MyQQ 客户端 🌟        " << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << " 1. 注册新账号\n";
        std::cout << " 2. 登录已有账号\n";
        std::cout << " 0. 安全退出系统\n";
        std::cout << "---------------------------------" << std::endl;
        std::cout << "请选择操作 [0-2]: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(256, '\n');
            continue;
        }

        if (choice == 0) break;

        std::string username, password;

        if (choice == 1) {
            std::cout << "请输入注册用户名 (无空格): ";
            std::cin >> username;
            std::cout << "请输入注册密码 (无空格): ";
            std::cin >> password;

            SOCKET regSock = ConnectServer();
            if (regSock == INVALID_SOCKET) {
                std::cout << "❌ 连不上服务器！" << std::endl;
                continue;
            }

            std::string sendBuf = "register|" + username + "|" + password;
            send(regSock, sendBuf.c_str(), sendBuf.length(), 0);

            char temp[BUF_SIZE] = {0};
            int len = recv(regSock, temp, BUF_SIZE - 1, 0);
            if (len > 0) {
                temp[len] = '\0';
                std::string res(temp);
                if (res == "ok") {
                    std::cout << "✅ [注册成功] 快去登录吧！" << std::endl;
                } else {
                    std::cout << "❌ [注册失败] 账号已被占用或服务器报错！" << std::endl;
                }
            }
            closesocket(regSock);
        }
        else if (choice == 2) {
            std::cout << "请输入登录用户名: ";
            std::cin >> username;
            std::cout << "请输入登录密码: ";
            std::cin >> password;

            clientSock = ConnectServer();
            if (clientSock == INVALID_SOCKET) {
                std::cout << "❌ 连不上服务器！" << std::endl;
                continue;
            }

            std::string sendBuf = "login|" + username + "|" + password;
            send(clientSock, sendBuf.c_str(), sendBuf.length(), 0);

            char temp[BUF_SIZE] = {0};
            int len = recv(clientSock, temp, BUF_SIZE - 1, 0);
            if (len > 0) {
                temp[len] = '\0';
                std::string res(temp);
                if (res == "login_ok") {
                    std::cout << "✅ [登录成功] 您已成功上线！" << std::endl;
                    is_online = true;

                    // 启动后台“偷听”消息线程
                    std::thread t(ReceiveThread, clientSock);
                    t.detach();

                    // 进入即时聊天控制台子菜单
                    while (is_online) {
                        std::cout << "\n---------------------------------" << std::endl;
                        std::cout << "  📨 聊天面板（当前用户: " << username << "）" << std::endl;
                        std::cout << "  1. 发送消息给别的好友\n";
                        std::cout << "  0. 注销登录 (返回主菜单)\n";
                        std::cout << "---------------------------------" << std::endl;
                        std::cout << "请选择操作 [0-1]: ";
                        int subChoice;
                        std::cin >> subChoice;

                        if (subChoice == 0) {
                            is_online = false;
                            closesocket(clientSock);
                            clientSock = INVALID_SOCKET;
                            std::cout << "🚪 已安全注销账户。" << std::endl;
                            break;
                        } else if (subChoice == 1) {
                            std::string target_user, chat_text;
                            std::cout << "请输入对方用户名: ";
                            std::cin >> target_user;
                            std::cout << "请输入消息内容: ";
                            std::cin >> chat_text;

                            // 发送消息协议：message|发送者|接收者|消息内容
                            std::string msgBuf = "message|" + username + "|" + target_user + "|" + chat_text;
                            send(clientSock, msgBuf.c_str(), msgBuf.length(), 0);
                            std::cout << "📡 消息已发射给服务器！" << std::endl;
                        }
                    }
                } else {
                    std::cout << "❌ [登录失败] 用户名或密码错误！" << std::endl;
                    closesocket(clientSock);
                }
            }
        }
    }

    if (clientSock != INVALID_SOCKET) closesocket(clientSock);
    WSACleanup();
    return 0;
}