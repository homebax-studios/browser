#pragma once

#include <windows.h>
#include <string>

#include "TabBar.h"
#include "Toolbar.h"
#include "Sidebar.h"
#include "SettingsUI.h"

class TabManager;
class Navigation;
class WebViewManager;

class MainUI
{
public:
    MainUI(HWND hwnd, TabManager& tabs, Navigation& navigation, WebViewManager& webViews);

    void Paint(HDC hdc);
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void UpdateLayout();

private:
    void Invalidate();
    void UpdateAddress();

    HWND hwnd_{};
    TabManager& tabs_;
    Navigation& navigation_;
    WebViewManager& webViews_;

    TabBar tabBar_;
    Toolbar toolbar_;
    Sidebar sidebar_;
    SettingsUI settings_;
};
