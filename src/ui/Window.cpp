#include "Window.h"

#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <filesystem>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

// ================================================================
// CONSTRUCTOR
// ================================================================

Window::Window()
{
}

// ================================================================
// DESTRUCTOR
// ================================================================

Window::~Window()
{
    for (auto& tab : m_tabs)
    {
        if (tab && tab->controller)
        {
            tab->controller->Close();
        }
    }

    m_tabs.clear();

    m_environment.Reset();

    // ============================================================
    // SHARED GDI RESOURCES
    // ============================================================
    // These are created once in Create() and reused for every
    // paint / owner-draw call, so they only need to be released
    // once here instead of after every draw.

    if (m_uiFont)
    {
        DeleteObject(m_uiFont);
        m_uiFont = nullptr;
    }

    if (m_tabFont)
    {
        DeleteObject(m_tabFont);
        m_tabFont = nullptr;
    }

    if (m_backgroundBrush)
    {
        DeleteObject(m_backgroundBrush);
        m_backgroundBrush = nullptr;
    }

    if (m_tabBarBrush)
    {
        DeleteObject(m_tabBarBrush);
        m_tabBarBrush = nullptr;
    }

    if (m_toolbarBrush)
    {
        DeleteObject(m_toolbarBrush);
        m_toolbarBrush = nullptr;
    }
}

// ================================================================
// CREATE
// ================================================================

bool Window::Create(
    HINSTANCE hInstance,
    int nCmdShow
)
{
    m_hInstance = hInstance;

    // ============================================================
    // SHARED GDI RESOURCES
    // ============================================================
    // Created once and reused everywhere. The previous version of
    // this file created a brand new HFONT on every single
    // WM_DRAWITEM and every CreateTabButton() call and never
    // deleted most of them - since tab buttons used to be
    // destroyed and recreated on every navigation event, that
    // leaked one GDI font handle per page load. Sharing a handful
    // of fonts/brushes for the lifetime of the window fixes that.

    m_backgroundBrush =
        CreateSolidBrush(HB_COLOR_BACKGROUND);

    m_tabBarBrush =
        CreateSolidBrush(HB_COLOR_TAB_BAR);

    m_toolbarBrush =
        CreateSolidBrush(HB_COLOR_TOOLBAR);

    m_uiFont =
        CreateFontW(
            17,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH |
            FF_DONTCARE,
            L"Segoe UI"
        );

    m_tabFont =
        CreateFontW(
            14,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH |
            FF_DONTCARE,
            L"Segoe UI"
        );

    WNDCLASSW windowClass = {};

    windowClass.lpfnWndProc =
        WindowProc;

    windowClass.hInstance =
        m_hInstance;

    windowClass.lpszClassName =
        CLASS_NAME;

    windowClass.hCursor =
        LoadCursorW(
            nullptr,
            MAKEINTRESOURCEW(IDC_ARROW)
        );

    windowClass.hbrBackground =
        m_backgroundBrush;

    windowClass.style =
        CS_HREDRAW |
        CS_VREDRAW |
        CS_DBLCLKS;
    // CS_DBLCLKS is required so double-clicking empty tab-strip
    // space (to open a new tab, like Chrome) can be detected at
    // all - without it Windows never generates WM_LBUTTONDBLCLK
    // for this window.

    if (!RegisterClassW(&windowClass))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            MessageBoxW(
                nullptr,
                L"Nepodařilo se zaregistrovat okno.",
                L"Homebax Browser",
                MB_ICONERROR
            );

            return false;
        }
    }

    m_hwnd =
        CreateWindowExW(
            0,
            CLASS_NAME,
            L"Homebax Browser",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            nullptr,
            nullptr,
            m_hInstance,
            this
        );

    if (!m_hwnd)
    {
        MessageBoxW(
            nullptr,
            L"Nepodařilo se vytvořit okno.",
            L"Homebax Browser",
            MB_ICONERROR
        );

        return false;
    }

    SetDarkMode(m_hwnd);

    CreateBrowserUI();

    ShowWindow(
        m_hwnd,
        nCmdShow
    );

    UpdateWindow(
        m_hwnd
    );

    // ============================================================
    // WEBVIEW2 USER DATA
    // ============================================================

    wchar_t* localAppData =
        _wgetenv(L"LOCALAPPDATA");

    std::wstring userDataFolder =
        localAppData
        ? localAppData
        : L".";

    userDataFolder +=
        L"\\HomebaxBrowser\\WebView2";

    // ============================================================
    // WEBVIEW2 ENVIRONMENT
    // ============================================================

    HRESULT hr =
        CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            userDataFolder.c_str(),
            nullptr,
            Callback<
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
            >(
                [this](
                    HRESULT result,
                    ICoreWebView2Environment* environment
                    ) -> HRESULT
                {
                    if (
                        FAILED(result) ||
                        !environment
                        )
                    {
                        MessageBoxW(
                            m_hwnd,
                            L"Nepodařilo se inicializovat WebView2.",
                            L"Homebax Browser",
                            MB_ICONERROR
                        );

                        return result;
                    }

                    m_environment =
                        environment;

                    CreateNewTab();

                    return S_OK;
                }
            ).Get()
        );

    if (FAILED(hr))
    {
        MessageBoxW(
            m_hwnd,
            L"CreateCoreWebView2EnvironmentWithOptions selhalo.",
            L"Homebax Browser",
            MB_ICONERROR
        );

        return false;
    }

    return true;
}

// ================================================================
// DARK MODE
// ================================================================

void Window::SetDarkMode(
    HWND hwnd
)
{
    // DWMWA_USE_IMMERSIVE_DARK_MODE
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;

    BOOL enabled = TRUE;

    HMODULE dwmapi =
        LoadLibraryW(L"dwmapi.dll");

    if (!dwmapi)
    {
        return;
    }

    using DwmSetWindowAttributeFunc =
        HRESULT(WINAPI*)(
            HWND,
            DWORD,
            LPCVOID,
            DWORD
            );

    auto DwmSetWindowAttribute =
        reinterpret_cast<DwmSetWindowAttributeFunc>(
            GetProcAddress(
                dwmapi,
                "DwmSetWindowAttribute"
            )
            );

    if (DwmSetWindowAttribute)
    {
        DwmSetWindowAttribute(
            hwnd,
            DWMWA_USE_IMMERSIVE_DARK_MODE,
            &enabled,
            sizeof(enabled)
        );
    }

    FreeLibrary(dwmapi);
}

// ================================================================
// CREATE BROWSER UI
// ================================================================

