#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <wil/com.h>
#include <WebView2.h>

class WebViewManager
{
public:
    explicit WebViewManager(HWND parent);
    ~WebViewManager();

    void Initialize();
    void CreateTab(int id, const std::wstring& url);
    void CloseTab(int id);
    void SetActiveTab(int id);
    void Navigate(int id, const std::wstring& url);
    void GoBack(int id);
    void GoForward(int id);
    void Reload(int id);
    void UpdateBounds(const RECT& bounds);

private:
    struct View
    {
        wil::com_ptr<ICoreWebView2Controller> controller;
        wil::com_ptr<ICoreWebView2> webView;
        std::wstring initialUrl;
    };

    void CreateController(int id, const std::wstring& url);
    void ApplyVisibility();

    HWND parent_{};
    RECT bounds_{};
    int activeId_{};
    bool environmentRequested_{};
    wil::com_ptr<ICoreWebView2Environment> environment_;
    std::unordered_map<int, View> views_;
    std::vector<std::pair<int, std::wstring>> pendingTabs_;
};
