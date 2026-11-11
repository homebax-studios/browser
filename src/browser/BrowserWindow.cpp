#include "BrowserWindow.h"

#include <dwmapi.h>
#include <algorithm>
#include <windowsx.h>

#include "app/BrowserController.h"
#include "../../platform/windows/WinUtil.h"

namespace hb {

namespace {
constexpr wchar_t kMainClass[] = L"HB_BROWSER_MAIN_WINDOW";
constexpr wchar_t kHostClass[] = L"HB_BROWSER_HOST";
constexpr int kBorder = 5;  // resize border in DIP; the host windows are inset by the same amount
constexpr UINT_PTR kTimerHousekeeping = 1;
}  // namespace

BrowserWindow::BrowserWindow(HINSTANCE instance, StartupOptions options)
    : instance_(instance), options_(std::move(options))
{
}

BrowserWindow::~BrowserWindow()
{
    if (background_) DeleteObject(background_);
}

bool BrowserWindow::Create(int showCommand)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    wc.lpszClassName = kMainClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    WNDCLASSEXW host{};
    host.cbSize = sizeof(host);
    host.lpfnWndProc = HostProc;
    host.hInstance = instance_;
    host.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    host.lpszClassName = kHostClass;
    if (!RegisterClassExW(&host) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    background_ = CreateSolidBrush(RGB(28, 29, 33));

    hwnd_ = CreateWindowExW(0, kMainClass, L"HB Browser", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1360, 860, nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;
    scale_ = win::DpiForWindow(hwnd_) / 96.0;

    contentHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                   0, 0, 100, 100, hwnd_, nullptr, instance_, nullptr);
    uiHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, 100, 100, hwnd_, nullptr, instance_, nullptr);
    if (!contentHost_ || !uiHost_) return false;

    const MARGINS margins{1, 1, 1, 1};  // keeps the drop shadow and rounded corners of the borderless frame
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    controller_ = std::make_unique<BrowserController>(*this, options_);
    if (!controller_->Start()) return false;

    Layout();
    SetTimer(hwnd_, kTimerHousekeeping, 500, nullptr);
    ShowWindow(hwnd_, startMaximized_ ? SW_SHOWMAXIMIZED : showCommand);
    UpdateWindow(hwnd_);
    return true;
}

