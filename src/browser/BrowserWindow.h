#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

namespace hb {

class BrowserController;

struct StartupOptions
{
    std::vector<std::string> urls;
    bool privateMode = false;
};

// Win32 frame: custom title bar area, two host child windows (browser UI + page content), DPI, fullscreen.
class BrowserWindow
{
public:
    BrowserWindow(HINSTANCE instance, StartupOptions options);
    ~BrowserWindow();

    bool Create(int showCommand);

    HWND Handle() const noexcept { return hwnd_; }
    HWND UiHost() const noexcept { return uiHost_; }
    HWND ContentHost() const noexcept { return contentHost_; }
    double Scale() const noexcept { return scale_; }
    bool IsFullscreen() const noexcept { return fullscreen_; }
    bool IsMaximized() const noexcept { return IsZoomed(hwnd_) != FALSE; }

    // Called by the controller. All sizes from the UI are CSS pixels.
    void SetChromeHeight(int cssPx);
    void SetOverlays(std::vector<RECT> cssRects);  // popup areas (menu, suggestions...) that must be drawn above the page
    void SetFullscreen(bool on);
    void SetTheme(bool dark, COLORREF frameColor);
    void SetTitle(const std::wstring& title);
    void Minimize();
    void ToggleMaximize();
    void StartDrag();
    void Activate();
    void ApplyPlacement(int x, int y, int w, int h, bool maximized);
    void GetPlacement(int& x, int& y, int& w, int& h, bool& maximized) const;

    static constexpr UINT kMsgUpdateResult = WM_APP + 2;
    static constexpr DWORD kCopyDataId = 0x48424252;  // "HBBR"

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK HostProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT OnMessage(UINT msg, WPARAM wp, LPARAM lp);

    void Layout();
    int Scaled(int v) const { return static_cast<int>(v * scale_ + 0.5); }

    HINSTANCE instance_{};
    StartupOptions options_;
    HWND hwnd_{};
    HWND uiHost_{};
    HWND contentHost_{};
    HBRUSH background_{};
    double scale_ = 1.0;
    int chromeCss_ = 88;
    std::vector<RECT> overlays_;
    bool fullscreen_ = false;
    bool startMaximized_ = false;
    WINDOWPLACEMENT beforeFullscreen_{};
    std::unique_ptr<BrowserController> controller_;
};

}  // namespace hb
