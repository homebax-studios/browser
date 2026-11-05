#pragma once
#include <windows.h>
#include <wrl.h>

#include <wil/com.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hb {

// Everything WebViewManager reports back. Tab id 0 means "the browser UI view".
class IWebViewSink
{
public:
    virtual ~IWebViewSink() = default;
    virtual void OnEnvironmentReady() = 0;
    virtual void OnEnvironmentFailed(HRESULT hr) = 0;
    virtual void OnUiMessage(const std::string& jsonText) = 0;
    virtual void OnPageMessage(int tabId, const std::string& source, const std::string& jsonText) = 0;
    virtual void OnTabReady(int tabId) = 0;
    virtual void OnNavigationStarting(int tabId, const std::string& url) = 0;
    virtual void OnNavigationCompleted(int tabId, bool success) = 0;
    virtual void OnSourceChanged(int tabId, const std::string& url) = 0;
    virtual void OnTitleChanged(int tabId, const std::string& title) = 0;
    virtual void OnFaviconChanged(int tabId, const std::string& uri) = 0;
    virtual void OnHistoryChanged(int tabId, bool canBack, bool canForward) = 0;
    virtual void OnAudioChanged(int tabId, bool playing) = 0;
    // Returns the id of a new (already modelled) tab that should host the popup, or -1 to block it.
    virtual int OnNewWindowRequested(int fromTab, const std::string& uri, bool userInitiated, bool& isPrivate) = 0;
    virtual void OnCloseRequested(int tabId) = 0;
    virtual void OnFullscreenChanged(bool on) = 0;
    virtual void OnZoomChanged(int tabId, double zoom) = 0;
    virtual void OnContentFocused(int tabId) = 0;
    virtual bool OnAccelerator(int tabId, UINT vk, bool repeat) = 0;
    virtual void OnDownloadStarting(int tabId, ICoreWebView2DownloadOperation* op, ICoreWebView2DownloadStartingEventArgs* args) = 0;
    virtual void OnProcessFailed(int tabId, bool renderCrashed) = 0;
};

struct ViewOptions
{
    int colorScheme = 0;       // 0 system, 1 light, 2 dark
    int tracking = 2;          // 0 off, 1 basic, 2 balanced, 3 strict
    bool passwordAutosave = false;
    bool autofill = false;
    bool devTools = true;
};

struct NewWindowAdopt
{
    wil::com_ptr<ICoreWebView2NewWindowRequestedEventArgs> args;
    wil::com_ptr<ICoreWebView2Deferral> deferral;
};

enum BrowsingData : unsigned
{
    kClearCookies = 1u << 0,
    kClearSiteData = 1u << 1,
    kClearCache = 1u << 2,
    kClearPasswords = 1u << 3,
    kClearAutofill = 1u << 4,
};

// Thin wrapper around WebView2: one environment, one view for the browser UI and one view per tab.
class WebViewManager
{
public:
    WebViewManager(HWND uiHost, HWND contentHost, IWebViewSink& sink, std::filesystem::path userDataDir,
                   std::filesystem::path resourcesDir);
    ~WebViewManager();

    void Initialize(const std::wstring& language);
    void CreateUi();
    void CreateTab(int id, const std::wstring& url, bool isPrivate, std::shared_ptr<NewWindowAdopt> adopt = nullptr);
    void CloseTab(int id);
    bool HasTab(int id) const { return views_.count(id) != 0; }
    bool PrivateSupported() const noexcept { return privateSupported_; }

    void ShowTab(int id);
    void Navigate(int id, const std::wstring& url);
    void GoBack(int id);
    void GoForward(int id);
    void Reload(int id, bool ignoreCache);
    void Stop(int id);
    void ExecuteScript(int id, const std::wstring& js, std::function<void(std::string)> done = {});
    void OpenDevTools(int id);
    void FocusTab(int id);
    void FocusUi();

    void SetContentBounds(const RECT& r);
    void SetUiBounds(const RECT& r);
    void SetUiBackground(COLORREF color);
    void SetOptions(const ViewOptions& options);

    double Zoom(int id) const;
    void SetZoom(int id, double factor);
    void SetMuted(int id, bool muted);
    void Suspend(int id);
    bool IsSuspended(int id) const;

    void PostToUi(const std::string& utf8Json);
    void PostToTab(int id, const std::string& utf8Json);
    void ClearBrowsingData(unsigned kinds, std::function<void(bool)> done);

private:
    struct View
    {
        wil::com_ptr<ICoreWebView2Controller> controller;
        wil::com_ptr<ICoreWebView2> webView;
        bool isPrivate = false;
        bool suspended = false;
    };

    void CreateController(HWND host, bool isPrivate, std::function<void(ICoreWebView2Controller*)> done);
    void SetupCommon(int id, View& v, bool isUi);
    void ApplySettings(View& v, bool isUi);
    void ApplyProfile(View& v);
    void ResumeIfSuspended(View& v);

    HWND uiHost_;
    HWND contentHost_;
    IWebViewSink& sink_;
    std::filesystem::path userDataDir_;
    std::filesystem::path resourcesDir_;

    wil::com_ptr<ICoreWebView2Environment> env_;
    View ui_;
    std::unordered_map<int, View> views_;
    RECT contentBounds_{0, 0, 100, 100};
    RECT uiBounds_{0, 0, 100, 100};
    COLORREF uiBackground_ = RGB(28, 29, 33);
    ViewOptions options_;
    std::unordered_set<int> pending_;    // views that are still being created
    std::unordered_set<int> cancelled_;  // closed while they were being created
    int activeId_ = -1;
    bool privateSupported_ = true;
    bool initialized_ = false;
};

}  // namespace hb
