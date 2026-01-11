cmake_minimum_required(VERSION 3.10)
project(UTIME)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(UTIME SHARED
    src/dllmain.cpp
    src/TextService.cpp
    src/Register.cpp
    src/EditSession.cpp
    include/Globals.h
    include/TextService.h
    include/EditSession.h
)

target_link_libraries(UTIME
    PRIVATE
    user32
    ole32
    advapi32
    uuid
)

add_definitions(-DUNICODE -D_UNICODE)
