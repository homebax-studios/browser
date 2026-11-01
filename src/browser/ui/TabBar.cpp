#include "TabBar.h"
#include "../core/TabManager.h"

#include <algorithm>

TabBar::TabBar(TabManager& tabs)
    : tabs_(tabs)
{
}

void TabBar::Paint(HDC hdc, int width)
{
    HBRUSH background = CreateSolidBrush(RGB(18, 18, 20));
    RECT area{0, 0, width, 40};
    FillRect(hdc, &area, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    int x = 10;
    for (const auto& tab : tabs_.GetTabs())
    {
        RECT rect{x, 5, x + 190, 35};
        HBRUSH tabBrush = CreateSolidBrush(
            tab.id == tabs_.GetActiveId() ? RGB(43, 43, 48) : RGB(27, 27, 30));
        FillRect(hdc, &rect, tabBrush);
        DeleteObject(tabBrush);

        SetTextColor(hdc, RGB(235, 235, 240));
        RECT titleRect{rect.left + 12, rect.top, rect.right - 34, rect.bottom};
        DrawTextW(hdc, tab.title.c_str(), -1, &titleRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT closeRect{rect.right - 30, rect.top, rect.right - 4, rect.bottom};
        DrawTextW(hdc, L"×", 1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += 196;
    }

    RECT addRect{x + 2, 5, x + 34, 35};
    SetTextColor(hdc, RGB(235, 235, 240));
    DrawTextW(hdc, L"+", 1, &addRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

bool TabBar::HandleMouse(int x, int y, bool isDown)
{
    if (!isDown || y < 0 || y >= 40)
        return false;

    if (x < 10)
        return true;

    const auto& tabs = tabs_.GetTabs();
    int tabIndex = (x - 10) / 196;

    if (tabIndex >= 0 && tabIndex < static_cast<int>(tabs.size()))
    {
        const int within = (x - 10) % 196;
        if (within >= 160)
            tabs_.CloseTab(tabs[tabIndex].id);
        else
            tabs_.SetActiveTab(tabs[tabIndex].id);
        return true;
    }

    const int addX = 10 + static_cast<int>(tabs.size()) * 196;
    if (x >= addX && x <= addX + 40)
    {
        tabs_.AddTab(L"New tab", L"https://www.google.com");
        return true;
    }

    return false;
}
