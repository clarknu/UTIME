#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "../../include/sqlite/sqlite3.h"

// Helper to split string
std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

// Process CEDICT pinyin: "ni3 hao3" -> "nihao", initials: "nh"
void ProcessPinyin(const std::string& rawPinyin, std::string& outClean, std::string& outInitials) {
    outClean = "";
    outInitials = "";
    
    std::vector<std::string> syllables = Split(rawPinyin, ' ');
    for (const auto& syl : syllables) {
        std::string cleanSyl;
        for (char c : syl) {
            if (isalpha(c)) {
                cleanSyl += tolower(c);
            } else if (c == ':') {
                // Handle u: -> v
                if (!cleanSyl.empty() && cleanSyl.back() == 'u') {
                    cleanSyl.back() = 'v';
                }
            }
        }
        
        if (!cleanSyl.empty()) {
            outClean += cleanSyl;
            outInitials += cleanSyl[0];
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: DictBuilder <cedict_ts.u8> <output.db>" << std::endl;
        return 1;
    }

    std::string dictPath = argv[1];
    std::string dbPath = argv[2];

    // Remove existing db
    remove(dbPath.c_str());

    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Create table with initials support
    // Priority: Default 0. We can adjust this later based on length or external freq data.
    sqlite3_exec(db, "CREATE TABLE lexicon (" \
                     "id INTEGER PRIMARY KEY AUTOINCREMENT," \
                     "hanzi TEXT NOT NULL," \
                     "pinyin_clean TEXT NOT NULL," \
                     "initials TEXT NOT NULL," \
                     "priority INTEGER DEFAULT 0);", 0, 0, 0);
    
    // Indexes for fast lookup
    sqlite3_exec(db, "CREATE INDEX idx_pinyin ON lexicon (pinyin_clean);", 0, 0, 0);
    sqlite3_exec(db, "CREATE INDEX idx_initials ON lexicon (initials);", 0, 0, 0);
    
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    std::ifstream file(dictPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << dictPath << std::endl;
        return 1;
    }

    std::string line;
    int count = 0;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "INSERT INTO lexicon (hanzi, pinyin_clean, initials, priority) VALUES (?, ?, ?, ?);", -1, &stmt, 0);

    std::cout << "Processing " << dictPath << "..." << std::endl;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Format: Traditional Simplified [pin1 yin1] /English/
        // Example: 你好 你好 [ni3 hao3] /Hello!/
        
        std::stringstream ss(line);
        std::string trad, simp, pinyinRaw;
        
        ss >> trad >> simp;
        
        // Extract [ ... ]
        size_t startBracket = line.find('[');
        size_t endBracket = line.find(']');
        
        if (startBracket != std::string::npos && endBracket != std::string::npos) {
            pinyinRaw = line.substr(startBracket + 1, endBracket - startBracket - 1);
            
            std::string clean, initials;
            ProcessPinyin(pinyinRaw, clean, initials);
            
            if (!clean.empty() && !simp.empty()) {
                // Priority heuristic: shorter words are more common? 
                // Let's give slight boost to shorter words (negative length as priority, so sorted DESC gives shorter first?)
                // Actually let's store 10 - length. So 1 char = 9, 2 chars = 8.
                int priority = 10 - (int)simp.length(); 
                if (priority < 0) priority = 0;

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, simp.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, clean.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, initials.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 4, priority);
                
                sqlite3_step(stmt);
                count++;
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    sqlite3_close(db);

    std::cout << "Generated " << dbPath << " with " << count << " records." << std::endl;
    return 0;
}
