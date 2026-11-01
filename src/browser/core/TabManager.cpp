#include "TabManager.h"
#include "../webview/WebViewManager.h"

#include <algorithm>

TabManager::TabManager(WebViewManager& webViews)
    : webViews_(webViews)
{
}

int TabManager::AddTab(const std::wstring& title, const std::wstring& url)
{
    const int id = nextId_++;
    tabs_.push_back({ id, title, url });
    webViews_.CreateTab(id, url);
    SetActiveTab(id);
    return id;
}

void TabManager::CloseTab(int id)
{
    const auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const BrowserTab& tab) { return tab.id == id; });

    if (it == tabs_.end())
        return;

    const auto index = static_cast<size_t>(std::distance(tabs_.begin(), it));
    const bool wasActive = activeId_ == id;

    webViews_.CloseTab(id);
    tabs_.erase(it);

    if (tabs_.empty())
    {
        AddTab(L"New tab", L"https://www.google.com");
        return;
    }

    if (wasActive)
    {
        const size_t newIndex = index == 0 ? 0 : index - 1;
        SetActiveTab(tabs_[newIndex].id);
    }
}

void TabManager::SetActiveTab(int id)
{
    const auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const BrowserTab& tab) { return tab.id == id; });

    if (it == tabs_.end())
        return;

    activeId_ = id;
    webViews_.SetActiveTab(id);
}

void TabManager::SetActiveUrl(const std::wstring& url)
{
    for (auto& tab : tabs_)
    {
        if (tab.id == activeId_)
        {
            tab.url = url;
            break;
        }
    }
}

const BrowserTab* TabManager::GetActiveTab() const noexcept
{
    for (const auto& tab : tabs_)
        if (tab.id == activeId_)
            return &tab;
    return nullptr;
}