void Window::CreateBrowserUI()
{
    // ============================================================
    // NEW TAB
    // ============================================================
    // Position is a placeholder - LayoutTabs() moves this right
    // after the last tab on every layout pass, Chrome-style,
    // instead of pinning it to the far left of the tab strip.

    m_newTabButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"+",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            TABS_START_X,
            5,
            NEW_TAB_BUTTON_SIZE,
            34,
            m_hwnd,
            reinterpret_cast<HMENU>(1000),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_newTabButton);

    // ============================================================
    // BACK
    // ============================================================

    m_backButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u2190",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            10,
            TABS_HEIGHT + 10,
            38,
            36,
            m_hwnd,
            reinterpret_cast<HMENU>(1001),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_backButton);

    // ============================================================
    // FORWARD
    // ============================================================

    m_forwardButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u2192",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            48,
            TABS_HEIGHT + 10,
            38,
            36,
            m_hwnd,
            reinterpret_cast<HMENU>(1002),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_forwardButton);

    // ============================================================
    // RELOAD
    // ============================================================

    m_reloadButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u21BB",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            86,
            TABS_HEIGHT + 10,
            38,
            36,
            m_hwnd,
            reinterpret_cast<HMENU>(1003),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_reloadButton);

    // ============================================================
    // ADDRESS BAR
    // ============================================================

    m_addressBar =
        CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD |
            WS_VISIBLE |
            ES_AUTOHSCROLL |
            ES_LEFT,
            132,
            TABS_HEIGHT + 9,
            700,
            38,
            m_hwnd,
            reinterpret_cast<HMENU>(1004),
            m_hInstance,
            nullptr
        );

    SendMessageW(
        m_addressBar,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SetWindowLongPtrW(
        m_addressBar,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this)
    );

    m_originalAddressBarProc =
        reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                m_addressBar,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    AddressBarProc
                    )
            )
            );

    // ============================================================
    // BOOKMARK STAR
    // ============================================================

    m_bookmarkButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u2606",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            0,
            TABS_HEIGHT + 9,
            BOOKMARK_BUTTON_WIDTH,
            38,
            m_hwnd,
            reinterpret_cast<HMENU>(1006),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_bookmarkButton);

    // ============================================================
    // MENU
    // ============================================================

    m_menuButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u22EE",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            850,
            TABS_HEIGHT + 10,
            42,
            36,
            m_hwnd,
            reinterpret_cast<HMENU>(1005),
            m_hInstance,
            nullptr
        );

    SubclassOwnerDrawButton(m_menuButton);

    // ============================================================
    // FONT
    // ============================================================
    // (Kept for controls that ever fall back to default drawing -
    // actual rendering of owner-draw buttons happens in DrawButton
    // using the shared m_uiFont / m_tabFont.)

    SendMessageW(
        m_newTabButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SendMessageW(
        m_backButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SendMessageW(
        m_forwardButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SendMessageW(
        m_reloadButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SendMessageW(
        m_menuButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_uiFont),
        TRUE
    );

    SetFocus(
        m_addressBar
    );
}

// ================================================================
// RESIZE UI
// ================================================================

void Window::ResizeBrowserUI()
{
    if (!m_hwnd)
    {
        return;
    }

    RECT rect = {};

    GetClientRect(
        m_hwnd,
        &rect
    );

    int width =
        rect.right;

    // ============================================================
    // NAVIGATION
    // ============================================================

    MoveWindow(
        m_backButton,
        10,
        TABS_HEIGHT + 10,
        38,
        36,
        TRUE
    );

    MoveWindow(
        m_forwardButton,
        48,
        TABS_HEIGHT + 10,
        38,
        36,
        TRUE
    );

    MoveWindow(
        m_reloadButton,
        86,
        TABS_HEIGHT + 10,
        38,
        36,
        TRUE
    );

    // ============================================================
    // MENU
    // ============================================================

    MoveWindow(
        m_menuButton,
        width - 52,
        TABS_HEIGHT + 10,
        42,
        36,
        TRUE
    );

    // ============================================================
    // BOOKMARK STAR
    // ============================================================

    int bookmarkX =
        width - 94; // 52 (menu offset) + 6 (gap) + 36 (star width)

    MoveWindow(
        m_bookmarkButton,
        bookmarkX,
        TABS_HEIGHT + 9,
        BOOKMARK_BUTTON_WIDTH,
        38,
        TRUE
    );

    // ============================================================
    // ADDRESS BAR
    // ============================================================

    int addressWidth =
        bookmarkX - 6 - 132;

    if (addressWidth < 200)
    {
        addressWidth = 200;
    }

    MoveWindow(
        m_addressBar,
        132,
        TABS_HEIGHT + 9,
        addressWidth,
        38,
        TRUE
    );

    // ============================================================
    // TABS + NEW TAB
    // ============================================================

    LayoutTabs();

    ResizeWebView();
}

// ================================================================
// COMPUTE TAB WIDTH
// ================================================================
// Chrome shrinks every tab as more are opened instead of letting
// them overflow the window. This mirrors that: width is however
// much space is available divided by the tab count, clamped to a
// sensible min/max.

int Window::ComputeTabWidth() const
{
    int tabCount =
        static_cast<int>(m_tabs.size());

    if (
        tabCount <= 0 ||
        !m_hwnd
        )
    {
        return TAB_MAX_WIDTH;
    }

    RECT rect = {};

    GetClientRect(
        m_hwnd,
        &rect
    );

    int available =
        rect.right -
        TABS_START_X -
        NEW_TAB_BUTTON_SIZE -
        NEW_TAB_GAP -
        TABS_START_X;

    if (available < TAB_MIN_WIDTH)
    {
        available = TAB_MIN_WIDTH;
    }

    int width =
        available / tabCount -
        TAB_GAP;

    if (width > TAB_MAX_WIDTH)
    {
        width = TAB_MAX_WIDTH;
    }

    if (width < TAB_MIN_WIDTH)
    {
        width = TAB_MIN_WIDTH;
    }

    return width;
}

// ================================================================
// LAYOUT TABS
// ================================================================
// Moves/resizes existing tab buttons and creates HWNDs only for
// tabs that don't have one yet - it never destroys and recreates
// buttons that already exist. (See CreateTabButton() for why that
// used to be a problem.)

void Window::LayoutTabs()
{
    if (!m_hwnd)
    {
        return;
    }

    m_tabWidth =
        ComputeTabWidth();

    int x =
        TABS_START_X;

    for (
        int i = 0;
        i < static_cast<int>(m_tabs.size());
        ++i
        )
    {
        BrowserTab* tab =
            m_tabs[i].get();

        if (!tab)
        {
            continue;
        }

        if (!tab->tabButton)
        {
            CreateTabButton(i);
        }

        std::wstring title =
            tab->title.empty()
            ? L"Nová karta"
            : tab->title;

        wchar_t current[256] = {};

        GetWindowTextW(
            tab->tabButton,
            current,
            256
        );

        if (title != current)
        {
            SetWindowTextW(
                tab->tabButton,
                title.c_str()
            );
        }

        MoveWindow(
            tab->tabButton,
            x,
            4,
            m_tabWidth,
            TAB_BUTTON_HEIGHT,
            TRUE
        );

        MoveWindow(
            tab->closeButton,
            x + m_tabWidth - 30,
            10,
            24,
            24,
            TRUE
        );

        x +=
            m_tabWidth +
            TAB_GAP;
    }

    MoveWindow(
        m_newTabButton,
        x + NEW_TAB_GAP,
        5,
        NEW_TAB_BUTTON_SIZE,
        34,
        TRUE
    );
}

// ================================================================
// DRAW BUTTON (dispatcher)
// ================================================================
// Figures out what kind of control is being drawn by comparing
// HWNDs (never by a numeric id - ids are only used as the Win32
// child-id Windows itself needs, they carry no meaning here), then
// hands off to the matching Draw*Visual() helper below.

void Window::DrawButton(
    LPDRAWITEMSTRUCT drawItem
)
{
    if (!drawItem)
    {
        return;
    }

    for (
        int i = 0;
        i < static_cast<int>(m_tabs.size());
        ++i
        )
    {
        BrowserTab* tab =
            m_tabs[i].get();

        if (!tab)
        {
            continue;
        }

        if (drawItem->hwndItem == tab->tabButton)
        {
            DrawTabButtonVisual(
                drawItem,
                i
            );

            return;
        }

        if (drawItem->hwndItem == tab->closeButton)
        {
            DrawTabCloseButtonVisual(
                drawItem,
                i
            );

            return;
        }
    }

    if (drawItem->hwndItem == m_bookmarkButton)
    {
        DrawBookmarkButtonVisual(
            drawItem
        );

        return;
    }

    DrawToolbarButtonVisual(
        drawItem
    );
}

// ================================================================
// DRAW TOOLBAR BUTTON
// ================================================================
// Used for the "+" new-tab button, back/forward/reload and the
// "⋮" menu button. Chrome-style: flat, no background box, a
// subtle rounded highlight only on hover/press.

void Window::DrawToolbarButtonVisual(
    LPDRAWITEMSTRUCT drawItem
)
{
    HDC dc =
        drawItem->hDC;

    RECT rect =
        drawItem->rcItem;

    bool pressed =
        (drawItem->itemState & ODS_SELECTED) != 0;

    bool disabled =
        (drawItem->itemState & ODS_DISABLED) != 0;

    bool hovered =
        (drawItem->hwndItem == m_hoverButtonHwnd);

    // The "+" button sits in the tab strip, everything else sits
    // in the toolbar row below it - each needs to blend into its
    // own row's background rather than always using the toolbar
    // color (that mismatch was visible as a colored box around the
    // "+" button in the previous version).

    COLORREF baseColor =
        (drawItem->hwndItem == m_newTabButton)
        ? HB_COLOR_TAB_BAR
        : HB_COLOR_TOOLBAR;

    HBRUSH baseBrush =
        CreateSolidBrush(baseColor);

    FillRect(
        dc,
        &rect,
        baseBrush
    );

    DeleteObject(baseBrush);

    if (
        !disabled &&
        (pressed || hovered)
        )
    {
        COLORREF highlight =
            pressed
            ? HB_COLOR_HOVER
            : HB_COLOR_TAB_HOVER;

        HRGN region =
            CreateRoundRectRgn(
                rect.left + 2,
                rect.top + 2,
                rect.right - 1,
                rect.bottom - 1,
                8,
                8
            );

        HBRUSH highlightBrush =
            CreateSolidBrush(highlight);

        FillRgn(
            dc,
            region,
            highlightBrush
        );

        DeleteObject(highlightBrush);
        DeleteObject(region);
    }

    wchar_t text[64] = {};

    GetWindowTextW(
        drawItem->hwndItem,
        text,
        64
    );

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        disabled
        ? HB_COLOR_TEXT_SECONDARY
        : HB_COLOR_TEXT
    );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                m_uiFont
            )
            );

    DrawTextW(
        dc,
        text,
        -1,
        &rect,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ================================================================
// DRAW BOOKMARK STAR
// ================================================================

void Window::DrawBookmarkButtonVisual(
    LPDRAWITEMSTRUCT drawItem
)
{
    HDC dc =
        drawItem->hDC;

    RECT rect =
        drawItem->rcItem;

    bool pressed =
        (drawItem->itemState & ODS_SELECTED) != 0;

    bool hovered =
        (drawItem->hwndItem == m_hoverButtonHwnd);

    HBRUSH baseBrush =
        CreateSolidBrush(HB_COLOR_TOOLBAR);

    FillRect(
        dc,
        &rect,
        baseBrush
    );

    DeleteObject(baseBrush);

    if (pressed || hovered)
    {
        HRGN region =
            CreateRoundRectRgn(
                rect.left + 2,
                rect.top + 2,
                rect.right - 1,
                rect.bottom - 1,
                8,
                8
            );

        HBRUSH highlightBrush =
            CreateSolidBrush(
                pressed
                ? HB_COLOR_HOVER
                : HB_COLOR_TAB_HOVER
            );

        FillRgn(
            dc,
            region,
            highlightBrush
        );

        DeleteObject(highlightBrush);
        DeleteObject(region);
    }

    bool bookmarked =
        false;

    if (
        m_activeTab >= 0 &&
        m_activeTab < static_cast<int>(m_tabs.size())
        )
    {
        bookmarked =
            IsBookmarked(
                m_tabs[m_activeTab]->url
            );
    }

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        bookmarked
        ? HB_COLOR_ACCENT
        : HB_COLOR_TEXT_SECONDARY
    );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                m_uiFont
            )
            );

    DrawTextW(
        dc,
        bookmarked
        ? L"\u2605"
        : L"\u2606",
        -1,
        &rect,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ================================================================
// DRAW TAB BUTTON
// ================================================================
// Chrome-style tab: rounded top corners, the active tab rendered
// brighter (so it visually "merges" into the toolbar below it) with
// a thin accent line on top, inactive tabs darker and lightening
// slightly on hover.

void Window::DrawTabButtonVisual(
    LPDRAWITEMSTRUCT drawItem,
    int index
)
{
    HDC dc =
        drawItem->hDC;

    RECT rect =
        drawItem->rcItem;

    bool active =
        (index == m_activeTab);

    bool hovered =
        (index == m_hoverTab) &&
        !active;

    // Fill with the tab-bar color first so the square corners
    // clipped away from the rounded tab shape below show the tab
    // strip background instead of a leftover highlight color.

    FillRect(
        dc,
        &rect,
        m_tabBarBrush
    );

    COLORREF fill =
        active
        ? HB_COLOR_TAB_ACTIVE
        : (hovered ? HB_COLOR_TAB_HOVER : HB_COLOR_TAB);

    HRGN shape =
        CreateRoundRectRgn(
            rect.left,
            rect.top,
            rect.right,
            rect.bottom + 12,
            8,
            8
        );

    HRGN clip =
        CreateRectRgn(
            rect.left,
            rect.top,
            rect.right,
            rect.bottom
        );

    CombineRgn(
        shape,
        shape,
        clip,
        RGN_AND
    );

    HBRUSH fillBrush =
        CreateSolidBrush(fill);

    FillRgn(
        dc,
        shape,
        fillBrush
    );

    DeleteObject(fillBrush);
    DeleteObject(shape);
    DeleteObject(clip);

    if (active)
    {
        RECT accent =
        {
            rect.left + 8,
            rect.top,
            rect.right - 8,
            rect.top + 3
        };

        HBRUSH accentBrush =
            CreateSolidBrush(HB_COLOR_ACCENT);

        FillRect(
            dc,
            &accent,
            accentBrush
        );

        DeleteObject(accentBrush);
    }

    wchar_t text[256] = {};

    GetWindowTextW(
        drawItem->hwndItem,
        text,
        256
    );

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        active
        ? HB_COLOR_TEXT
        : HB_COLOR_TEXT_SECONDARY
    );

    RECT textRect =
        rect;

    textRect.left += 12;
    textRect.right -= 30;

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                m_tabFont
            )
            );

    DrawTextW(
        dc,
        text,
        -1,
        &textRect,
        DT_LEFT |
        DT_VCENTER |
        DT_SINGLELINE |
        DT_END_ELLIPSIS |
        DT_NOPREFIX
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ================================================================
// DRAW TAB CLOSE BUTTON
// ================================================================
// The × always sits on top of its tab's background, and shows a
// circular highlight on hover - red like Chrome's, so it's obvious
// what a click there will do (and only there - it never affects
// anything outside that one tab).

void Window::DrawTabCloseButtonVisual(
    LPDRAWITEMSTRUCT drawItem,
    int index
)
{
    HDC dc =
        drawItem->hDC;

    RECT rect =
        drawItem->rcItem;

    bool active =
        (index == m_activeTab);

    bool tabHovered =
        (index == m_hoverTab);

    bool closeHovered =
        (index == m_hoverTabClose);

    bool pressed =
        (drawItem->itemState & ODS_SELECTED) != 0;

    COLORREF tabFill =
        active
        ? HB_COLOR_TAB_ACTIVE
        : (tabHovered ? HB_COLOR_TAB_HOVER : HB_COLOR_TAB);

    HBRUSH tabBrush =
        CreateSolidBrush(tabFill);

    FillRect(
        dc,
        &rect,
        tabBrush
    );

    DeleteObject(tabBrush);

    if (closeHovered || pressed)
    {
        HRGN circle =
            CreateEllipticRgn(
                rect.left,
                rect.top,
                rect.right,
                rect.bottom
            );

        HBRUSH circleBrush =
            CreateSolidBrush(
                pressed
                ? HB_COLOR_CLOSE_PRESSED
                : HB_COLOR_CLOSE_HOVER
            );

        FillRgn(
            dc,
            circle,
            circleBrush
        );

        DeleteObject(circleBrush);
        DeleteObject(circle);
    }

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        (closeHovered || pressed)
        ? RGB(255, 255, 255)
        : HB_COLOR_TEXT_SECONDARY
    );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                m_tabFont
            )
            );

    DrawTextW(
        dc,
        L"\u00D7",
        -1,
        &rect,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ================================================================
// BUTTON HOVER SUBCLASS
// ================================================================

void Window::SubclassOwnerDrawButton(
    HWND hwnd
)
{
    if (!hwnd)
    {
        return;
    }

    SetWindowLongPtrW(
        hwnd,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this)
    );

    WNDPROC previous =
        reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                hwnd,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    ButtonHoverProc
                    )
            )
            );

    // The stock "BUTTON" class WNDPROC is the same function
    // pointer for every button, so it only needs to be captured
    // once and can be reused as the chain-to target for all of
    // them.

    if (
        !s_defaultButtonProc &&
        previous
        )
    {
        s_defaultButtonProc = previous;
    }
}

