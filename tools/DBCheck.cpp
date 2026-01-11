#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <shlobj.h>
#include "sqlite3.h"

// Link with sqlite3.c in the build
#pragma comment(lib, "shell32.lib")

int main()
{
    // 1. Get AppData path
    TCHAR szPath[MAX_PATH];
    if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
    {
        std::cerr << "Failed to get AppData path" << std::endl;
        return 1;
    }

    std::wstring dbPath = szPath;
    dbPath += L"\\UTIME\\utime.db";
    
    char dbPathUtf8[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, dbPathUtf8, MAX_PATH * 3, NULL, NULL);

    std::cout << "Checking database at: " << dbPathUtf8 << std::endl;

    if (GetFileAttributes(dbPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::cerr << "Database file does not exist!" << std::endl;
        return 1;
    }

    sqlite3* db;
    int rc = sqlite3_open(dbPathUtf8, &db);
    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    std::cout << "Database opened successfully." << std::endl;

    // Check table structure
    const char* sql = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='lexicon';";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        if (count == 0) {
            std::cerr << "Table 'lexicon' does not exist!" << std::endl;
        } else {
            std::cout << "Table 'lexicon' exists." << std::endl;
        }
    }
    sqlite3_finalize(stmt);

    // Check record count
    sql = "SELECT count(*) FROM lexicon;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            std::cout << "Record count in lexicon: " << count << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to query count: " << sqlite3_errmsg(db) << std::endl;
    }

    // Test query
    sql = "SELECT hanzi FROM lexicon WHERE pinyin_clean LIKE 'ni%' LIMIT 5;";
    std::cout << "Test query 'ni%':" << std::endl;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            std::cout << " - " << text << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to test query: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_close(db);
    return 0;
}
