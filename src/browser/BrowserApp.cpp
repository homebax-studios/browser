#include "BrowserApp.h"

namespace hb {

bool BrowserApp::Initialize(int showCommand, StartupOptions options)
{
    window_ = std::make_unique<BrowserWindow>(instance_, std::move(options));
    if (window_->Create(showCommand)) return true;
    MessageBoxW(nullptr, L"HB Browser could not start. See data\\logs\\browser.log for details.", L"HB Browser", MB_ICONERROR);
    return false;
}

int BrowserApp::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

}  // namespace hb
