#pragma once

#include <windows.h>

class TabManager;

class TabBar
{
public:
    explicit TabBar(TabManager& tabs);

    void Paint(HDC hdc, int width);
    bool HandleMouse(int x, int y, bool isDown);

private:
    TabManager& tabs_;
};
