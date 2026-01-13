#include "DictionaryEngine.h"
#include "Globals.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <vector>
#include <string>

// ---------------------------------------------------------
// Smart Correction & Fuzzy Logic Helpers
// ---------------------------------------------------------

// 1. Auto-Correction: Fix common typos
std::string AutoCorrect(std::string input) {
    static const std::vector<std::pair<std::string, std::string>> corrections = {
        {"ign", "ing"},  {"img", "ing"},
        {"uen", "un"},   {"iou", "iu"},
        {"uei", "ui"},   {"v", "u"} 
    };
    for (const auto& pair : corrections) {
        size_t pos = 0;
        while ((pos = input.find(pair.first, pos)) != std::string::npos) {
            input.replace(pos, pair.first.length(), pair.second);
            pos += pair.second.length();
        }
    }
    return input;
}

// 2. Fuzzy Expansion: Generate variants (z<->zh, l<->n, etc.)
std::vector<std::string> GetFuzzyList(const std::string& input) {
    std::string corrected = AutoCorrect(input);
    std::set<std::string> variants;
    variants.insert(corrected);

    // Simple single-pass fuzzy expansion for performance
    // Focusing on initials and common confused sounds
    std::string temp = corrected;
    
    // z <-> zh
    if (temp.find("zh") == 0) variants.insert("z" + temp.substr(2));
    else if (temp.find("z") == 0) variants.insert("zh" + temp.substr(1));
    
    // c <-> ch
    if (temp.find("ch") == 0) variants.insert("c" + temp.substr(2));
    else if (temp.find("c") == 0) variants.insert("ch" + temp.substr(1));
    
    // s <-> sh
    if (temp.find("sh") == 0) variants.insert("s" + temp.substr(2));
    else if (temp.find("s") == 0) variants.insert("sh" + temp.substr(1));
    
    // l <-> n
    if (temp.find("n") == 0) variants.insert("l" + temp.substr(1));
    else if (temp.find("l") == 0) variants.insert("n" + temp.substr(1));
    
    // ing <-> in
    if (temp.length() > 3 && temp.substr(temp.length()-3) == "ing") 
        variants.insert(temp.substr(0, temp.length()-1)); // ing -> in
    else if (temp.length() > 2 && temp.substr(temp.length()-2) == "in")
        variants.insert(temp + "g"); // in -> ing

    return std::vector<std::string>(variants.begin(), variants.end());
}


// Use UTF-8 literal strings directly
// #define U8(x) u8##x

// Helper to remove tones from pinyin (e.g., "hǎo" -> "hao")
std::string RemoveTones(const std::string& pinyin) {
    std::string result;
    // Simple tone mapping (this is a simplified version, robust implementation would map all unicode tone chars)
    // Since input is UTF-8, we need to handle multibyte chars.
    // However, for this specific task, we can use a simpler approach:
    // Only keep a-z chars if we assume user only types a-z.
    // But wait, the input file has unicode tones (ā, á, ǎ, à...).
    // Let's use a brute-force mapping for common pinyin tone vowels.
    
    // Detailed mapping for Pinyin vowels with tones to ASCII vowels
    static const std::vector<std::pair<std::string, char>> toneMap = {
        {"ā", 'a'}, {"á", 'a'}, {"ǎ", 'a'}, {"à", 'a'},
        {"ē", 'e'}, {"é", 'e'}, {"ě", 'e'}, {"è", 'e'},
        {"ī", 'i'}, {"í", 'i'}, {"ǐ", 'i'}, {"ì", 'i'},
        {"ō", 'o'}, {"ó", 'o'}, {"ǒ", 'o'}, {"ò", 'o'},
        {"ū", 'u'}, {"ú", 'u'}, {"ǔ", 'u'}, {"ù", 'u'},
        {"ǖ", 'v'}, {"ǘ", 'v'}, {"ǚ", 'v'}, {"ǜ", 'v'}, {"ü", 'v'},
        {"n", 'n'}, {"g", 'g'} // just to be safe
    };

    for (size_t i = 0; i < pinyin.length(); ++i) {
        unsigned char c = (unsigned char)pinyin[i];
        if (c < 128) {
            if (isalpha(c)) result += tolower(c);
        } else {
            // Multibyte character, check if it is a tone vowel
            bool found = false;
            for (const auto& pair : toneMap) {
                if (pinyin.substr(i).find(pair.first) == 0) {
                    result += pair.second;
                    i += pair.first.length() - 1;
                    found = true;
                    break;
                }
            }
            if (!found) {
               // Skip unknown multibyte chars (or keep them? let's skip for pure pinyin)
               // Actually, for safety, let's just skip until next ascii if we don't recognize
            }
        }
    }
    return result;
}

