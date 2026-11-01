#include "Toolbar.h"
#include "../core/Navigation.h"

#include <windows.h>

namespace
{
    constexpr COLORREF Background = RGB(23, 23, 26);
    constexpr COLORREF Button = RGB(42, 42, 47);
    constexpr COLORREF Text = RGB(235, 235, 240);
}

Toolbar::Toolbar(Navigation& navigation)
    : navigation_(navigation)
{
}

void Toolbar::DrawButton(HDC hdc, const RECT& rect, const wchar_t* text)
{
    HBRUSH brush = CreateSolidBrush(Button);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    SetTextColor(hdc, Text);
    DrawTextW(hdc, text, -1, const_cast<RECT*>(&rect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void Toolbar::Paint(HDC hdc, int width)
{
    RECT area{0, 40, width, 94};
    HBRUSH brush = CreateSolidBrush(Background);
    FillRect(hdc, &area, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));

    DrawButton(hdc, RECT{12, 49, 48, 85}, L"←");
    DrawButton(hdc, RECT{54, 49, 90, 85}, L"→");
    DrawButton(hdc, RECT{96, 49, 132, 85}, L"↻");

    addressRect_ = RECT{146, 49, width - 110, 85};
    HBRUSH addressBrush = CreateSolidBrush(RGB(39, 39, 44));
    FillRect(hdc, &addressRect_, addressBrush);
    DeleteObject(addressBrush);

    SetTextColor(hdc, Text);
    RECT textRect = addressRect_;
    textRect.left += 14;
    DrawTextW(hdc, address_.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    DrawButton(hdc, RECT{width - 96, 49, width - 58, 85}, L"☆");
    DrawButton(hdc, RECT{width - 52, 49, width - 14, 85}, L"⋮");

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

bool Toolbar::HandleMouse(int x, int y, bool isDown)
{
    if (!isDown || y < 40 || y >= 94)
        return false;

    if (x >= 12 && x < 48) navigation_.Back();
    else if (x >= 54 && x < 90) navigation_.Forward();
    else if (x >= 96 && x < 132) navigation_.Reload();
    else if (x >= addressRect_.left && x < addressRect_.right)
    {
        addressFocused_ = true;
        SetFocus(GetActiveWindow());
    }
    else
        return false;

    return true;
}

bool Toolbar::HandleKey(UINT message, WPARAM wParam)
{
    if (!addressFocused_)
        return false;

    if (message == WM_KEYDOWN && wParam == VK_RETURN)
    {
        navigation_.Navigate(address_);
        addressFocused_ = false;
        return true;
    }

    if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
    {
        addressFocused_ = false;
        return true;
    }

    if (message == WM_CHAR)
    {
        if (wParam == VK_BACK)
        {
            if (!address_.empty()) address_.pop_back();
        }
        else if (wParam >= 32)
        {
            address_.push_back(static_cast<wchar_t>(wParam));
        }
        return true;
    }

    return false;
}

void Toolbar::SetAddress(const std::wstring& address)
{
    address_ = address;
}
