#include "SettingsUI.h"

void SettingsUI::Paint(HDC hdc, int width, int height)
{
    if (!open_)
        return;

    RECT panel{width - 370, 94, width, height};
    HBRUSH brush = CreateSolidBrush(RGB(29, 29, 34));
    FillRect(hdc, &panel, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 245, 248));
    HFONT font = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));

    RECT heading{width - 340, 120, width - 20, 160};
    DrawTextW(hdc, L"Settings", -1, &heading, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old);
    DeleteObject(font);

    SetTextColor(hdc, RGB(185, 185, 195));
    RECT description{width - 340, 172, width - 20, 230};
    DrawTextW(hdc, L"Appearance\\nDark theme\\nMore settings will be added here.",
        -1, &description, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

bool SettingsUI::HandleMouse(int x, int y, bool isDown)
{
    if (!isDown || !open_)
        return false;

    if (x >= 0 && y >= 94)
    {
        open_ = false;
        return true;
    }
    return false;
}
