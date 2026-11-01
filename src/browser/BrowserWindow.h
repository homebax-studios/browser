#pragma once

#include <windows.h>
#include <memory>

class BrowserApp;
class MainUI;
class WebViewManager;
class TabManager;
class Navigation;

class BrowserWindow
{
public:
    explicit BrowserWindow(HINSTANCE instance);
    ~BrowserWindow();

    bool Create();
    HWND GetHandle() const noexcept { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_{};
    HWND hwnd_{};

    std::unique_ptr<WebViewManager> webViewManager_;
    std::unique_ptr<TabManager> tabManager_;
    std::unique_ptr<Navigation> navigation_;
    std::unique_ptr<MainUI> mainUI_;
};
