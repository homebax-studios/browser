#pragma once

#include <string>

class TabManager;
class WebViewManager;

class Navigation
{
public:
    Navigation(TabManager& tabs, WebViewManager& webViews);

    void Navigate(std::wstring address);
    void Back();
    void Forward();
    void Reload();

private:
    static std::wstring NormalizeAddress(std::wstring address);

    TabManager& tabs_;
    WebViewManager& webViews_;
};
