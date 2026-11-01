#include "BrowserWindow.h"

#include "core/Navigation.h"
#include "core/TabManager.h"
#include "ui/MainUI.h"
#include "webview/WebViewManager.h"

#include <memory>

namespace
{
    constexpr wchar_t WindowClassName[] = L"HB_BROWSER_MAIN_WINDOW";
}

BrowserWindow::BrowserWindow(HINSTANCE instance)
    : instance_(instance)
{
}

BrowserWindow::~BrowserWindow()
{
    mainUI_.reset();
    navigation_.reset();
    tabManager_.reset();
    webViewManager_.reset();
}

bool BrowserWindow::Create()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance_;
    wc.lpfnWndProc = WindowProcedure;
    wc.lpszClassName = WindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    hwnd_ = CreateWindowExW(
        0, WindowClassName, L"HB Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1360, 900,
        nullptr, nullptr, instance_, this
    );

    if (!hwnd_)
        return false;

    webViewManager_ = std::make_unique<WebViewManager>(hwnd_);
    tabManager_ = std::make_unique<TabManager>(*webViewManager_);
    navigation_ = std::make_unique<Navigation>(*tabManager_, *webViewManager_);
    mainUI_ = std::make_unique<MainUI>(hwnd_, *tabManager_, *navigation_, *webViewManager_);

    webViewManager_->Initialize();
    tabManager_->AddTab(L"New tab", L"https://www.google.com");
    mainUI_->UpdateLayout();

    return true;
}

LRESULT CALLBACK BrowserWindow::WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    BrowserWindow* self = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<BrowserWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }

    if (self)
        return self->HandleMessage(message, wParam, lParam);

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT BrowserWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (mainUI_)
            mainUI_->UpdateLayout();
        return 0;

    case WM_PAINT:
        if (mainUI_)
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd_, &ps);
            mainUI_->Paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_CHAR:
    case WM_KEYDOWN:
        if (mainUI_ && mainUI_->HandleMessage(message, wParam, lParam))
            return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
