#pragma once

#include <string>
#include <vector>

class WebViewManager;

struct BrowserTab
{
    int id{};
    std::wstring title;
    std::wstring url;
};

class TabManager
{
public:
    explicit TabManager(WebViewManager& webViews);

    int AddTab(const std::wstring& title, const std::wstring& url);
    void CloseTab(int id);
    void SetActiveTab(int id);
    void SetActiveUrl(const std::wstring& url);

    const std::vector<BrowserTab>& GetTabs() const noexcept { return tabs_; }
    const BrowserTab* GetActiveTab() const noexcept;
    int GetActiveId() const noexcept { return activeId_; }

private:
    WebViewManager& webViews_;
    std::vector<BrowserTab> tabs_;
    int nextId_{1};
    int activeId_{};
};