CDictionaryEngine& CDictionaryEngine::Instance()
{
    static CDictionaryEngine instance;
    return instance;
}

CDictionaryEngine::CDictionaryEngine() : _db(NULL), _isInitialized(false)
{
}

CDictionaryEngine::~CDictionaryEngine()
{
    if (_db)
    {
        sqlite3_close(_db);
        _db = NULL;
    }
}

bool CDictionaryEngine::Initialize()
{
    if (_isInitialized) return true;

    DebugLog(L"DictionaryEngine::Initialize starting...");

    // Build candidate paths list
    std::vector<std::pair<std::wstring, bool>> candidatePaths; // path, isWritable
    
    // Path 1: AppData\UTIME\utime.db (read-write)
    TCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
    {
        std::wstring appDataPath = szPath;
        appDataPath += L"\\UTIME\\utime.db";
        candidatePaths.push_back(std::make_pair(appDataPath, true));
        DebugLog(L"Candidate path 1 (AppData): %s", appDataPath.c_str());
    }
    else
    {
        DebugLog(L"SHGetFolderPath failed, skipping AppData path");
    }
    
    // Path 2: DLL directory\utime.db (read-only)
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileName(g_hInst, dllPath, MAX_PATH))
    {
        std::wstring dllDirPath = dllPath;
        size_t lastSlash = dllDirPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            dllDirPath = dllDirPath.substr(0, lastSlash);
            dllDirPath += L"\\utime.db";
            candidatePaths.push_back(std::make_pair(dllDirPath, false));
            DebugLog(L"Candidate path 2 (DLL dir): %s", dllDirPath.c_str());
        }
    }
    
    // Path 3: Windows\Temp\UTIME\utime.db (read-write)
    TCHAR szTempPath[MAX_PATH];
    if (GetTempPath(MAX_PATH, szTempPath))
    {
        std::wstring tempPath = szTempPath;
        tempPath += L"UTIME\\utime.db";
        candidatePaths.push_back(std::make_pair(tempPath, true));
        DebugLog(L"Candidate path 3 (Temp): %s", tempPath.c_str());
    }

    // Try each path
    for (size_t i = 0; i < candidatePaths.size(); ++i)
    {
        std::wstring dbPath = candidatePaths[i].first;
        bool isWritable = candidatePaths[i].second;
        
        DebugLog(L"Trying path %d: %s (writable=%d)", i + 1, dbPath.c_str(), isWritable);
        
        // Check if file exists
        DWORD attr = GetFileAttributes(dbPath.c_str());
        bool fileExists = (attr != INVALID_FILE_ATTRIBUTES);
        
        if (!fileExists && isWritable)
        {
            // Try to copy from DLL directory
            DebugLog(L"Database not found, attempting to copy from DLL dir");
            
            // Get source path (DLL directory)
            wchar_t srcDllPath[MAX_PATH];
            if (GetModuleFileName(g_hInst, srcDllPath, MAX_PATH))
            {
                std::wstring sourcePath = srcDllPath;
                size_t lastSlash = sourcePath.find_last_of(L"\\/");
                if (lastSlash != std::wstring::npos)
                {
                    sourcePath = sourcePath.substr(0, lastSlash);
                    sourcePath += L"\\utime.db";
                    
                    DebugLog(L"Source DB path: %s", sourcePath.c_str());
                    
                    // Create directory if needed
                    std::wstring dirPath = dbPath.substr(0, dbPath.find_last_of(L"\\/"));
                    CreateDirectory(dirPath.c_str(), NULL);
                    
                    if (!CopyFile(sourcePath.c_str(), dbPath.c_str(), FALSE))
                    {
                        DebugLog(L"Failed to copy database, error=%d", GetLastError());
                        continue; // Try next path
                    }
                    else
                    {
                        DebugLog(L"Database copied successfully");
                        fileExists = true;
                    }
                }
            }
        }
        
        if (fileExists)
        {
            // Try to open database
            char dbPathUtf8[MAX_PATH * 3];
            int ret = WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, dbPathUtf8, MAX_PATH * 3, NULL, NULL);
            if (ret == 0)
            {
                DebugLog(L"Failed to convert path to UTF-8, error=%d", GetLastError());
                continue;
            }
            
            int rc = sqlite3_open(dbPathUtf8, &_db);
            if (rc != SQLITE_OK)
            {
                DebugLog(L"Failed to open database: %S", sqlite3_errmsg(_db));
                sqlite3_close(_db);
                _db = NULL;
                continue;
            }
            
            DebugLog(L"Database opened successfully");
            
            // Verify table structure
            sqlite3_stmt* stmt;
            const char* testQuery = "SELECT COUNT(*) FROM lexicon LIMIT 1;";
            rc = sqlite3_prepare_v2(_db, testQuery, -1, &stmt, 0);
            if (rc == SQLITE_OK)
            {
                rc = sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                if (rc == SQLITE_ROW || rc == SQLITE_DONE)
                {
                    DebugLog(L"Database structure validated");
                    _isInitialized = true;
                    return true;
                }
                else
                {
                    DebugLog(L"Database validation failed: %S", sqlite3_errmsg(_db));
                }
            }
            else
            {
                DebugLog(L"Failed to prepare validation query: %S", sqlite3_errmsg(_db));
            }
            
            // If validation failed, close and try next path
            sqlite3_close(_db);
            _db = NULL;
        }
    }
    
    DebugLog(L"All database paths failed");
    return false;
}

