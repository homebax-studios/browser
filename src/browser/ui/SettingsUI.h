#pragma once

#include <windows.h>

class SettingsUI
{
public:
    void Paint(HDC hdc, int width, int height);
    bool HandleMouse(int x, int y, bool isDown);
    bool IsOpen() const noexcept { return open_; }
    void Toggle() noexcept { open_ = !open_; }

private:
    bool open_{};
};
