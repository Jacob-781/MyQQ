#include <iostream>
#include "connect.h"

#define BUF_SIZE 1024

// 发送数据并接收服务端返回结果
std::string SendAndRecv(SOCKET sock, const char* sendData)
{
    char recvBuf[BUF_SIZE] = {0};
    send(sock, sendData, strlen(sendData), 0);
    recv(sock, recvBuf, BUF_SIZE, 0);
    return std::string(recvBuf);
}

int main()
{
    system("CHCP 65001 > nul");
    int choice;

    while(true)
    {
        std::cout << "\n===== MyQQ客户端 =====\n";
        std::cout << "1. 注册账号\n";
        std::cout << "2. 登录账号\n";
        std::cout << "0. 退出程序\n";
        std::cout << "请输入选项：";
        std::cin >> choice;

        if(choice == 0) break;

        // 每次操作都重新连接服务端（简单方案）
        SOCKET sock = ConnectServer();
        if(sock == INVALID_SOCKET) continue;

        char username[64], password[64];
        char sendBuf[BUF_SIZE] = {0};

        if(choice == 1)
        {
            std::cout << "请输入注册用户名：";
            std::cin >> username;
            std::cout << "请输入密码：";
            std::cin >> password;

            // 拼接协议：register|账号|密码
            sprintf_s(sendBuf, "register|%s|%s", username, password);
            std::string res = SendAndRecv(sock, sendBuf);

            if(res == "ok")
                std::cout << "✅ 注册成功！" << std::endl;
            else if(res == "exist")
                std::cout << "❌ 账号已被占用！" << std::endl;
        }
        else if(choice == 2)
        {
            std::cout << "请输入登录用户名：";
            std::cin >> username;
            std::cout << "请输入密码：";
            std::cin >> password;

            // 拼接协议：login|账号|密码
            sprintf_s(sendBuf, "login|%s|%s", username, password);
            std::string res = SendAndRecv(sock, sendBuf);

            if(res == "login_ok")
                std::cout << "✅ 登录成功！" << std::endl;
            else if(res == "fail")
                std::cout << "❌ 用户名或密码错误！" << std::endl;
        }

        CloseSocket(sock);
    }

    std::cout << "客户端已退出" << std::endl;
    return 0;
}
