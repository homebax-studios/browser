cmake_minimum_required(VERSION 3.25)

project(HBBrowser VERSION 2.5.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "HB Browser (WebView2) is a Windows application. Other platforms will get their own front-end under platform/.")
endif()
enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------- third party
set(WEBVIEW2_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/webview2" CACHE PATH "WebView2 SDK (Microsoft.Web.WebView2 NuGet package, unpacked)")
set(WIL_ROOT      "${CMAKE_SOURCE_DIR}/thirdparty/wil"      CACHE PATH "Windows Implementation Library")
set(JSON_ROOT     "${CMAKE_SOURCE_DIR}/thirdparty/nlohmann-json" CACHE PATH "nlohmann/json")
set(SQLITE_ROOT   "${CMAKE_SOURCE_DIR}/thirdparty/sqlite"   CACHE PATH "SQLite amalgamation (sqlite3.c + sqlite3.h)")
set(SPDLOG_ROOT   "${CMAKE_SOURCE_DIR}/thirdparty/spdlog"   CACHE PATH "spdlog")

if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(WV2_ARCH arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WV2_ARCH x64)
else()
    set(WV2_ARCH x86)
endif()

find_path(WEBVIEW2_INCLUDE_DIR WebView2.h PATHS "${WEBVIEW2_ROOT}/build/native/include" NO_DEFAULT_PATH)
find_library(WEBVIEW2_LOADER_LIBRARY NAMES WebView2LoaderStatic
    PATHS "${WEBVIEW2_ROOT}/build/native/${WV2_ARCH}" NO_DEFAULT_PATH)
find_path(WIL_INCLUDE_DIR wil/com.h PATHS "${WIL_ROOT}/include" NO_DEFAULT_PATH)
find_path(JSON_INCLUDE_DIR nlohmann/json.hpp PATHS "${JSON_ROOT}/include" "${JSON_ROOT}/single_include" "${JSON_ROOT}" NO_DEFAULT_PATH)
find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h PATHS "${SPDLOG_ROOT}/include" NO_DEFAULT_PATH)
find_path(SQLITE_DIR sqlite3.c PATHS "${SQLITE_ROOT}" NO_DEFAULT_PATH)

foreach(dep
    WEBVIEW2_INCLUDE_DIR
    WEBVIEW2_LOADER_LIBRARY
    WIL_INCLUDE_DIR
    JSON_INCLUDE_DIR
    SPDLOG_INCLUDE_DIR
    SQLITE_DIR
)
    if(NOT DEFINED ${dep} OR "${${dep}}" STREQUAL "" OR "${${dep}}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR
            "${dep} not found!\n"
            "Check thirdparty folder structure.\n"
            "Expected path: ${${dep}}"
        )
    endif()

    message(STATUS "[HB Browser] Found ${dep}: ${${dep}}")
endforeach()

# SQLite is compiled from the amalgamation as a tiny static library.
add_library(sqlite3 STATIC "${SQLITE_DIR}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${SQLITE_DIR}")
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /w)
endif()

# ------------------------------------------------------------------ the app
configure_file(resources/manifest/HBBrowser.rc.in "${CMAKE_BINARY_DIR}/HBBrowser.rc" @ONLY)

add_executable(HBBrowser WIN32
    src/browser/main.cpp
    src/browser/BrowserApp.cpp
    src/browser/BrowserWindow.cpp
    src/browser/app/BrowserController.cpp
    src/browser/webview/WebViewManager.cpp
    src/browser/core/Util.cpp
    src/browser/core/Navigation.cpp
    src/browser/core/Settings.cpp
    src/browser/core/Storage.cpp
    src/browser/core/TabManager.cpp
    src/browser/core/Security.cpp
    platform/windows/WinUtil.cpp
    "${CMAKE_BINARY_DIR}/HBBrowser.rc"
)

target_include_directories(HBBrowser PRIVATE
    src/browser
    "${WEBVIEW2_INCLUDE_DIR}"
    "${WIL_INCLUDE_DIR}"
    "${JSON_INCLUDE_DIR}"
    "${SPDLOG_INCLUDE_DIR}"
)

target_compile_definitions(HBBrowser PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    SPDLOG_WCHAR_FILENAMES
    HB_VERSION="${PROJECT_VERSION}"
    _WIN32_WINNT=0x0A00
)

target_link_libraries(HBBrowser PRIVATE
    sqlite3
    "${WEBVIEW2_LOADER_LIBRARY}"
    user32 gdi32 ole32 shell32 shlwapi advapi32 dwmapi winhttp uuid
)

if(MSVC)
    target_compile_options(HBBrowser PRIVATE /W4 /permissive- /EHsc /utf-8 /GS $<$<NOT:$<CONFIG:Debug>>:/guard:cf>)
    # Exploit mitigations: ASLR, DEP, control-flow guard (not combinable with the Debug edit-and-continue mode).
    target_link_options(HBBrowser PRIVATE /DYNAMICBASE /NXCOMPAT $<$<NOT:$<CONFIG:Debug>>:/guard:cf>)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_options(HBBrowser PRIVATE /HIGHENTROPYVA)
    endif()
    # The manifest (DPI awareness, UTF-8, long paths) comes from the .rc file.
    target_link_options(HBBrowser PRIVATE /MANIFEST:NO)
endif()

set_target_properties(HBBrowser PROPERTIES
    OUTPUT_NAME "HB Browser"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# UI files are plain files next to the exe, so they can be edited without recompiling.
add_custom_command(TARGET HBBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:HBBrowser>/resources"
    COMMENT "Copying resources next to the executable"
)

# --------------------------------------------------- portable ZIP (no installer)
install(TARGETS HBBrowser RUNTIME DESTINATION .)
install(DIRECTORY resources DESTINATION . PATTERN "manifest" EXCLUDE)
install(FILES LICENSE DESTINATION . OPTIONAL)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "HB-Browser-${PROJECT_VERSION}-win-${WV2_ARCH}-portable")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
