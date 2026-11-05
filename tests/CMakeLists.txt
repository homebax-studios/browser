# Tests for the platform-neutral core (addresses, settings, SQLite storage, tab model).
# They build on any OS:   cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/core_test
cmake_minimum_required(VERSION 3.20)
project(HBCoreTests CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../src/browser)
set(TP   ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp PATHS ${TP}/nlohmann-json/include ${TP}/nlohmann-json/single_include)
if(NOT JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found (thirdparty/nlohmann-json or -DJSON_INCLUDE_DIR=...)")
endif()

add_executable(core_test core_test.cpp
    ${CORE}/core/Util.cpp ${CORE}/core/Navigation.cpp ${CORE}/core/Settings.cpp
    ${CORE}/core/Storage.cpp ${CORE}/core/TabManager.cpp)
target_include_directories(core_test PRIVATE ${CORE} ${JSON_INCLUDE_DIR})

if(EXISTS ${TP}/sqlite/sqlite3.c)
    add_library(sqlite3_tp STATIC ${TP}/sqlite/sqlite3.c)
    target_include_directories(sqlite3_tp PUBLIC ${TP}/sqlite)
    target_link_libraries(core_test PRIVATE sqlite3_tp)
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(core_test PRIVATE SQLite::SQLite3)
endif()
