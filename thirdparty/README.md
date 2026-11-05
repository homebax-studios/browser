# Third-party libraries

The libraries themselves are **not** part of the repository. Put each one into its folder exactly like this
(CMake checks these paths and tells you which one is missing):

| Library | Folder | Needed file(s) |
|---|---|---|
| WebView2 SDK | `thirdparty/webview2/` | unpacked NuGet package `Microsoft.Web.WebView2` (version 1.0.2535 or newer; private tabs, favicons and tab sleeping use newer interfaces): `build/native/include/WebView2.h`, `build/native/x64/WebView2LoaderStatic.lib` (+ `arm64`, `x86`) |
| nlohmann/json | `thirdparty/nlohmann-json/` | `include/nlohmann/json.hpp` (or `single_include/nlohmann/json.hpp`) |
| SQLite | `thirdparty/sqlite/` | amalgamation: `sqlite3.c`, `sqlite3.h` |
| spdlog | `thirdparty/spdlog/` | `include/spdlog/...` (header only) |
| WIL | `thirdparty/wil/` | `include/wil/com.h` ... |

A NuGet package is a ZIP: rename `microsoft.web.webview2.x.y.z.nupkg` to `.zip` and extract it into `thirdparty/webview2/`.

## WebView2
- **URL:** https://developer.microsoft.com/microsoft-edge/webview2
- **License:** BSD 3-Clause License (Microsoft)
- **Files source:** `thirdparty/webview2/`

## nlohmann/json
- **URL:** https://github.com/nlohmann/json
- **License:** MIT License
- **Files source:** `thirdparty/nlohmann-json/`

## SQLite
- **URL:** https://www.sqlite.org/download.html
- **License:** Public Domain
- **Files source:** `thirdparty/sqlite/`

## spdlog
- **URL:** https://github.com/gabime/spdlog
- **License:** MIT License
- **Files source:** `thirdparty/spdlog/`

## WIL (Windows Implementation Library)
- **URL:** https://github.com/microsoft/wil
- **License:** MIT License
- **Files source:** `thirdparty/wil/`
