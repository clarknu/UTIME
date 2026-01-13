#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "sqlite/sqlite3.h"

class CDictionaryEngine
{
public:
    static CDictionaryEngine& Instance();

    bool Initialize();
    std::vector<std::wstring> Query(const std::wstring& pinyin);

private:
    CDictionaryEngine();
    ~CDictionaryEngine();
    
    bool _CreateDatabase();

    sqlite3* _db;
    bool _isInitialized;
};
