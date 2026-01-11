#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "../../include/sqlite/sqlite3.h"

// Reuse sqlite3.c from src
// #include "../../src/sqlite/sqlite3.c"

// Helper to remove tones
std::string RemoveTones(const std::string& pinyin) {
    // Simplified tone mapping for UTF-8 input
    static const std::vector<std::pair<std::string, char>> toneMap = {
        {"ā", 'a'}, {"á", 'a'}, {"ǎ", 'a'}, {"à", 'a'},
        {"ē", 'e'}, {"é", 'e'}, {"ě", 'e'}, {"è", 'e'},
        {"ī", 'i'}, {"í", 'i'}, {"ǐ", 'i'}, {"ì", 'i'},
        {"ō", 'o'}, {"ó", 'o'}, {"ǒ", 'o'}, {"ò", 'o'},
        {"ū", 'u'}, {"ú", 'u'}, {"ǔ", 'u'}, {"ù", 'u'},
        {"ǖ", 'v'}, {"ǘ", 'v'}, {"ǚ", 'v'}, {"ǜ", 'v'}, {"ü", 'v'},
        {"n", 'n'}, {"g", 'g'}
    };

    std::string result;
    for (size_t i = 0; i < pinyin.length(); ++i) {
        unsigned char c = (unsigned char)pinyin[i];
        if (c < 128) {
            if (isalpha(c)) result += tolower(c);
        } else {
            bool found = false;
            for (const auto& pair : toneMap) {
                if (pinyin.substr(i).find(pair.first) == 0) {
                    result += pair.second;
                    i += pair.first.length() - 1;
                    found = true;
                    break;
                }
            }
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: DictBuilder <pinyin.txt> <output.db>" << std::endl;
        return 1;
    }

    std::string pinyinPath = argv[1];
    std::string dbPath = argv[2];

    // Remove existing db
    remove(dbPath.c_str());

    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    sqlite3_exec(db, "CREATE TABLE pinyin_map (id INTEGER PRIMARY KEY AUTOINCREMENT, pinyin TEXT NOT NULL, hanzi TEXT NOT NULL);", 0, 0, 0);
    sqlite3_exec(db, "CREATE INDEX idx_pinyin ON pinyin_map (pinyin);", 0, 0, 0);
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    std::ifstream file(pinyinPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << pinyinPath << std::endl;
        return 1;
    }

    std::string line;
    int count = 0;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "INSERT INTO pinyin_map (pinyin, hanzi) VALUES (?, ?);", -1, &stmt, 0);

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t colonPos = line.find(':');
        size_t hashPos = line.find('#');

        if (colonPos != std::string::npos && hashPos != std::string::npos) {
            std::string pinyinPart = line.substr(colonPos + 1, hashPos - colonPos - 1);
            std::string hanzi = line.substr(hashPos + 1);
            
            // Trim hanzi
            hanzi.erase(0, hanzi.find_first_not_of(" \t\r\n"));
            hanzi.erase(hanzi.find_last_not_of(" \t\r\n") + 1);

            std::stringstream ss(pinyinPart);
            std::string pinyinItem;
            while (std::getline(ss, pinyinItem, ',')) {
                // Trim pinyinItem
                pinyinItem.erase(0, pinyinItem.find_first_not_of(" \t\r\n"));
                pinyinItem.erase(pinyinItem.find_last_not_of(" \t\r\n") + 1);

                std::string cleanPinyin = RemoveTones(pinyinItem);
                if (!cleanPinyin.empty()) {
                    sqlite3_reset(stmt);
                    sqlite3_bind_text(stmt, 1, cleanPinyin.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, hanzi.c_str(), -1, SQLITE_STATIC);
                    sqlite3_step(stmt);
                    count++;
                }
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    sqlite3_close(db);

    std::cout << "Generated " << dbPath << " with " << count << " records." << std::endl;
    return 0;
}
