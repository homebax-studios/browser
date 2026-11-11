#include "WinUtil.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>

#include <fstream>

namespace hb::win {

std::wstring Widen(const std::string& s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string Narrow(const std::wstring& w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::filesystem::path ExePath()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) { buf.resize(n); break; }
        buf.resize(buf.size() * 2);
    }
    return buf;
}

std::filesystem::path ExeDir() { return ExePath().parent_path(); }

static bool IsWritable(const std::filesystem::path& dir)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto probe = dir / L".write-test";
    std::ofstream f(probe, std::ios::binary);
    const bool ok = f.good();
    f.close();
    std::filesystem::remove(probe, ec);
    return ok;
}

std::filesystem::path ResolveDataDir()
{
    const auto portable = ExeDir() / L"data";
    if (IsWritable(portable)) return portable;
    PWSTR local = nullptr;
    std::filesystem::path result = portable;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)))
    {
        result = std::filesystem::path(local) / L"HB Browser";
        CoTaskMemFree(local);
    }
    std::error_code ec;
    std::filesystem::create_directories(result, ec);
    return result;
}

std::filesystem::path FindResources()
{
    std::error_code ec;
    auto dir = ExeDir();
    for (int i = 0; i < 4; ++i)
    {
        if (std::filesystem::exists(dir / L"resources" / L"ui" / L"shell.html", ec)) return dir / L"resources";
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return ExeDir() / L"resources";
}

std::filesystem::path DownloadsDir()
{
    PWSTR p = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p)))
    {
        result = p;
        CoTaskMemFree(p);
    }
    else
        result = ExeDir() / L"downloads";
    return result;
}

std::filesystem::path UniquePath(const std::filesystem::path& dir, const std::wstring& fileName)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path base(fileName);
    std::filesystem::path candidate = dir / base.filename();
    const std::wstring stem = base.stem().wstring();
    const std::wstring ext = base.extension().wstring();
    for (int i = 1; std::filesystem::exists(candidate, ec) && i < 10000; ++i)
        candidate = dir / (stem + L" (" + std::to_wstring(i) + L")" + ext);
    return candidate;
}

bool SystemUsesDarkTheme()
{
    DWORD value = 1, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS)
        return false;
    return value == 0;
}

UINT DpiForWindow(HWND hwnd)
{
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
    if (fn && hwnd) return fn(hwnd);
    HDC dc = GetDC(nullptr);
    const UINT dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
    ReleaseDC(nullptr, dc);
    return dpi ? dpi : 96;
}

void EnableDpiAwareness()
{
    using SetCtx = BOOL(WINAPI*)(HANDLE);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto fn = reinterpret_cast<SetCtx>(reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
    if (fn && fn(reinterpret_cast<HANDLE>(-4))) return;  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    SetProcessDPIAware();
}

void SetDarkTitleBar(HWND hwnd, bool dark, COLORREF border)
{
    const BOOL on = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &on, sizeof(on));
    const int round = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &round, sizeof(round));
    DwmSetWindowAttribute(hwnd, 34 /*DWMWA_BORDER_COLOR*/, &border, sizeof(border));
}

std::wstring PickFolder(HWND owner, const std::wstring& initial)
{
    std::wstring result;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return result;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (!initial.empty())
    {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder))))
        {
            dlg->SetFolder(folder);
            folder->Release();
        }
    }
    if (SUCCEEDED(dlg->Show(owner)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

void ShellOpen(const std::wstring& target)
{
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowInFolder(const std::filesystem::path& file)
{
    std::error_code ec;
    if (std::filesystem::exists(file, ec))
    {
        const std::wstring args = L"/select,\"" + file.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
    else
        ShellExecuteW(nullptr, L"open", file.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void SetReg(const std::wstring& subKey, const wchar_t* name, const std::wstring& value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, subKey.c_str(), name, REG_SZ, value.c_str(),
                    static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

bool RegisterAsBrowser(const std::wstring& exe)
{
    const std::wstring open = L"\"" + exe + L"\" \"%1\"";
    const std::wstring app = L"Software\\Clients\\StartMenuInternet\\HBBrowser";

    SetReg(L"Software\\Classes\\HBBrowserHTML", nullptr, L"HB Browser HTML Document");
    SetReg(L"Software\\Classes\\HBBrowserHTML\\DefaultIcon", nullptr, exe + L",0");
    SetReg(L"Software\\Classes\\HBBrowserHTML\\shell\\open\\command", nullptr, open);
    SetReg(L"Software\\Classes\\HBBrowserURL", nullptr, L"HB Browser URL");
    SetReg(L"Software\\Classes\\HBBrowserURL", L"URL Protocol", L"");
    SetReg(L"Software\\Classes\\HBBrowserURL\\DefaultIcon", nullptr, exe + L",0");
    SetReg(L"Software\\Classes\\HBBrowserURL\\shell\\open\\command", nullptr, open);

    SetReg(app, nullptr, L"HB Browser");
    SetReg(app + L"\\DefaultIcon", nullptr, exe + L",0");
    SetReg(app + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\"");
    SetReg(app + L"\\Capabilities", L"ApplicationName", L"HB Browser");
    SetReg(app + L"\\Capabilities", L"ApplicationDescription", L"HB Browser by Homebax Studios");
    SetReg(app + L"\\Capabilities\\FileAssociations", L".htm", L"HBBrowserHTML");
    SetReg(app + L"\\Capabilities\\FileAssociations", L".html", L"HBBrowserHTML");
    SetReg(app + L"\\Capabilities\\URLAssociations", L"http", L"HBBrowserURL");
    SetReg(app + L"\\Capabilities\\URLAssociations", L"https", L"HBBrowserURL");
    SetReg(L"Software\\RegisteredApplications", L"HB Browser", app + L"\\Capabilities");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool HttpGet(const std::wstring& url, std::string& body)
{
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;

    HINTERNET session = WinHttpOpen(L"HBBrowser-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 5000, 5000, 8000, 8000);
    bool ok = false;
    if (HINTERNET conn = WinHttpConnect(session, host, uc.nPort, 0))
    {
        const DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        if (HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags))
        {
            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(req, nullptr))
            {
                DWORD status = 0, size = sizeof(status);
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
                if (status == 200)
                {
                    ok = true;
                    for (;;)
                    {
                        DWORD avail = 0;
                        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                        std::string chunk(avail, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0) break;
                        body.append(chunk.data(), read);
                        if (body.size() > (1u << 20)) break;  // 1 MB is plenty for a version file
                    }
                }
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(session);
    return ok;
}

}  // namespace hb::win