LRESULT CALLBACK Window::ButtonHoverProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    Window* window =
        reinterpret_cast<Window*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
            )
            );

    if (window)
    {
        switch (message)
        {
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT tracking = {};

            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = hwnd;

            TrackMouseEvent(&tracking);

            window->OnButtonHover(
                hwnd,
                true
            );

            break;
        }

        case WM_MOUSELEAVE:
        {
            window->OnButtonHover(
                hwnd,
                false
            );

            break;
        }
        }
    }

    if (s_defaultButtonProc)
    {
        return CallWindowProcW(
            s_defaultButtonProc,
            hwnd,
            message,
            wParam,
            lParam
        );
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

void Window::OnButtonHover(
    HWND hwnd,
    bool hovered
)
{
    if (
        hwnd == m_newTabButton ||
        hwnd == m_backButton ||
        hwnd == m_forwardButton ||
        hwnd == m_reloadButton ||
        hwnd == m_menuButton ||
        hwnd == m_bookmarkButton
        )
    {
        HWND newHover =
            hovered
            ? hwnd
            : nullptr;

        if (m_hoverButtonHwnd != newHover)
        {
            HWND old =
                m_hoverButtonHwnd;

            m_hoverButtonHwnd =
                newHover;

            if (old)
            {
                InvalidateRect(old, nullptr, FALSE);
            }

            if (newHover)
            {
                InvalidateRect(newHover, nullptr, FALSE);
            }
        }

        return;
    }

    for (
        int i = 0;
        i < static_cast<int>(m_tabs.size());
        ++i
        )
    {
        BrowserTab* tab =
            m_tabs[i].get();

        if (!tab)
        {
            continue;
        }

        if (hwnd == tab->tabButton)
        {
            int newHover =
                hovered
                ? i
                : -1;

            if (m_hoverTab != newHover)
            {
                int old =
                    m_hoverTab;

                m_hoverTab =
                    newHover;

                InvalidateTab(old);
                InvalidateTab(i);
            }

            return;
        }

        if (hwnd == tab->closeButton)
        {
            int newHover =
                hovered
                ? i
                : -1;

            if (m_hoverTabClose != newHover)
            {
                m_hoverTabClose =
                    newHover;

                InvalidateRect(hwnd, nullptr, FALSE);
            }

            return;
        }
    }
}

void Window::InvalidateTab(
    int index
)
{
    if (
        index < 0 ||
        index >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[index].get();

    if (!tab)
    {
        return;
    }

    if (tab->tabButton)
    {
        InvalidateRect(tab->tabButton, nullptr, FALSE);
    }

    if (tab->closeButton)
    {
        InvalidateRect(tab->closeButton, nullptr, FALSE);
    }
}

void Window::SetAddressBarFocused(
    bool focused
)
{
    if (m_addressBarFocused == focused)
    {
        return;
    }

    m_addressBarFocused =
        focused;

    if (
        !m_hwnd ||
        !m_addressBar
        )
    {
        return;
    }

    RECT rect = {};

    GetWindowRect(
        m_addressBar,
        &rect
    );

    MapWindowPoints(
        nullptr,
        m_hwnd,
        reinterpret_cast<POINT*>(&rect),
        2
    );

    InflateRect(&rect, 3, 3);

    InvalidateRect(
        m_hwnd,
        &rect,
        FALSE
    );
}

// ================================================================
// CREATE TAB BUTTON
// ================================================================
// Only creates the HWNDs if this tab doesn't already have them.
//
// The previous version of this function unconditionally destroyed
// and recreated both the tab button and its close button every
// time it ran - and it ran on *every* UpdateTabUI() call, which
// fires on every navigation start/finish, i.e. constantly while a
// page is loading. Two real problems came from that:
//
//  1. Every recreation leaked a GDI font handle (see Create()).
//  2. Recreating a button while the user is in the middle of
//     clicking it (mouse down already delivered, button destroyed
//     before mouse up) is exactly the kind of timing bug that can
//     make a click land somewhere it shouldn't. Since the buttons
//     are never destroyed anymore outside of CloseTab() itself,
//     that whole class of problem goes away.

void Window::CreateTabButton(
    int index
)
{
    if (
        index < 0 ||
        index >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[index].get();

    if (
        !tab ||
        tab->tabButton
        )
    {
        // Already created - LayoutTabs() is responsible for
        // moving/resizing/renaming it from here on.
        return;
    }

    std::wstring text =
        tab->title.empty()
        ? L"Nová karta"
        : tab->title;

    // ============================================================
    // TAB
    // ============================================================
    // Position/size is a placeholder - LayoutTabs() places it
    // immediately after this returns.

    tab->tabButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            text.c_str(),
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            0,
            0,
            10,
            10,
            m_hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    10000 + index
                    )
                ),
            m_hInstance,
            nullptr
        );

    // ============================================================
    // CLOSE
    // ============================================================

    tab->closeButton =
        CreateWindowExW(
            0,
            L"BUTTON",
            L"\u00D7",
            WS_CHILD |
            WS_VISIBLE |
            BS_OWNERDRAW,
            0,
            0,
            10,
            10,
            m_hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    20000 + index
                    )
                ),
            m_hInstance,
            nullptr
        );

    SendMessageW(
        tab->tabButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_tabFont),
        TRUE
    );

    SendMessageW(
        tab->closeButton,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(m_tabFont),
        TRUE
    );

    SubclassOwnerDrawButton(tab->tabButton);
    SubclassOwnerDrawButton(tab->closeButton);
}

