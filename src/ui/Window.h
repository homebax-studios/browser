#pragma once

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <memory>
#include <string>
#include <vector>

class Window
{
private:

    // ============================================================
    // HISTORY
    // ============================================================

    struct HistoryEntry
    {
        std::wstring title;
        std::wstring url;
    };

    // ============================================================
    // BOOKMARKS
    // ============================================================

    struct BookmarkEntry
    {
        std::wstring title;
        std::wstring url;
    };

    // ============================================================
    // DOWNLOADS
    // ============================================================

    struct DownloadEntry
    {
        std::wstring fileName;
        std::wstring state;
    };

    // ============================================================
    // BROWSER TAB
    // ============================================================

    struct BrowserTab
    {
        HWND tabButton = nullptr;
        HWND closeButton = nullptr;

        Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> webView;

        std::wstring title = L"Nová karta";
        std::wstring url = L"";

        // Prevents the same page load from being written into
        // the history list more than once (title can change
        // several times while a page is loading).
        bool historyRecorded = false;
    };

public:

    Window();
    ~Window();

    bool Create(
        HINSTANCE hInstance,
        int nCmdShow
    );

    int Run();

private:

    // ============================================================
    // WINDOW
    // ============================================================

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    static LRESULT CALLBACK AddressBarProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    // A single shared subclass procedure for every owner-drawn
    // button (toolbar buttons, the bookmark star, every tab and
    // every tab close button). It only exists to turn plain mouse
    // enter/leave into hover state that DrawButton() can render -
    // owner-draw buttons don't get "hot" styling for free.
    static LRESULT CALLBACK ButtonHoverProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    // ============================================================
    // UI
    // ============================================================

    void CreateBrowserUI();

    void ResizeBrowserUI();

    void CreateTabButton(
        int index
    );

    void UpdateTabUI();

    // Positions every tab button + close button + the "new tab"
    // button according to the current window width, shrinking
    // tabs (like Chrome) instead of letting them run off-window.
    void LayoutTabs();

    int ComputeTabWidth() const;

    void UpdateNavigationButtons();

    void ShowMenu();

    void DrawButton(
        LPDRAWITEMSTRUCT drawItem
    );

    void DrawTabButtonVisual(
        LPDRAWITEMSTRUCT drawItem,
        int index
    );

    void DrawTabCloseButtonVisual(
        LPDRAWITEMSTRUCT drawItem,
        int index
    );

    void DrawToolbarButtonVisual(
        LPDRAWITEMSTRUCT drawItem
    );

    void DrawBookmarkButtonVisual(
        LPDRAWITEMSTRUCT drawItem
    );

    void SubclassOwnerDrawButton(
        HWND hwnd
    );

    // Called by ButtonHoverProc when the mouse enters/leaves any
    // owner-drawn button. Looks up which control (and, for tabs,
    // which index) it belongs to by comparing HWNDs - never by a
    // numeric id, so a click can never be misrouted to the wrong
    // tab just because tabs were opened/closed in between.
    void OnButtonHover(
        HWND hwnd,
        bool hovered
    );

    void InvalidateTab(
        int index
    );

    void SetAddressBarFocused(
        bool focused
    );

    // ============================================================
    // TABS
    // ============================================================

    void CreateNewTab(
        const std::wstring& initialUrl = L"https://www.google.com"
    );

    void CreateWebViewForTab(
        int index
    );

    void CloseTab(
        int index
    );

    void SwitchToTab(
        int index
    );

    void CycleTab(
        int direction
    );

    // ============================================================
    // NAVIGATION
    // ============================================================

    void Navigate();

    void GoBack();

    void GoForward();

    void Reload();

    // ============================================================
    // WEBVIEW
    // ============================================================

    void ResizeWebView();

    void UpdateAddressBar();

    // ============================================================
    // HISTORY
    // ============================================================

    void AddHistoryEntry(
        const std::wstring& title,
        const std::wstring& url
    );

    void ShowHistory();

    // ============================================================
    // BOOKMARKS
    // ============================================================

    bool IsBookmarked(
        const std::wstring& url
    ) const;

    void ToggleBookmark();

    void ShowBookmarks();

    // ============================================================
    // DOWNLOADS
    // ============================================================

    void ShowDownloads();

    // ============================================================
    // HELPERS
    // ============================================================

    std::wstring NormalizeURL(
        std::wstring url
    );

    void SetDarkMode(
        HWND hwnd
    );

    void UpdateWindowTitle();

private:

    // ============================================================
    // MAIN WINDOW
    // ============================================================

