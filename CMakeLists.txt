cmake_minimum_required(VERSION 3.15)

project(HomebaxBrowser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# WEBVIEW2
# ============================================================

set(WEBVIEW2_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/WebView2"
)

set(WEBVIEW2_INCLUDE
    "${WEBVIEW2_ROOT}/build/native/include"
)

set(WEBVIEW2_LIB
    "${WEBVIEW2_ROOT}/build/native/x64/WebView2Loader.dll.lib"
)

# ============================================================
# CHECK WEBVIEW2
# ============================================================

if(NOT EXISTS "${WEBVIEW2_INCLUDE}/WebView2.h")
    message(FATAL_ERROR
        "WebView2.h nebyl nalezen: ${WEBVIEW2_INCLUDE}/WebView2.h"
    )
endif()

if(NOT EXISTS "${WEBVIEW2_LIB}")
    message(FATAL_ERROR
        "WebView2Loader.dll.lib nebyl nalezen: ${WEBVIEW2_LIB}"
    )
endif()

# ============================================================
# SOURCES
# ============================================================

add_executable(
    HomebaxBrowser
    WIN32

    src/main.cpp

    src/ui/Window.cpp

    resources.rc

    src/engine/Engine.cpp

    src/networking/URL.cpp
    src/networking/HTTP.cpp

    src/html/DOM.cpp
    src/html/HTMLParser.cpp

    src/css/CSSParser.cpp
    src/css/StyleEngine.cpp

    src/rendering/Renderer.cpp
)

# ============================================================
# INCLUDE DIRECTORIES
# ============================================================

target_include_directories(
    HomebaxBrowser
    PRIVATE

    "${CMAKE_CURRENT_SOURCE_DIR}/src"

    "${WEBVIEW2_INCLUDE}"
)

# ============================================================
# WEBVIEW2
# ============================================================

target_link_libraries(
    HomebaxBrowser
    PRIVATE

    "${WEBVIEW2_LIB}"
)

# ============================================================
# WINDOWS
# ============================================================

target_link_libraries(
    HomebaxBrowser
    PRIVATE

    user32
    gdi32
    ole32
    oleaut32
    shlwapi
    shell32
)

# ============================================================
# UNICODE
# ============================================================

target_compile_definitions(
    HomebaxBrowser
    PRIVATE

    UNICODE
    _UNICODE
)