#include <iostream>
#include <winsock2.h> // Windows 网络库核心头文件
#include <ws2tcpip.h>
#include <string>

// 强行告诉链接器，我们在 Windows 下需要链接网络动态链接库
#pragma comment(lib, "ws2_32.lib")

int main() {
    // 强制控制台使用 UTF-8 编码输出中文，完美支持 Emoji
    system("chcp 65001 > nul");

    std::cout << "🚀 【MyQQ 服务端】正在初始化 Windows 网络环境..." << std::endl;

    // 1. 初始化 Winsock 系统组件
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "❌ 网络初始化失败！" << std::endl;
        return -1;
    }

    // 2. 创建 TCP 套接字 (Socket)
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "❌ 创建 Socket 失败！错误码: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    // 3. 绑定 IP 地址和端口 (允许局域网内任意内网 IP 连接测试，端口定为 8888)
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 监听本地所有网卡 (关键：便于局域网通信)
    serverAddr.sin_port = htons(8888);        // 端口号：8888

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "❌ 端口绑定失败！可能 8888 端口被占用了。错误码: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    // 4. 开始监听客户端的连接请求 (最大排队队列设为 10)
    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        std::cout << "❌ 监听失败！错误码: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    std::cout << "\n=========================================" << std::endl;
    std::cout << "🟢 【MyQQ Server】启动成功！正在运行中..." << std::endl;
    std::cout << "📍 正在监听局域网端口: 8888" << std::endl;
    std::cout << "💡 提示：按 Ctrl + C 可以随时关闭服务器" << std::endl;
    std::cout << "=========================================\n" << std::endl;

    std::cout << "⏳ 正在等待组员的客户端连接..." << std::endl;

    // 5. 阻塞等待客户端连接
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (clientSocket != INVALID_SOCKET) {
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "🎉 报告队长！发现新连接！来自组员 IP: " << clientIP << std::endl;
        
        // 简单打个招呼
        std::string welcomeMsg = "【MyQQ Server】: 恭喜！你已成功连接上队长的中央服务器！\n";
        send(clientSocket, welcomeMsg.c_str(), welcomeMsg.length(), 0);
        
        closesocket(clientSocket);
    }

    // 清理关闭
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}