#include "sqlite3.h"
#include <iostream>

int main() {
    sqlite3* db;
    if (sqlite3_open("steam.db", &db) == SQLITE_OK) {
        std::cout << "✅ SQLite connected successfully!\n";
        sqlite3_close(db);
    } else {
        std::cerr << "❌ Failed to connect to SQLite.\n";
    }
    return 0;
}
