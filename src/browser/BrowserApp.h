#pragma once

#include <windows.h>
#include <memory>

class BrowserWindow;

class BrowserApp
{
public:
    explicit BrowserApp(HINSTANCE instance);
    ~BrowserApp();

    bool Initialize(int showCommand);
    int Run();

private:
    HINSTANCE instance_{};
    std::unique_ptr<BrowserWindow> window_;
};