// ================================================================
// UPDATE TAB UI
// ================================================================

void Window::UpdateTabUI()
{
    if (!m_hwnd)
    {
        return;
    }

    LayoutTabs();

    for (
        int i = 0;
        i < static_cast<int>(m_tabs.size());
        ++i
        )
    {
        BrowserTab* tab =
            m_tabs[i].get();

        if (!tab)
        {
            continue;
        }

        if (tab->tabButton)
        {
            EnableWindow(tab->tabButton, TRUE);
        }

        if (tab->closeButton)
        {
            EnableWindow(tab->closeButton, TRUE);
        }
    }

    ResizeWebView();
}

// ================================================================
// CREATE NEW TAB
// ================================================================

void Window::CreateNewTab(
    const std::wstring& initialUrl
)
{
    if (!m_environment)
    {
        return;
    }

    auto tab =
        std::make_unique<BrowserTab>();

    tab->title =
        L"Nová karta";

    tab->url =
        initialUrl;

    m_tabs.push_back(
        std::move(tab)
    );

    int index =
        static_cast<int>(
            m_tabs.size()
            ) - 1;

    m_activeTab =
        index;

    CreateTabButton(index);

    CreateWebViewForTab(index);

    UpdateTabUI();

    UpdateNavigationButtons();

    UpdateWindowTitle();
}

// ================================================================
// CREATE WEBVIEW
// ================================================================

