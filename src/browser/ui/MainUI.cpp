#include "MainUI.h"
#include <windowsx.h>

#include "../core/Navigation.h"
#include "../core/TabManager.h"
#include "../webview/WebViewManager.h"

MainUI::MainUI(HWND hwnd, TabManager& tabs, Navigation& navigation, WebViewManager& webViews)
    : hwnd_(hwnd),
      tabs_(tabs),
      navigation_(navigation),
      webViews_(webViews),
      tabBar_(tabs),
      toolbar_(navigation)
{
}

void MainUI::Paint(HDC hdc)
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right;
    const int height = client.bottom;

    HBRUSH background = CreateSolidBrush(RGB(14, 14, 16));
    FillRect(hdc, &client, background);
    DeleteObject(background);

    tabBar_.Paint(hdc, width);
    toolbar_.Paint(hdc, width);
    sidebar_.Paint(hdc, height);
    settings_.Paint(hdc, width, height);
}

bool MainUI::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);

    if (message == WM_LBUTTONDOWN)
    {
        SetFocus(hwnd_);

        if (settings_.IsOpen() && settings_.HandleMouse(x, y, true))
        {
            Invalidate();
            return true;
        }

        if (tabBar_.HandleMouse(x, y, true))
        {
            UpdateAddress();
            UpdateLayout();
            Invalidate();
            return true;
        }

        if (toolbar_.HandleMouse(x, y, true))
        {
            Invalidate();
            return true;
        }

        // The bottom sidebar button opens the settings panel.
        if (x < 64 && y >= 94)
        {
            RECT client{};
            GetClientRect(hwnd_, &client);
            if (y >= client.bottom - 70)
            {
                settings_.Toggle();
                Invalidate();
                return true;
            }
        }

        if (sidebar_.HandleMouse(x, y, true))
        {
            Invalidate();
            return true;
        }
    }
    else if (message == WM_CHAR || message == WM_KEYDOWN)
    {
        if (toolbar_.HandleKey(message, wParam))
        {
            Invalidate();
            return true;
        }

        if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'L')
        {
            toolbar_.SetAddress(L"");
            toolbar_.HandleMouse(200, 60, true);
            Invalidate();
            return true;
        }
    }

    return false;
}

void MainUI::UpdateLayout()
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    webViews_.UpdateBounds(sidebar_.GetContentBounds(client.right, client.bottom));
}

void MainUI::Invalidate()
{
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainUI::UpdateAddress()
{
    const BrowserTab* active = tabs_.GetActiveTab();
    if (active)
        toolbar_.SetAddress(active->url);
}
