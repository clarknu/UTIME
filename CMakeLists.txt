cmake_minimum_required(VERSION 3.10)
project(UTIME)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(src/sqlite)

set(SOURCES
    src/dllmain.cpp
    src/TextService.cpp
    src/Register.cpp
    src/EditSession.cpp
    src/CandidateWindow.cpp
    src/DictionaryEngine.cpp
    src/sqlite/sqlite3.c
    src/UTIME.def
)

set(HEADERS
    include/Globals.h
    include/TextService.h
    include/EditSession.h
    include/CandidateWindow.h
    include/DictionaryEngine.h
    include/GetTextExtEditSession.h
    include/sqlite/sqlite3.h
)

add_library(UTIME SHARED ${SOURCES} ${HEADERS})

target_link_libraries(UTIME
    PRIVATE
    user32
    ole32
    advapi32
    uuid
    gdi32
)

target_compile_definitions(UTIME PRIVATE
    UNICODE
    _UNICODE
    UTIME_EXPORTS
    _WINDOWS
    _USRDLL
)

# Copy utime.db to the output directory
# We'll use a post-build command to ensure it's copied to the same folder as the DLL
add_custom_command(TARGET UTIME POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/utime.db"
        "$<TARGET_FILE_DIR:UTIME>/utime.db"
)
