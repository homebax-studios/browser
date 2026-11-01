#pragma once

#include <windows.h>

class Sidebar
{
public:
    void Paint(HDC hdc, int height);
    bool HandleMouse(int x, int y, bool isDown);
    RECT GetContentBounds(int width, int height) const;
    bool IsVisible() const noexcept { return visible_; }

private:
    bool visible_{true};
};
