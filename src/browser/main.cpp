#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <filesystem>

#include "BrowserApp.h"
#include "core/Settings.h"
#include "core/Util.h"
#include "../../platform/windows/WinUtil.h"

namespace {

// Turns command line arguments into URLs: http(s) links stay, existing files become file:/// URLs.
std::string ToUrl(const std::wstring& arg)
{
    std::error_code ec;
    if (std::filesystem::exists(arg, ec))
    {
        std::string u = hb::win::Narrow(std::filesystem::absolute(arg).generic_wstring());
        return "file:///" + hb::ReplaceAll(u, " ", "%20");
    }
    return hb::win::Narrow(arg);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    // Process hardening: only load DLLs from system32 / our own folder, abort on heap corruption.
    if (HMODULE k32 = GetModuleHandleW(L"kernel32.dll"))
        if (auto fn = reinterpret_cast<BOOL(WINAPI*)(DWORD)>(GetProcAddress(k32, "SetDefaultDllDirectories")))
            fn(0x00000800 /*LOAD_LIBRARY_SEARCH_SYSTEM32*/ | 0x00000200 /*LOAD_LIBRARY_SEARCH_APPLICATION_DIR*/);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    hb::win::EnableDpiAwareness();

    hb::StartupOptions options;
    bool newInstance = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i)
    {
        const std::wstring a = argv[i];
        if (a == L"--private" || a == L"-private") options.privateMode = true;
        else if (a == L"--new-instance") newInstance = true;
        else if (!a.empty() && a[0] != L'-') options.urls.push_back(ToUrl(a));
    }
    if (argv) LocalFree(argv);

    // One running browser by default: a second start hands its URLs to the first window.
    bool single = true;
    {
        hb::Settings s(hb::win::ResolveDataDir() / L"settings.json");
        s.Load();
        single = s.Get<bool>("singleInstance");
    }
    HANDLE mutex = nullptr;
    if (single && !newInstance && !options.privateMode)
    {
        mutex = CreateMutexW(nullptr, TRUE, L"Local\\HomebaxStudios.HBBrowser.Instance");
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            for (int attempt = 0; attempt < 20; ++attempt)
            {
                if (HWND other = FindWindowW(L"HB_BROWSER_MAIN_WINDOW", nullptr))
                {
                    std::string payload;
                    for (const auto& u : options.urls) payload += u + "\n";
                    COPYDATASTRUCT cds{hb::BrowserWindow::kCopyDataId, static_cast<DWORD>(payload.size()), payload.data()};
                    SendMessageW(other, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
                    CloseHandle(mutex);
                    return 0;
                }
                Sleep(100);  // the first instance may still be creating its window
            }
        }
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int exitCode = 1;
    {
        hb::BrowserApp app(instance);
        if (app.Initialize(showCommand, std::move(options))) exitCode = app.Run();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    if (mutex) CloseHandle(mutex);
    return exitCode;
}