    HWND m_hwnd = nullptr;

    HINSTANCE m_hInstance = nullptr;

    // ============================================================
    // NAVIGATION UI
    // ============================================================

    HWND m_backButton = nullptr;

    HWND m_forwardButton = nullptr;

    HWND m_reloadButton = nullptr;

    HWND m_addressBar = nullptr;

    HWND m_bookmarkButton = nullptr;

    HWND m_menuButton = nullptr;

    HWND m_newTabButton = nullptr;

    bool m_addressBarFocused = false;

    // ============================================================
    // ADDRESS BAR SUBCLASS
    // ============================================================

    WNDPROC m_originalAddressBarProc = nullptr;

    // ============================================================
    // TABS
    // ============================================================

    std::vector<std::unique_ptr<BrowserTab>> m_tabs;

    std::vector<HistoryEntry> m_history;

    std::vector<BookmarkEntry> m_bookmarks;

    std::vector<DownloadEntry> m_downloads;

    int m_activeTab = -1;

    int m_tabWidth = 0;

    // Hover tracking - HWND based for toolbar/bookmark button,
    // index based for tabs (recomputed on every mouse move, so a
    // tab being closed can never leave a stale hover pointing at
    // the wrong control).
    HWND m_hoverButtonHwnd = nullptr;

    int m_hoverTab = -1;

    int m_hoverTabClose = -1;

    // ============================================================
    // BUTTON SUBCLASS
    // ============================================================

    // Shared WNDPROC of the stock "BUTTON" class, captured once so
    // ButtonHoverProc can chain to the real button behaviour after
    // handling hover. Same function pointer for every BUTTON
    // control, so it only needs to be captured once.
    inline static WNDPROC s_defaultButtonProc = nullptr;

    // ============================================================
    // WEBVIEW ENVIRONMENT
    // ============================================================

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_environment;

    // ============================================================
    // SHARED GDI RESOURCES
    // ============================================================

    HFONT m_uiFont = nullptr;

    HFONT m_tabFont = nullptr;

    HBRUSH m_backgroundBrush = nullptr;

    HBRUSH m_tabBarBrush = nullptr;

    HBRUSH m_toolbarBrush = nullptr;

    // ============================================================
    // UI CONSTANTS
    // ============================================================

    static constexpr int TABS_HEIGHT = 44;

    static constexpr int TOOLBAR_HEIGHT = 58;

    static constexpr int TAB_BUTTON_HEIGHT = 36;

    static constexpr int TABS_START_X = 8;

    static constexpr int TAB_GAP = 3;

    static constexpr int TAB_MIN_WIDTH = 104;

    static constexpr int TAB_MAX_WIDTH = 232;

    static constexpr int NEW_TAB_BUTTON_SIZE = 32;

    static constexpr int NEW_TAB_GAP = 6;

    static constexpr int BOOKMARK_BUTTON_WIDTH = 36;

    static constexpr int WINDOW_WIDTH = 1280;

    static constexpr int WINDOW_HEIGHT = 800;

    inline static constexpr wchar_t CLASS_NAME[] =
        L"HomebaxBrowserWindow";

    // ============================================================
    // COLORS
    // ============================================================

    static constexpr COLORREF HB_COLOR_BACKGROUND =
        RGB(18, 18, 20);

    static constexpr COLORREF HB_COLOR_TAB_BAR =
        RGB(24, 24, 27);

    static constexpr COLORREF HB_COLOR_TOOLBAR =
        RGB(27, 27, 30);

    static constexpr COLORREF HB_COLOR_TAB =
        RGB(31, 31, 35);

    static constexpr COLORREF HB_COLOR_TAB_HOVER =
        RGB(39, 39, 44);

    static constexpr COLORREF HB_COLOR_TAB_ACTIVE =
        RGB(43, 43, 48);

    static constexpr COLORREF HB_COLOR_ADDRESS =
        RGB(35, 35, 40);

    static constexpr COLORREF HB_COLOR_HOVER =
        RGB(55, 55, 61);

    static constexpr COLORREF HB_COLOR_CLOSE_HOVER =
        RGB(232, 17, 35);

    static constexpr COLORREF HB_COLOR_CLOSE_PRESSED =
        RGB(180, 12, 27);

    static constexpr COLORREF HB_COLOR_TEXT =
        RGB(235, 235, 238);

    static constexpr COLORREF HB_COLOR_TEXT_SECONDARY =
        RGB(170, 170, 178);

    static constexpr COLORREF HB_COLOR_ACCENT =
        RGB(80, 150, 255);

}; // <-- Window
