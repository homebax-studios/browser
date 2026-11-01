#pragma once

#include <windows.h>
#include <string>

class Navigation;

class Toolbar
{
public:
    explicit Toolbar(Navigation& navigation);

    void Paint(HDC hdc, int width);
    bool HandleMouse(int x, int y, bool isDown);
    bool HandleKey(UINT message, WPARAM wParam);
    void SetAddress(const std::wstring& address);
    const RECT& GetBounds() const noexcept { return bounds_; }

private:
    void DrawButton(HDC hdc, const RECT& rect, const wchar_t* text);

    Navigation& navigation_;
    RECT bounds_{};
    RECT addressRect_{};
    std::wstring address_{L"https://www.google.com"};
    bool addressFocused_{};
};