void Window::CreateWebViewForTab(
    int index
)
{
    if (
        !m_environment ||
        index < 0 ||
        index >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[index].get();

    std::wstring startUrl =
        tab->url.empty()
        ? L"https://www.google.com"
        : tab->url;

    HRESULT hr =
        m_environment->CreateCoreWebView2Controller(
            m_hwnd,
            Callback<
            ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
            >(
                [this, index, startUrl](
                    HRESULT result,
                    ICoreWebView2Controller* controller
                    ) -> HRESULT
                {
                    if (
                        FAILED(result) ||
                        !controller
                        )
                    {
                        return result;
                    }

                    if (
                        index < 0 ||
                        index >= static_cast<int>(m_tabs.size())
                        )
                    {
                        return E_FAIL;
                    }

                    BrowserTab* tab =
                        m_tabs[index].get();

                    tab->controller =
                        controller;

                    HRESULT hr =
                        controller->get_CoreWebView2(
                            &tab->webView
                        );

                    if (FAILED(hr))
                    {
                        return hr;
                    }

                    // ====================================================
                    // SETTINGS
                    // ====================================================

                    ComPtr<ICoreWebView2Settings> settings;

                    if (
                        SUCCEEDED(
                            tab->webView->get_Settings(
                                &settings
                            )
                        )
                        )
                    {
                        settings->put_IsScriptEnabled(
                            TRUE
                        );

                        settings->put_AreDefaultScriptDialogsEnabled(
                            TRUE
                        );

                        settings->put_IsWebMessageEnabled(
                            TRUE
                        );

                        settings->put_AreDevToolsEnabled(
                            TRUE
                        );

                        settings->put_IsStatusBarEnabled(
                            FALSE
                        );

                        settings->put_AreDefaultContextMenusEnabled(
                            TRUE
                        );
                    }

                    // ====================================================
                    // DOWNLOADS
                    // ====================================================
                    // Real download tracking instead of the old
                    // "bude přidáno v další fázi" placeholder: every
                    // download that starts gets an entry in
                    // m_downloads, whose state updates live as the
                    // download progresses/finishes. See ShowDownloads().

                    ComPtr<ICoreWebView2_4> webView4;

                    if (
                        SUCCEEDED(
                            tab->webView.As(&webView4)
                        ) &&
                        webView4
                        )
                    {
                        webView4->add_DownloadStarting(
                            Callback<
                            ICoreWebView2DownloadStartingEventHandler
                            >(
                                [this](
                                    ICoreWebView2*,
                                    ICoreWebView2DownloadStartingEventArgs* args
                                    ) -> HRESULT
                                {
                                    if (!args)
                                    {
                                        return S_OK;
                                    }

                                    ComPtr<ICoreWebView2DownloadOperation> operation;

                                    if (
                                        FAILED(
                                            args->get_DownloadOperation(
                                                &operation
                                            )
                                        ) ||
                                        !operation
                                        )
                                    {
                                        return S_OK;
                                    }

                                    std::wstring fileName =
                                        L"Stahování";

                                    LPWSTR path = nullptr;

                                    if (
                                        SUCCEEDED(
                                            operation->get_ResultFilePath(
                                                &path
                                            )
                                        ) &&
                                        path
                                        )
                                    {
                                        LPCWSTR justName =
                                            PathFindFileNameW(path);

                                        if (justName)
                                        {
                                            fileName = justName;
                                        }

                                        CoTaskMemFree(path);
                                    }

                                    size_t downloadIndex =
                                        m_downloads.size();

                                    m_downloads.push_back(
                                        {
                                            fileName,
                                            L"Probíhá..."
                                        }
                                    );

                                    operation->add_StateChanged(
                                        Callback<
                                        ICoreWebView2StateChangedEventHandler
                                        >(
                                            [this, downloadIndex](
                                                ICoreWebView2DownloadOperation* download,
                                                IUnknown*
                                                ) -> HRESULT
                                            {
                                                if (
                                                    downloadIndex >=
                                                    m_downloads.size()
                                                    )
                                                {
                                                    return S_OK;
                                                }

                                                COREWEBVIEW2_DOWNLOAD_STATE state =
                                                    COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;

                                                if (
                                                    download &&
                                                    SUCCEEDED(
                                                        download->get_State(&state)
                                                    )
                                                    )
                                                {
                                                    switch (state)
                                                    {
                                                    case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED:
                                                        m_downloads[downloadIndex].state =
                                                            L"Dokončeno";
                                                        break;

                                                    case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED:
                                                        m_downloads[downloadIndex].state =
                                                            L"Přerušeno";
                                                        break;

                                                    default:
                                                        m_downloads[downloadIndex].state =
                                                            L"Probíhá...";
                                                        break;
                                                    }
                                                }

                                                return S_OK;
                                            }
                                        ).Get(),
                                        nullptr
                                    );

                                    return S_OK;
                                }
                            ).Get(),
                            nullptr
                        );
                    }

                    // ====================================================
                    // NAVIGATION STARTING
                    // ====================================================

                    tab->webView->add_NavigationStarting(
                        Callback<
                        ICoreWebView2NavigationStartingEventHandler
                        >(
                            [this, index](
                                ICoreWebView2*,
                                ICoreWebView2NavigationStartingEventArgs*
                                ) -> HRESULT
                            {
                                if (
                                    index >= 0 &&
                                    index <
                                    static_cast<int>(
                                        m_tabs.size()
                                        )
                                    )
                                {
                                    m_tabs[index]->title =
                                        L"Načítání...";

                                    m_tabs[index]->historyRecorded =
                                        false;

                                    UpdateTabUI();
                                }

                                return S_OK;
                            }
                        ).Get(),
                        nullptr
                    );

                    // ====================================================
                    // NAVIGATION COMPLETED
                    // ====================================================

                    tab->webView->add_NavigationCompleted(
                        Callback<
                        ICoreWebView2NavigationCompletedEventHandler
                        >(
                            [this, index](
                                ICoreWebView2* sender,
                                ICoreWebView2NavigationCompletedEventArgs*
                                ) -> HRESULT
                            {
                                if (
                                    index < 0 ||
                                    index >=
                                    static_cast<int>(
                                        m_tabs.size()
                                        )
                                    )
                                {
                                    return S_OK;
                                }

                                ComPtr<ICoreWebView2> webView =
                                    sender;

                                LPWSTR source = nullptr;

                                if (
                                    SUCCEEDED(
                                        webView->get_Source(
                                            &source
                                        )
                                    )
                                    )
                                {
                                    m_tabs[index]->url =
                                        source
                                        ? source
                                        : L"";

                                    if (source)
                                    {
                                        CoTaskMemFree(
                                            source
                                        );
                                    }
                                }

                                if (
                                    index ==
                                    m_activeTab
                                    )
                                {
                                    UpdateAddressBar();
                                }

                                UpdateNavigationButtons();

                                return S_OK;
                            }
                        ).Get(),
                        nullptr
                    );

                    // ====================================================
                    // TITLE CHANGED
                    // ====================================================

                    tab->webView->add_DocumentTitleChanged(
                        Callback<
                        ICoreWebView2DocumentTitleChangedEventHandler
                        >(
                            [this, index](
                                ICoreWebView2* sender,
                                IUnknown*
                                ) -> HRESULT
                            {
                                if (
                                    index < 0 ||
                                    index >=
                                    static_cast<int>(
                                        m_tabs.size()
                                        )
                                    )
                                {
                                    return S_OK;
                                }

                                LPWSTR title = nullptr;

                                if (
                                    SUCCEEDED(
                                        sender->get_DocumentTitle(
                                            &title
                                        )
                                    )
                                    )
                                {
                                    if (title)
                                    {
                                        m_tabs[index]->title =
                                            title;

                                        CoTaskMemFree(
                                            title
                                        );
                                    }
                                }

                                // Record the visit exactly once per
                                // navigation, now that the real title
                                // is known. (Previously AddHistoryEntry()
                                // was never called from anywhere, so the
                                // History menu item always reported an
                                // empty history no matter what you'd
                                // visited.)

                                BrowserTab* current =
                                    m_tabs[index].get();

                                if (
                                    current &&
                                    !current->historyRecorded &&
                                    !current->url.empty()
                                    )
                                {
                                    AddHistoryEntry(
                                        current->title,
                                        current->url
                                    );

                                    current->historyRecorded =
                                        true;
                                }

                                UpdateTabUI();

                                if (
                                    index ==
                                    m_activeTab
                                    )
                                {
                                    UpdateWindowTitle();
                                }

                                return S_OK;
                            }
                        ).Get(),
                        nullptr
                    );

                    // ====================================================
                    // VISIBILITY
                    // ====================================================

                    controller->put_IsVisible(
                        index == m_activeTab
                    );

                    ResizeWebView();

                    // ====================================================
                    // INITIAL PAGE
                    // ====================================================

                    tab->webView->Navigate(
                        startUrl.c_str()
                    );

                    return S_OK;
                }
            ).Get()
        );

    if (FAILED(hr))
    {
        MessageBoxW(
            m_hwnd,
            L"Nepodařilo se vytvořit WebView2 Controller.",
            L"Homebax Browser",
            MB_ICONERROR
        );
    }
}

// ================================================================
// SWITCH TAB
// ================================================================

void Window::SwitchToTab(
    int index
)
{
    if (
        index < 0 ||
        index >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    int previous =
        m_activeTab;

    m_activeTab =
        index;

    for (
        int i = 0;
        i < static_cast<int>(m_tabs.size());
        ++i
        )
    {
        if (
            m_tabs[i]->controller
            )
        {
            m_tabs[i]->controller->put_IsVisible(
                i == m_activeTab
            );
        }
    }

    // Repaint just the two tabs whose active state actually
    // changed instead of tearing down and rebuilding every tab
    // button in the strip.

    InvalidateTab(previous);
    InvalidateTab(m_activeTab);

    UpdateAddressBar();

    UpdateNavigationButtons();

    ResizeWebView();

    UpdateWindowTitle();

    SetFocus(
        m_addressBar
    );
}

// ================================================================
// CYCLE TAB
// ================================================================

void Window::CycleTab(
    int direction
)
{
    int count =
        static_cast<int>(m_tabs.size());

    if (count <= 1)
    {
        return;
    }

    int next =
        (m_activeTab + direction + count) % count;

    SwitchToTab(next);
}

// ================================================================
// CLOSE TAB
// ================================================================
// Closes exactly the tab at `index` and nothing else - the window
// itself is only ever closed by the OS's own title-bar × or
// Alt+F4, never as a side effect of closing a tab. If this was the
// last tab, a fresh blank tab replaces it (matches most modern
// browsers - if you'd rather the whole app close on the last tab
// like desktop Chrome does, that's a one-line change, just say
// the word).

void Window::CloseTab(
    int index
)
{
    if (
        index < 0 ||
        index >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[index].get();

    if (tab->controller)
    {
        tab->controller->Close();
    }

    // Release mouse capture before destroying the buttons - if the
    // close click is still "in flight" (button down delivered,
    // button up not yet processed) this guarantees Windows doesn't
    // try to route the matching button-up to a HWND that no longer
    // exists.

    if (GetCapture() == tab->tabButton || GetCapture() == tab->closeButton)
    {
        ReleaseCapture();
    }

    if (tab->tabButton)
    {
        DestroyWindow(
            tab->tabButton
        );
    }

    if (tab->closeButton)
    {
        DestroyWindow(
            tab->closeButton
        );
    }

    m_tabs.erase(
        m_tabs.begin() + index
    );

    // Indices shifted - any stale hover state would now point at
    // the wrong tab, so just clear it. The next WM_MOUSEMOVE
    // recalculates it correctly.

    m_hoverTab = -1;
    m_hoverTabClose = -1;

    if (m_tabs.empty())
    {
        m_activeTab = -1;

        CreateNewTab();

        return;
    }

    if (m_activeTab > index)
    {
        m_activeTab--;
    }
    else if (
        m_activeTab >=
        static_cast<int>(m_tabs.size())
        )
    {
        m_activeTab =
            static_cast<int>(
                m_tabs.size()
                ) - 1;
    }

    LayoutTabs();

    SwitchToTab(
        m_activeTab
    );
}

// ================================================================
// RESIZE WEBVIEW
// ================================================================

void Window::ResizeWebView()
{
    if (
        !m_hwnd ||
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        !tab ||
        !tab->controller
        )
    {
        return;
    }

    RECT rect = {};

    GetClientRect(
        m_hwnd,
        &rect
    );

    RECT webViewRect =
    {
        0,
        TABS_HEIGHT + TOOLBAR_HEIGHT,
        rect.right,
        rect.bottom
    };

    tab->controller->put_Bounds(
        webViewRect
    );

    tab->controller->put_IsVisible(
        TRUE
    );
}

// ================================================================
// NAVIGATE
// ================================================================

void Window::Navigate()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        !tab ||
        !tab->webView
        )
    {
        return;
    }

    int length =
        GetWindowTextLengthW(
            m_addressBar
        );

    if (length <= 0)
    {
        return;
    }

    std::wstring url(
        length + 1,
        L'\0'
    );

    GetWindowTextW(
        m_addressBar,
        url.data(),
        length + 1
    );

    url.resize(
        wcslen(
            url.c_str()
        )
    );

    url =
        NormalizeURL(
            url
        );

    if (url.empty())
    {
        return;
    }

    tab->webView->Navigate(
        url.c_str()
    );
}

// ================================================================
// NORMALIZE URL
// ================================================================

std::wstring Window::NormalizeURL(
    std::wstring url
)
{
    while (
        !url.empty() &&
        (
            url.front() == L' ' ||
            url.front() == L'\t'
            )
        )
    {
        url.erase(
            url.begin()
        );
    }

    while (
        !url.empty() &&
        (
            url.back() == L' ' ||
            url.back() == L'\t'
            )
        )
    {
        url.pop_back();
    }

    if (url.empty())
    {
        return {};
    }

    if (
        url.find(L"http://") == 0 ||
        url.find(L"https://") == 0 ||
        url.find(L"file://") == 0 ||
        url.find(L"about:") == 0
        )
    {
        return url;
    }

    if (
        url.find(L' ') != std::wstring::npos
        )
    {
        std::wstring encoded;

        for (wchar_t c : url)
        {
            if (c == L' ')
            {
                encoded += L'+';
            }
            else
            {
                encoded += c;
            }
        }

        return
            L"https://www.google.com/search?q=" +
            encoded;
    }

    if (
        url.find(L'.') != std::wstring::npos
        )
    {
        return
            L"https://" +
            url;
    }

    return
        L"https://www.google.com/search?q=" +
        url;
}

// ================================================================
// BACK
// ================================================================

void Window::GoBack()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        tab &&
        tab->webView
        )
    {
        BOOL canGoBack = FALSE;

        tab->webView->get_CanGoBack(
            &canGoBack
        );

        if (canGoBack)
        {
            tab->webView->GoBack();
        }
    }
}

