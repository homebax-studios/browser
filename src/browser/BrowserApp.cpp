#include "BrowserApp.h"
#include "BrowserWindow.h"

BrowserApp::BrowserApp(HINSTANCE instance)
    : instance_(instance)
{
}

BrowserApp::~BrowserApp() = default;

bool BrowserApp::Initialize(int showCommand)
{
    window_ = std::make_unique<BrowserWindow>(instance_);
    if (!window_->Create())
        return false;

    ShowWindow(window_->GetHandle(), showCommand);
    UpdateWindow(window_->GetHandle());
    return true;
}

int BrowserApp::Run()
{
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