// _LoadFromTextFile and _InsertRecord are removed/unused now.
// bool CDictionaryEngine::_InsertRecord(const char* pinyin, const char* hanzi)
// {
//     // Deprecated
//     return false;
// }

std::vector<std::wstring> CDictionaryEngine::Query(const std::wstring& pinyin)
{
    std::vector<std::wstring> results;
    if (!_db || pinyin.empty()) 
    {
        DebugLog(L"Query: Database not initialized or pinyin empty");
        return results;
    }

    // Convert pinyin to UTF-8 and lowercase
    char pinyinUtf8[128];
    int ret = WideCharToMultiByte(CP_UTF8, 0, pinyin.c_str(), -1, pinyinUtf8, 128, NULL, NULL);
    if (ret == 0)
    {
        DebugLog(L"Query: Failed to convert pinyin to UTF-8, error=%d", GetLastError());
        return results;
    }
    
    std::string inputRaw = pinyinUtf8;
    std::transform(inputRaw.begin(), inputRaw.end(), inputRaw.begin(), ::tolower);
    
    DebugLog(L"Query: Input pinyin='%s', UTF-8='%S'", pinyin.c_str(), inputRaw.c_str());

    // Get all fuzzy variants (including auto-corrected)
    std::vector<std::string> searchKeys = GetFuzzyList(inputRaw);
    
    // Limit variants to 5 for performance
    if (searchKeys.size() > 5)
    {
        searchKeys.resize(5);
    }
    
    DebugLog(L"Query: Generated %d fuzzy variants", searchKeys.size());
    for (size_t i = 0; i < searchKeys.size() && i < 3; ++i)
    {
        DebugLog(L"  Variant %d: %S", i, searchKeys[i].c_str());
    }
    
    // Build Dynamic SQL
    std::string sql = "SELECT hanzi FROM lexicon WHERE ";
    for (size_t i = 0; i < searchKeys.size(); ++i) {
        if (i > 0) sql += " OR ";
        sql += "(pinyin_clean LIKE ? OR initials LIKE ?)";
    }
    sql += " ORDER BY length(pinyin_clean) ASC, priority DESC LIMIT 20;";
    
    DebugLog(L"Query: SQL='%S'", sql.c_str());

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK)
    {
        // Bind parameters with SQLITE_TRANSIENT to let SQLite copy the strings
        int bindIdx = 1;
        for (size_t i = 0; i < searchKeys.size(); ++i) {
            std::string likePattern = searchKeys[i] + "%";
            // Use SQLITE_TRANSIENT to force SQLite to make a copy
            sqlite3_bind_text(stmt, bindIdx++, likePattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, bindIdx++, likePattern.c_str(), -1, SQLITE_TRANSIENT);
        }
        
        DebugLog(L"Query: Bound %d parameters", bindIdx - 1);
        
        std::set<std::wstring> seen;
        int rowCount = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text)
            {
                wchar_t hanziW[128];
                MultiByteToWideChar(CP_UTF8, 0, (const char*)text, -1, hanziW, 128);
                if (seen.find(hanziW) == seen.end()) {
                    results.push_back(hanziW);
                    seen.insert(hanziW);
                    rowCount++;
                }
            }
        }
        sqlite3_finalize(stmt);
        
        DebugLog(L"Query: Found %d candidates", rowCount);
        for (size_t i = 0; i < results.size() && i < 3; ++i)
        {
            DebugLog(L"  Result %d: %s", i, results[i].c_str());
        }
    }
    else
    {
        DebugLog(L"Query: SQL prepare failed: %S", sqlite3_errmsg(_db));
    }

    return results;
}