void BrowserWindow::Layout()
{
    if (!hwnd_ || !uiHost_ || !contentHost_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int inset = (fullscreen_ || IsZoomed(hwnd_)) ? 0 : Scaled(kBorder);
    const int x = inset, y = inset;
    const int w = std::max(0, static_cast<int>(rc.right) - 2 * inset);
    const int h = std::max(0, static_cast<int>(rc.bottom) - 2 * inset);
    const int shellH = fullscreen_ ? 0 : std::min(h, Scaled(shellCss_));

    MoveWindow(contentHost_, x, y + shellH, w, std::max(0, h - shellH), TRUE);
    MoveWindow(uiHost_, x, y, w, h, TRUE);
    ShowWindow(uiHost_, fullscreen_ ? SW_HIDE : SW_SHOW);

    // The UI host is as big as the whole window but only the toolbar strip and open popups are "solid".
    if (!fullscreen_)
    {
        HRGN region = CreateRectRgn(0, 0, w, shellH);
        for (const RECT& r : overlays_)
        {
            // popups have 12 css px rounded corners, so the clip region is rounded the same way
            HRGN part = CreateRoundRectRgn(Scaled(r.left), Scaled(r.top), Scaled(r.right) + 1, Scaled(r.bottom) + 1, Scaled(24), Scaled(24));
            CombineRgn(region, region, part, RGN_OR);
            DeleteObject(part);
        }
        SetWindowRgn(uiHost_, region, TRUE);  // the system owns the region from here on
    }
    SetWindowPos(uiHost_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (controller_)
        controller_->OnLayout(RECT{0, 0, w, h}, RECT{0, 0, w, std::max(0, h - shellH)});
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void BrowserWindow::SetShellHeight(int cssPx)
{
    cssPx = std::max(40, std::min(cssPx, 400));
    if (cssPx == shellCss_) return;
    shellCss_ = cssPx;
    Layout();
}

void BrowserWindow::SetOverlays(std::vector<RECT> cssRects)
{
    overlays_ = std::move(cssRects);
    Layout();
}

void BrowserWindow::SetFullscreen(bool on)
{
    if (on == fullscreen_) return;
    if (on)
    {
        beforeFullscreen_.length = sizeof(beforeFullscreen_);
        GetWindowPlacement(hwnd_, &beforeFullscreen_);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        fullscreen_ = true;
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    }
    else
    {
        fullscreen_ = false;
        SetWindowPlacement(hwnd_, &beforeFullscreen_);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    Layout();
}

void BrowserWindow::SetTheme(bool dark, COLORREF frameColor)
{
    if (background_) DeleteObject(background_);
    background_ = CreateSolidBrush(frameColor);
    win::SetDarkTitleBar(hwnd_, dark, frameColor);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void BrowserWindow::SetTitle(const std::wstring& title) { SetWindowTextW(hwnd_, title.c_str()); }
void BrowserWindow::Minimize() { ShowWindow(hwnd_, SW_MINIMIZE); }
void BrowserWindow::ToggleMaximize() { ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE); }

void BrowserWindow::StartDrag()
{
    ReleaseCapture();
    SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void BrowserWindow::Activate()
{
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

void BrowserWindow::ApplyPlacement(int x, int y, int w, int h, bool maximized)
{
    startMaximized_ = maximized;
    if (w < 480 || h < 360) return;
    RECT r{x, y, x + w, y + h};
    if (!MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) return;  // saved on a monitor that is gone
    SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void BrowserWindow::GetPlacement(int& x, int& y, int& w, int& h, bool& maximized) const
{
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (fullscreen_) wp = beforeFullscreen_;
    else GetWindowPlacement(hwnd_, &wp);
    x = wp.rcNormalPosition.left;
    y = wp.rcNormalPosition.top;
    w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    maximized = wp.showCmd == SW_SHOWMAXIMIZED;
}

LRESULT CALLBACK BrowserWindow::HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND) return 1;  // WebView2 paints everything
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK BrowserWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<BrowserWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) return self->OnMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT BrowserWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_NCCALCSIZE:
        if (wp)
        {
            // No system title bar or border: the client area is the whole window.
            auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
            if (!fullscreen_ && IsZoomed(hwnd_))
            {
                MONITORINFO mi{sizeof(mi)};
                if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) p->rgrc[0] = mi.rcWork;
            }
        }
        return 0;

    case WM_NCHITTEST:
    {
        if (fullscreen_ || IsZoomed(hwnd_)) return HTCLIENT;
        const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        RECT wr{};
        GetWindowRect(hwnd_, &wr);
        const int b = Scaled(kBorder);
        const bool l = pt.x < wr.left + b, r = pt.x >= wr.right - b, t = pt.y < wr.top + b, bt = pt.y >= wr.bottom - b;
        if (t && l) return HTTOPLEFT;
        if (t && r) return HTTOPRIGHT;
        if (bt && l) return HTBOTTOMLEFT;
        if (bt && r) return HTBOTTOMRIGHT;
        if (l) return HTLEFT;
        if (r) return HTRIGHT;
        if (t) return HTTOP;
        if (bt) return HTBOTTOM;
        return HTCLIENT;
    }

    case WM_GETMINMAXINFO:
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = Scaled(520);
        mmi->ptMinTrackSize.y = Scaled(380);
        return 0;
    }

    case WM_SIZE:
        Layout();
        if (controller_) controller_->OnWindowStateChanged();
        return 0;

    case WM_DPICHANGED:
    {
        scale_ = HIWORD(wp) / 96.0;
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }

    case WM_ERASEBKGND:
    {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, background_);
        return 1;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        FillRect(dc, &ps.rcPaint, background_);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_SETFOCUS:
    case WM_ACTIVATE:
        if (msg == WM_SETFOCUS || LOWORD(wp) != WA_INACTIVE)
            if (controller_) controller_->FocusActive();
        break;

    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;  // no beeping when Alt is pressed
        break;

    case WM_SETTINGCHANGE:
        if (lp && lstrcmpW(reinterpret_cast<LPCWSTR>(lp), L"ImmersiveColorSet") == 0 && controller_)
            controller_->OnSystemThemeChanged();
        break;

    case WM_TIMER:
        if (controller_) controller_->OnTimer(wp);
        return 0;

    case kMsgUpdateResult:
        if (controller_) controller_->OnUpdateResult(reinterpret_cast<std::string*>(lp));
        else delete reinterpret_cast<std::string*>(lp);
        return 0;

    case WM_COPYDATA:
    {
        const auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if (cds && cds->dwData == kCopyDataId && controller_)
        {
            const std::string text(static_cast<const char*>(cds->lpData), cds->cbData);
            std::vector<std::string> urls;
            size_t start = 0;
            while (start < text.size())
            {
                size_t end = text.find('\n', start);
                if (end == std::string::npos) end = text.size();
                if (end > start) urls.push_back(text.substr(start, end - start));
                start = end + 1;
            }
            controller_->OpenExternal(urls);
            Activate();
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        if (controller_) controller_->OnClosing();
        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd_, kTimerHousekeeping);
        if (controller_)
        {
            controller_->Shutdown();
            controller_.reset();
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace hb
