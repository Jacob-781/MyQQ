#ifndef CONNECT_H
#define CONNECT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"  // 本地调试，局域网改成服务端电脑IP
#define SERVER_PORT 8888

// 连接服务端，成功返回socket句柄，失败返回INVALID_SOCKET
SOCKET ConnectServer()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        std::cout << "Winsock初始化失败！" << std::endl;
        return INVALID_SOCKET;
    }

    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(clientSock == INVALID_SOCKET)
    {
        std::cout << "创建客户端Socket失败！" << std::endl;
        WSACleanup();
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    if(connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "无法连接服务端，请检查服务端是否开启、IP端口是否正确！" << std::endl;
        closesocket(clientSock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    return clientSock;
}

// 关闭套接字+清理网络库
void CloseSocket(SOCKET sock)
{
    closesocket(sock);
    WSACleanup();
}

#endif
