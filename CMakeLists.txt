cmake_minimum_required(VERSION 3.25)

project(HBBrowser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WEBVIEW2_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/webview2" CACHE PATH "WebView2 SDK directory")
set(WIL_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/wil" CACHE PATH "WIL directory")

find_path(WEBVIEW2_INCLUDE_DIR WebView2.h
    PATHS "${WEBVIEW2_ROOT}/build/native/include"
    NO_DEFAULT_PATH
)
find_path(WIL_INCLUDE_DIR wil/com.h
    PATHS "${WIL_ROOT}/include"
    NO_DEFAULT_PATH
)
find_library(WEBVIEW2_LOADER_LIBRARY
    NAMES WebView2LoaderStatic WebView2Loader.dll
    PATHS
        "${WEBVIEW2_ROOT}/build/native/x64"
        "${WEBVIEW2_ROOT}/build/native/arm64"
        "${WEBVIEW2_ROOT}/build/native"
    NO_DEFAULT_PATH
)

if(NOT WEBVIEW2_INCLUDE_DIR)
    message(FATAL_ERROR "WebView2.h not found. Check thirdparty/webview2/build/native/include.")
endif()
if(NOT WIL_INCLUDE_DIR)
    message(FATAL_ERROR "wil/com.h not found. Check thirdparty/wil/include.")
endif()
if(NOT WEBVIEW2_LOADER_LIBRARY)
    message(FATAL_ERROR "WebView2 loader library not found. Check thirdparty/webview2/build/native/x64.")
endif()

add_executable(HBBrowser WIN32
    src/browser/main.cpp
    src/browser/BrowserApp.cpp
    src/browser/BrowserWindow.cpp
    src/browser/core/TabManager.cpp
    src/browser/core/Navigation.cpp
    src/browser/webview/WebViewManager.cpp
    src/browser/ui/MainUI.cpp
    src/browser/ui/Toolbar.cpp
    src/browser/ui/TabBar.cpp
    src/browser/ui/Sidebar.cpp
    src/browser/ui/SettingsUI.cpp
)

target_include_directories(HBBrowser PRIVATE
    src/browser
    "${WEBVIEW2_INCLUDE_DIR}"
    "${WIL_INCLUDE_DIR}"
)

target_compile_definitions(HBBrowser PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(HBBrowser PRIVATE
    "${WEBVIEW2_LOADER_LIBRARY}"
    user32
    gdi32
    ole32
    shlwapi
)

if(MSVC)
    target_compile_options(HBBrowser PRIVATE /W4 /permissive- /EHsc)
endif()

set_target_properties(HBBrowser PROPERTIES
    OUTPUT_NAME "HB Browser"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy the WebView2 loader DLL beside the executable when it is available.
file(GLOB WEBVIEW2_LOADER_DLLS
    "${WEBVIEW2_ROOT}/runtimes/win-x64/native/WebView2Loader.dll"
    "${WEBVIEW2_ROOT}/build/native/x64/WebView2Loader.dll"
)
if(WEBVIEW2_LOADER_DLLS)
    add_custom_command(TARGET HBBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WEBVIEW2_LOADER_DLLS}
        "$<TARGET_FILE_DIR:HBBrowser>"
    )
endif()
