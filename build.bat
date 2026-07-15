@echo off
REM ====== MyQQ 编译脚本 ======
REM 需要 MSYS2 MinGW-w64 (ucrt64) 环境

REM 1. 初始化数据库
echo 初始化数据库...
gcc -std=c11 -c sqlite3.c -o sqlite3.o
g++ -std=c++11 init_db.cpp sqlite3.o -o init_db.exe
init_db.exe

REM 2. 编译服务端 (GUI版 + 聊天转发)
echo 编译服务端...
g++ -std=c++11 server_gui.cpp sqlite3.o -o MyQQServer.exe -lws2_32 -lcomctl32 -mwindows

REM 3. 编译客户端 (GUI版)
echo 编译客户端...
g++ -std=c++11 client_gui.cpp -o MyQQClient.exe -lws2_32 -lcomctl32 -mwindows

echo 编译完成！
pause
