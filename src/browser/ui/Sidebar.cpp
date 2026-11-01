#include "Sidebar.h"

void Sidebar::Paint(HDC hdc, int height)
{
    if (!visible_)
        return;

    HBRUSH brush = CreateSolidBrush(RGB(20, 20, 23));
    RECT area{0, 94, 64, height};
    FillRect(hdc, &area, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 228));
    HFONT font = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));

    RECT home{0, 110, 64, 154};
    RECT bookmarks{0, 166, 64, 210};
    RECT downloads{0, 222, 64, 266};
    RECT settings{0, height - 58, 64, height - 14};

    DrawTextW(hdc, L"⌂", -1, &home, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"☆", -1, &bookmarks, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"↓", -1, &downloads, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"⚙", -1, &settings, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old);
    DeleteObject(font);
}

bool Sidebar::HandleMouse(int x, int y, bool isDown)
{
    if (!isDown || x >= 64 || y < 94)
        return false;

    if (y >= 94)
        return true;

    return false;
}

RECT Sidebar::GetContentBounds(int width, int height) const
{
    return RECT{visible_ ? 64L : 0L, 94L, width, height};
}