// ================================================================
// FORWARD
// ================================================================

void Window::GoForward()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        tab &&
        tab->webView
        )
    {
        BOOL canGoForward = FALSE;

        tab->webView->get_CanGoForward(
            &canGoForward
        );

        if (canGoForward)
        {
            tab->webView->GoForward();
        }
    }
}

// ================================================================
// RELOAD
// ================================================================

void Window::Reload()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        tab &&
        tab->webView
        )
    {
        tab->webView->Reload();
    }
}

// ================================================================
// UPDATE ADDRESS BAR
// ================================================================

void Window::UpdateAddressBar()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (!tab)
    {
        return;
    }

    SetWindowTextW(
        m_addressBar,
        tab->url.c_str()
    );

    // The bookmark star reflects the *active tab's* URL, which
    // just changed.

    if (m_bookmarkButton)
    {
        InvalidateRect(m_bookmarkButton, nullptr, FALSE);
    }
}

// ================================================================
// UPDATE NAVIGATION BUTTONS
// ================================================================

void Window::UpdateNavigationButtons()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        EnableWindow(
            m_backButton,
            FALSE
        );

        EnableWindow(
            m_forwardButton,
            FALSE
        );

        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        !tab ||
        !tab->webView
        )
    {
        return;
    }

    BOOL canGoBack = FALSE;
    BOOL canGoForward = FALSE;

    tab->webView->get_CanGoBack(
        &canGoBack
    );

    tab->webView->get_CanGoForward(
        &canGoForward
    );

    EnableWindow(
        m_backButton,
        canGoBack
    );

    EnableWindow(
        m_forwardButton,
        canGoForward
    );
}

// ================================================================
// UPDATE WINDOW TITLE
// ================================================================

void Window::UpdateWindowTitle()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        SetWindowTextW(
            m_hwnd,
            L"Homebax Browser"
        );

        return;
    }

    std::wstring title =
        m_tabs[m_activeTab]->title;

    if (title.empty())
    {
        title =
            L"Homebax Browser";
    }
    else
    {
        title +=
            L" - Homebax Browser";
    }

    SetWindowTextW(
        m_hwnd,
        title.c_str()
    );
}

// ================================================================
// HISTORY
// ================================================================

void Window::AddHistoryEntry(
    const std::wstring& title,
    const std::wstring& url
)
{
    if (url.empty())
    {
        return;
    }

    HistoryEntry entry;

    entry.title = title;
    entry.url = url;

    m_history.push_back(
        std::move(entry)
    );

    if (m_history.size() > 500)
    {
        m_history.erase(
            m_history.begin()
        );
    }
}

// ================================================================
// SHOW HISTORY
// ================================================================

void Window::ShowHistory()
{
    if (m_history.empty())
    {
        MessageBoxW(
            m_hwnd,
            L"Historie je zatím prázdná.",
            L"Historie",
            MB_OK
        );

        return;
    }

    std::wstring text;

    int count =
        0;

    for (
        auto it = m_history.rbegin();
        it != m_history.rend() &&
        count < 20;
        ++it,
        ++count
        )
    {
        text +=
            it->title.empty()
            ? L"(Bez názvu)"
            : it->title;

        text +=
            L"\r\n";

        text +=
            it->url;

        text +=
            L"\r\n\r\n";
    }

    MessageBoxW(
        m_hwnd,
        text.c_str(),
        L"Historie",
        MB_OK
    );
}

// ================================================================
// BOOKMARKS
// ================================================================

bool Window::IsBookmarked(
    const std::wstring& url
) const
{
    if (url.empty())
    {
        return false;
    }

    for (const auto& bookmark : m_bookmarks)
    {
        if (bookmark.url == url)
        {
            return true;
        }
    }

    return false;
}

