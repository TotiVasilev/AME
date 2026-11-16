cmake_minimum_required(VERSION 3.15)

project(AME VERSION 0.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)

target_include_directories(sqlite3 PUBLIC
    third_party/sqlite
)

add_executable(AME WIN32
    src/ui/main.cpp
    src/ui/accounting_window.cpp
    src/ui/companies_window.cpp
    src/data/database.cpp
    src/data/company.cpp
)

target_compile_definitions(AME PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_compile_options(AME PRIVATE /utf-8)

target_link_libraries(AME PRIVATE
    sqlite3
    user32
    gdi32
    shell32
)