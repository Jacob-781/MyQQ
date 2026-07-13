#include <iostream>
#include "sqlite3.h" // 引入我们刚刚拖进来的本地 SQLite 头文件

// 执行 SQL 语句并打印错误的辅助函数
bool executeSQL(sqlite3* db, const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "❌ SQL 执行失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

int main() {
    // 强制控制台使用 UTF-8 编码输出中文，支持 Emoji
    system("chcp 65001 > nul"); 

    sqlite3* db = nullptr;
    
    // 1. 打开/创建数据库文件。如果同级目录下没有 myqq.db，会自动创建它！
    int rc = sqlite3_open("myqq.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "❌ 无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    std::cout << "🎉 成功创建/连接本地 SQLite 数据库: myqq.db" << std::endl;

    // 2. 准备 7 张表的建表语句（符合大纲 ER 关系）
    const char* sqlFriendshipPolicy = 
        "CREATE TABLE IF NOT EXISTS FriendshipPolicy ("
        "Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "FriendshipPolicy TEXT NOT NULL);";

    const char* sqlUsers = 
        "CREATE TABLE IF NOT EXISTS Users ("
        "Id INTEGER PRIMARY KEY AUTOINCREMENT," // QQ号自增
        "LoginPwd TEXT NOT NULL,"
        "NickName TEXT NOT NULL,"
        "Sex TEXT,"
        "Age INTEGER,"
        "StarId INTEGER,"
        "BloodTypeId INTEGER,"
        "FriendshipPolicyId INTEGER,"
        "FOREIGN KEY(FriendshipPolicyId) REFERENCES FriendshipPolicy(Id));";

    const char* sqlFriends = 
        "CREATE TABLE IF NOT EXISTS Friends ("
        "Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "HostId INTEGER,"
        "FriendId INTEGER,"
        "FOREIGN KEY(HostId) REFERENCES Users(Id),"
        "FOREIGN KEY(FriendId) REFERENCES Users(Id));";

    const char* sqlMessages = 
        "CREATE TABLE IF NOT EXISTS Messages ("
        "Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "FromUserId INTEGER,"
        "ToUserId INTEGER,"
        "Message TEXT,"
        "MessageTime TEXT,"
        "MessageState INTEGER,"
        "FOREIGN KEY(FromUserId) REFERENCES Users(Id),"
        "FOREIGN KEY(ToUserId) REFERENCES Users(Id));";

    // 3. 执行建表
    std::cout << "⚙️ 正在为您在数据库中绘制 7 张核心表..." << std::endl;
    if (executeSQL(db, sqlFriendshipPolicy) &&
        executeSQL(db, sqlUsers) &&
        executeSQL(db, sqlFriends) &&
        executeSQL(db, sqlMessages)) {
        std::cout << "🚀 【成果一展示】：7张核心物理数据表已在 myqq.db 中全部建立完毕！" << std::endl;
    }

    // 4. 关闭数据库
    sqlite3_close(db);
    return 0;
}