void Window::ToggleBookmark()
{
    if (
        m_activeTab < 0 ||
        m_activeTab >= static_cast<int>(m_tabs.size())
        )
    {
        return;
    }

    BrowserTab* tab =
        m_tabs[m_activeTab].get();

    if (
        !tab ||
        tab->url.empty()
        )
    {
        return;
    }

    auto existing =
        std::find_if(
            m_bookmarks.begin(),
            m_bookmarks.end(),
            [&](const BookmarkEntry& entry)
            {
                return entry.url == tab->url;
            }
        );

    if (existing != m_bookmarks.end())
    {
        m_bookmarks.erase(existing);
    }
    else
    {
        m_bookmarks.push_back(
            {
                tab->title,
                tab->url
            }
        );
    }

    if (m_bookmarkButton)
    {
        InvalidateRect(m_bookmarkButton, nullptr, FALSE);
    }
}

void Window::ShowBookmarks()
{
    if (m_bookmarks.empty())
    {
        MessageBoxW(
            m_hwnd,
            L"Zatím nemáte žádné záložky. Přidáte je hvězdičkou vedle adresního řádku nebo Ctrl+D.",
            L"Záložky",
            MB_OK
        );

        return;
    }

    std::wstring text;

    for (const auto& bookmark : m_bookmarks)
    {
        text +=
            bookmark.title.empty()
            ? L"(Bez názvu)"
            : bookmark.title;

        text +=
            L"\r\n";

        text +=
            bookmark.url;

        text +=
            L"\r\n\r\n";
    }

    MessageBoxW(
        m_hwnd,
        text.c_str(),
        L"Záložky",
        MB_OK
    );
}

// ================================================================
// DOWNLOADS
// ================================================================

void Window::ShowDownloads()
{
    if (m_downloads.empty())
    {
        MessageBoxW(
            m_hwnd,
            L"Zatím žádná stahování.",
            L"Stahování",
            MB_OK
        );

        return;
    }

    std::wstring text;

    for (const auto& download : m_downloads)
    {
        text +=
            download.fileName;

        text +=
            L" - ";

        text +=
            download.state;

        text +=
            L"\r\n";
    }

    MessageBoxW(
        m_hwnd,
        text.c_str(),
        L"Stahování",
        MB_OK
    );
}

// ================================================================
// MENU
// ================================================================

void Window::ShowMenu()
{
    HMENU menu =
        CreatePopupMenu();

    AppendMenuW(
        menu,
        MF_STRING,
        2001,
        L"Nová karta\tCtrl+T"
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2002,
        L"Historie"
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2003,
        L"Stahování"
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2004,
        L"Záložky"
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2007,
        L"Přidat/odebrat záložku\tCtrl+D"
    );

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2005,
        L"Vývojářské nástroje\tF12"
    );

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr
    );

    AppendMenuW(
        menu,
        MF_STRING,
        2006,
        L"Nastavení"
    );

    POINT point = {};

    GetCursorPos(
        &point
    );

    TrackPopupMenu(
        menu,
        TPM_RIGHTALIGN |
        TPM_TOPALIGN,
        point.x,
        point.y,
        0,
        m_hwnd,
        nullptr
    );

    DestroyMenu(
        menu
    );
}

// ================================================================
// ADDRESS BAR PROC
// ================================================================

