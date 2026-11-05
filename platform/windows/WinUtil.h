// Windows-only helpers. Everything platform specific lives under platform/<os>/ so the core stays portable.
#pragma once
#include <windows.h>

#include <filesystem>
#include <string>

namespace hb::win {

std::wstring Widen(const std::string& utf8);
std::string Narrow(const std::wstring& wide);

std::filesystem::path ExeDir();
std::filesystem::path ExePath();
// Portable data folder: <exe>/data, or %LOCALAPPDATA%\HB Browser when the exe folder is read-only.
std::filesystem::path ResolveDataDir();
// <exe>/resources (also looks next to the build folder so it works straight from Visual Studio).
std::filesystem::path FindResources();
std::filesystem::path DownloadsDir();
std::filesystem::path UniquePath(const std::filesystem::path& dir, const std::wstring& fileName);

bool SystemUsesDarkTheme();
UINT DpiForWindow(HWND hwnd);
void EnableDpiAwareness();
void SetDarkTitleBar(HWND hwnd, bool dark, COLORREF border);

std::wstring PickFolder(HWND owner, const std::wstring& initial);
void ShellOpen(const std::wstring& target);
void ShowInFolder(const std::filesystem::path& file);
// Registers the browser for the current user only (no admin, no installer) so it shows up in Default apps.
bool RegisterAsBrowser(const std::wstring& exePath);

// Blocking HTTPS GET with short timeouts. Call from a worker thread. Returns false on any failure.
bool HttpGet(const std::wstring& url, std::string& body);

}  // namespace hb::win
