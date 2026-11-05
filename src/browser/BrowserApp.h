#pragma once
#include <windows.h>

#include <memory>

#include "BrowserWindow.h"

namespace hb {

class BrowserApp
{
public:
    explicit BrowserApp(HINSTANCE instance) : instance_(instance) {}

    bool Initialize(int showCommand, StartupOptions options);
    int Run();

private:
    HINSTANCE instance_{};
    std::unique_ptr<BrowserWindow> window_;
};

}  // namespace hb