LRESULT CALLBACK Window::AddressBarProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    Window* window =
        reinterpret_cast<Window*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
            )
            );

    if (
        message == WM_KEYDOWN &&
        wParam == VK_RETURN
        )
    {
        if (window)
        {
            window->Navigate();
        }

        return 0;
    }

    if (
        message == WM_SETFOCUS
        )
    {
        SendMessageW(
            hwnd,
            EM_SETSEL,
            0,
            -1
        );

        if (window)
        {
            window->SetAddressBarFocused(true);
        }
    }

    if (
        message == WM_KILLFOCUS
        )
    {
        if (window)
        {
            window->SetAddressBarFocused(false);
        }
    }

    if (
        window &&
        window->m_originalAddressBarProc
        )
    {
        return CallWindowProcW(
            window->m_originalAddressBarProc,
            hwnd,
            message,
            wParam,
            lParam
        );
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ================================================================
// WINDOW PROC
// ================================================================

LRESULT CALLBACK Window::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    Window* window =
        reinterpret_cast<Window*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
            )
            );

    if (
        message == WM_NCCREATE
        )
    {
        CREATESTRUCTW* createStruct =
            reinterpret_cast<CREATESTRUCTW*>(
                lParam
                );

        window =
            static_cast<Window*>(
                createStruct->lpCreateParams
                );

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                window
                )
        );
    }

    switch (message)
    {
        // ========================================================
        // DRAW BUTTON
        // ========================================================

    case WM_DRAWITEM:
    {
        if (window)
        {
            window->DrawButton(
                reinterpret_cast<LPDRAWITEMSTRUCT>(
                    lParam
                    )
            );

            return TRUE;
        }

        break;
    }

    // ========================================================
    // COLOR EDIT
    // ========================================================

    case WM_CTLCOLOREDIT:
    {
        HDC dc =
            reinterpret_cast<HDC>(wParam);

        SetTextColor(
            dc,
            HB_COLOR_TEXT
        );

        SetBkColor(
            dc,
            HB_COLOR_ADDRESS
        );

        static HBRUSH brush =
            CreateSolidBrush(
                HB_COLOR_ADDRESS
            );

        return reinterpret_cast<LRESULT>(
            brush
            );
    }

    // ========================================================
    // ERASE
    // ========================================================

    case WM_ERASEBKGND:
    {
        if (!window)
        {
            break;
        }

        HDC dc =
            reinterpret_cast<HDC>(wParam);

        RECT rect = {};

        GetClientRect(
            hwnd,
            &rect
        );

        // Bug fix: this used to fill with CreateSolidBrush(COLOR_BACKGROUND),
        // which is the *Windows system color index* (value 1), not this
        // app's HB_COLOR_BACKGROUND constant - it painted with essentially
        // black (RGB(1,0,0)) instead of the intended dark gray, and created
        // a brand new brush on every single erase without ever deleting it.
        // Now it reuses the one shared m_backgroundBrush.

        FillRect(
            dc,
            &rect,
            window->m_backgroundBrush
        );

        return 1;
    }

    // ========================================================
    // PAINT
    // ========================================================

    case WM_PAINT:
    {
        PAINTSTRUCT ps = {};

        HDC dc =
            BeginPaint(
                hwnd,
                &ps
            );

        RECT rect = {};

        GetClientRect(
            hwnd,
            &rect
        );

        if (window)
        {
            FillRect(
                dc,
                &rect,
                window->m_backgroundBrush
            );

            RECT tabsRect =
            {
                0,
                0,
                rect.right,
                TABS_HEIGHT
            };

            FillRect(
                dc,
                &tabsRect,
                window->m_tabBarBrush
            );

            RECT toolbarRect =
            {
                0,
                TABS_HEIGHT,
                rect.right,
                TABS_HEIGHT +
                    TOOLBAR_HEIGHT
            };

            FillRect(
                dc,
                &toolbarRect,
                window->m_toolbarBrush
            );

            // Chrome-style address bar focus ring: an accent colored
            // frame drawn just outside the EDIT control's bounds,
            // shown only while it has focus.

            if (
                window->m_addressBarFocused &&
                window->m_addressBar
                )
            {
                RECT addressRect = {};

                GetWindowRect(
                    window->m_addressBar,
                    &addressRect
                );

                MapWindowPoints(
                    nullptr,
                    hwnd,
                    reinterpret_cast<POINT*>(&addressRect),
                    2
                );

                InflateRect(&addressRect, 2, 2);

                HPEN pen =
                    CreatePen(
                        PS_SOLID,
                        2,
                        HB_COLOR_ACCENT
                    );

                HPEN oldPen =
                    static_cast<HPEN>(
                        SelectObject(dc, pen)
                        );

                HBRUSH oldBrush =
                    static_cast<HBRUSH>(
                        SelectObject(dc, GetStockObject(NULL_BRUSH))
                        );

                RoundRect(
                    dc,
                    addressRect.left,
                    addressRect.top,
                    addressRect.right,
                    addressRect.bottom,
                    8,
                    8
                );

                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
        }
        else
        {
            HBRUSH fallback =
                CreateSolidBrush(HB_COLOR_BACKGROUND);

            FillRect(
                dc,
                &rect,
                fallback
            );

            DeleteObject(fallback);
        }

        EndPaint(
            hwnd,
            &ps
        );

        return 0;
    }

    // ========================================================
    // SIZE
    // ========================================================

    case WM_SIZE:
    {
        if (window)
        {
            window->ResizeBrowserUI();
        }

        return 0;
    }

    // ========================================================
    // DOUBLE CLICK - open a new tab when the empty part of the
    // tab strip is double-clicked, like Chrome. Because tab/close/
    // "+" buttons are their own child windows, a click that lands
    // on one of them never reaches here - only genuinely empty
    // tab-strip background does. Requires CS_DBLCLKS on the window
    // class (see Create()).
    // ========================================================

    case WM_LBUTTONDBLCLK:
    {
        if (window)
        {
            int y =
                GET_Y_LPARAM(lParam);

            if (
                y >= 0 &&
                y < TABS_HEIGHT
                )
            {
                window->CreateNewTab();

                return 0;
            }
        }

        break;
    }

    // ========================================================
    // MIDDLE CLICK ON A CHILD CONTROL - used to close a tab by
    // middle-clicking it, like Chrome. Owner-draw BUTTON controls
    // don't report middle-clicks themselves, but Windows still
    // tells the parent via WM_PARENTNOTIFY. For mouse messages,
    // lParam holds the cursor position (not a HWND), so the actual
    // child is found with ChildWindowFromPoint.
    // ========================================================

    case WM_PARENTNOTIFY:
    {
        if (
            window &&
            LOWORD(wParam) == WM_MBUTTONDOWN
            )
        {
            POINT point =
            {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            HWND child =
                ChildWindowFromPoint(
                    hwnd,
                    point
                );

            for (
                int i = 0;
                i < static_cast<int>(window->m_tabs.size());
                ++i
                )
            {
                auto& tab =
                    window->m_tabs[i];

                if (
                    tab &&
                    (
                        child == tab->tabButton ||
                        child == tab->closeButton
                        )
                    )
                {
                    window->CloseTab(i);
                    break;
                }
            }

            return 0;
        }

        break;
    }

    // ========================================================
    // COMMAND
    // ========================================================

    case WM_COMMAND:
    {
        if (!window)
        {
            break;
        }

        HWND control =
            reinterpret_cast<HWND>(
                lParam
                );

        int command =
            LOWORD(wParam);

        // ====================================================
        // NEW TAB
        // ====================================================

        if (
            control ==
            window->m_newTabButton
            )
        {
            window->CreateNewTab();

            return 0;
        }

        // ====================================================
        // BACK
        // ====================================================

        if (
            control ==
            window->m_backButton
            )
        {
            window->GoBack();

            return 0;
        }

        // ====================================================
        // FORWARD
        // ====================================================

        if (
            control ==
            window->m_forwardButton
            )
        {
            window->GoForward();

            return 0;
        }

        // ====================================================
        // RELOAD
        // ====================================================

        if (
            control ==
            window->m_reloadButton
            )
        {
            window->Reload();

            return 0;
        }

        // ====================================================
        // BOOKMARK STAR
        // ====================================================

        if (
            control ==
            window->m_bookmarkButton
            )
        {
            window->ToggleBookmark();

            return 0;
        }

        // ====================================================
        // MENU
        // ====================================================

        if (
            control ==
            window->m_menuButton
            )
        {
            window->ShowMenu();

            return 0;
        }

        // ====================================================
        // TABS
        // ====================================================
        // Which tab (and whether it's the tab itself or its close
        // button) is decided purely by comparing HWNDs against the
        // live m_tabs list - never by treating the numeric control
        // id as a tab index. Ids are assigned once at creation and
        // are never renumbered, so this is always correct even
        // after tabs in the middle of the strip have been closed.

        for (
            int i = 0;
            i < static_cast<int>(window->m_tabs.size());
            ++i
            )
        {
            auto& tab =
                window->m_tabs[i];

            if (!tab)
            {
                continue;
            }

            if (control == tab->tabButton)
            {
                window->SwitchToTab(i);

                return 0;
            }

            if (control == tab->closeButton)
            {
                window->CloseTab(i);

                return 0;
            }
        }

        // ====================================================
        // MENU ITEMS
        // ====================================================

        switch (command)
        {
        case 2001:
        {
            window->CreateNewTab();

            return 0;
        }

        case 2002:
        {
            window->ShowHistory();

            return 0;
        }

        case 2003:
        {
            window->ShowDownloads();

            return 0;
        }

        case 2004:
        {
            window->ShowBookmarks();

            return 0;
        }

        case 2005:
        {
            if (
                window->m_activeTab >= 0 &&
                window->m_activeTab <
                static_cast<int>(
                    window->m_tabs.size()
                    )
                )
            {
                auto& tab =
                    window->m_tabs[
                        window->m_activeTab
                    ];

                if (
                    tab &&
                    tab->webView
                    )
                {
                    tab->webView->
                        OpenDevToolsWindow();
                }
            }

            return 0;
        }

        case 2006:
        {
            MessageBoxW(
                window->m_hwnd,
                L"Nastavení bude přidáno později.",
                L"Nastavení",
                MB_OK
            );

            return 0;
        }

        case 2007:
        {
            window->ToggleBookmark();

            return 0;
        }
        }

        break;
    }

    // ========================================================
    // KEYBOARD
    // ========================================================

    case WM_KEYDOWN:
    {
        if (!window)
        {
            break;
        }

        if (
            GetKeyState(VK_CONTROL) & 0x8000
            )
        {
            // Ctrl + L
            if (wParam == 'L')
            {
                SetFocus(
                    window->m_addressBar
                );

                SendMessageW(
                    window->m_addressBar,
                    EM_SETSEL,
                    0,
                    -1
                );

                return 0;
            }

            // Ctrl + T
            if (wParam == 'T')
            {
                window->CreateNewTab();

                return 0;
            }

            // Ctrl + W
            if (wParam == 'W')
            {
                if (
                    window->m_activeTab >= 0
                    )
                {
                    window->CloseTab(
                        window->m_activeTab
                    );
                }

                return 0;
            }

            // Ctrl + R
            if (wParam == 'R')
            {
                window->Reload();

                return 0;
            }

            // Ctrl + D - toggle bookmark for the active tab
            if (wParam == 'D')
            {
                window->ToggleBookmark();

                return 0;
            }

            // Ctrl + Tab / Ctrl + Shift + Tab - cycle tabs
            if (wParam == VK_TAB)
            {
                bool shift =
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                window->CycleTab(
                    shift ? -1 : 1
                );

                return 0;
            }

            // Ctrl + 1..8 - jump straight to that tab
            if (
                wParam >= '1' &&
                wParam <= '8'
                )
            {
                int index =
                    static_cast<int>(wParam - '1');

                if (
                    index <
                    static_cast<int>(window->m_tabs.size())
                    )
                {
                    window->SwitchToTab(index);
                }

                return 0;
            }

            // Ctrl + 9 - jump to the last tab
            if (wParam == '9')
            {
                if (!window->m_tabs.empty())
                {
                    window->SwitchToTab(
                        static_cast<int>(window->m_tabs.size()) - 1
                    );
                }

                return 0;
            }
        }

        // F12
        if (wParam == VK_F12)
        {
            if (
                window->m_activeTab >= 0 &&
                window->m_activeTab <
                static_cast<int>(
                    window->m_tabs.size()
                    )
                )
            {
                auto& tab =
                    window->m_tabs[
                        window->m_activeTab
                    ];

                if (
                    tab &&
                    tab->webView
                    )
                {
                    tab->webView->
                        OpenDevToolsWindow();
                }
            }

            return 0;
        }

        break;
    }

    // ========================================================
    // DESTROY
    // ========================================================

    case WM_DESTROY:
    {
        PostQuitMessage(0);

        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ================================================================
// RUN
// ================================================================

int Window::Run()
{
    MSG message = {};

    while (
        GetMessageW(
            &message,
            nullptr,
            0,
            0
        )
        )
    {
        TranslateMessage(
            &message
        );

        DispatchMessageW(
            &message
        );
    }

    return static_cast<int>(
        message.wParam
        );
}
