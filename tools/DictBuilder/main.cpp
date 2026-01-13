#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>
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
        for (unsigned char c : syl) {
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
            outInitials += (char)cleanSyl[0];
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
        // Try absolute path or check current directory
        char buf[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buf);
        std::cerr << "Current Directory: " << buf << std::endl;
        return 1;
    }

    std::string line;
    int count = 0;
    int line_count = 0;
    sqlite3_stmt* stmt = nullptr;
    int rc_prep = sqlite3_prepare_v2(db, "INSERT INTO lexicon (hanzi, pinyin_clean, initials, priority) VALUES (?, ?, ?, ?);", -1, &stmt, 0);
    if (rc_prep != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    std::cout << "Processing " << dictPath << "..." << std::endl;

    try {
        while (std::getline(file, line)) {
            line_count++;
            if (line.empty() || line[0] == '#') continue;

            if (line_count % 1000 == 0) {
                std::cout << "Read " << line_count << " lines..." << std::endl;
            }

            // Format: Traditional Simplified [pin1 yin1] /English/
            // Example: 你好 你好 [ni3 hao3] /Hello!/
            
            std::stringstream ss(line);
            std::string trad, simp, pinyinRaw;
            
            if (!(ss >> trad >> simp)) {
                std::cerr << "Failed to parse trad/simp at line " << line_count << ": " << line << std::endl;
                continue;
            }
            
            // Extract [ ... ]
            size_t startBracket = line.find('[');
            size_t endBracket = line.find(']');
            
            if (startBracket != std::string::npos && endBracket != std::string::npos) {
                if (endBracket <= startBracket) {
                    std::cerr << "Invalid brackets at line " << line_count << ": " << line << std::endl;
                    continue;
                }
                pinyinRaw = line.substr(startBracket + 1, endBracket - startBracket - 1);
                
                std::string clean, initials;
                ProcessPinyin(pinyinRaw, clean, initials);
                
                if (!clean.empty() && !simp.empty()) {
                    // Priority heuristic: shorter words are more common? 
                    int priority = 10 - (int)simp.length(); 
                    if (priority < 0) priority = 0;

                    sqlite3_reset(stmt);
                    sqlite3_bind_text(stmt, 1, simp.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 2, clean.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 3, initials.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt, 4, priority);
                    
                    int rc_step = sqlite3_step(stmt);
                    if (rc_step != SQLITE_DONE) {
                        std::cerr << "Failed to insert record at line " << line_count << ": " << sqlite3_errmsg(db) << " (rc=" << rc_step << ")" << std::endl;
                    }
                    count++;
                    if (count % 10000 == 0) {
                        std::cout << "Inserted " << count << " records (total lines: " << line_count << ")..." << std::endl;
                    }
                }
            } else {
                // Some lines might not have brackets, that's fine for CEDICT if it's a comment but we already skipped #
                // But let's log if it's unexpected
                if (line.find('/') != std::string::npos) {
                    std::cerr << "Missing brackets in dictionary entry at line " << line_count << ": " << line << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred during processing: " << e.what() << " at line " << line_count << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred at line " << line_count << std::endl;
        return 1;
    }

    std::cout << "Finished reading file. Total lines: " << line_count << ", Total records found: " << count << std::endl;

    sqlite3_finalize(stmt);
    int rc_commit = sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    if (rc_commit != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);

    std::cout << "Generated " << dbPath << " with " << count << " records." << std::endl;
    return 0;
}
