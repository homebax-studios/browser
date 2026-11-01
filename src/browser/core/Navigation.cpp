#include "Navigation.h"
#include "TabManager.h"
#include "../webview/WebViewManager.h"

#include <algorithm>
#include <cwctype>
#include <utility>

Navigation::Navigation(TabManager& tabs, WebViewManager& webViews)
    : tabs_(tabs), webViews_(webViews)
{
}

std::wstring Navigation::NormalizeAddress(std::wstring address)
{
    address.erase(address.begin(), std::find_if(address.begin(), address.end(),
        [](wchar_t ch) { return !iswspace(ch); }));
    address.erase(std::find_if(address.rbegin(), address.rend(),
        [](wchar_t ch) { return !iswspace(ch); }).base(), address.end());

    if (address.empty())
        return L"https://www.google.com";

    if (address.find(L"://") == std::wstring::npos &&
        address.rfind(L"about:", 0) != 0 &&
        address.rfind(L"file:", 0) != 0)
    {
        if (address.find(L' ') != std::wstring::npos ||
            address.find(L'.') == std::wstring::npos)
        {
            std::wstring query = L"https://www.google.com/search?q=";
            for (wchar_t ch : address)
                query += ch == L' ' ? L'+' : ch;
            return query;
        }
        address = L"https://" + address;
    }

    return address;
}

void Navigation::Navigate(std::wstring address)
{
    address = NormalizeAddress(std::move(address));
    tabs_.SetActiveUrl(address);
    webViews_.Navigate(tabs_.GetActiveId(), address);
}

void Navigation::Back()
{
    webViews_.GoBack(tabs_.GetActiveId());
}

void Navigation::Forward()
{
    webViews_.GoForward(tabs_.GetActiveId());
}

void Navigation::Reload()
{
    webViews_.Reload(tabs_.GetActiveId());
}
