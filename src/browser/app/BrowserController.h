#pragma once
#include <windows.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../BrowserWindow.h"
#include "../core/Settings.h"
#include "../core/Storage.h"
#include "../core/TabManager.h"
#include "../webview/WebViewManager.h"

namespace hb {

// The brain of the browser: owns tabs, settings, storage and the WebViews, executes every command
// (from the browser shell UI, keyboard shortcuts, internal pages) and pushes state back to the UI.
class BrowserController final : public IWebViewSink
{
public:
    using json = nlohmann::json;

    BrowserController(BrowserWindow& window, StartupOptions options);
    ~BrowserController() override;

    bool Start();
    void Shutdown();

    // From the window
    void OnLayout(const RECT& ui, const RECT& content);
    void OnTimer(UINT_PTR id);
    void OnSystemThemeChanged();
    void OnWindowStateChanged();
    void OpenExternal(const std::vector<std::string>& urls);
    void FocusActive();
    void OnClosing();
    void OnUpdateResult(std::string* payload);

    // IWebViewSink
    void OnEnvironmentReady() override;
    void OnEnvironmentFailed(HRESULT hr) override;
    void OnUiMessage(const std::string& jsonText) override;
    void OnPageMessage(int tabId, const std::string& source, const std::string& jsonText) override;
    void OnTabReady(int tabId) override;
    void OnNavigationStarting(int tabId, const std::string& url) override;
    void OnNavigationCompleted(int tabId, bool success) override;
    void OnSourceChanged(int tabId, const std::string& url) override;
    void OnTitleChanged(int tabId, const std::string& title) override;
    void OnFaviconChanged(int tabId, const std::string& uri) override;
    void OnHistoryChanged(int tabId, bool canBack, bool canForward) override;
    void OnAudioChanged(int tabId, bool playing) override;
    int OnNewWindowRequested(int fromTab, const std::string& uri, bool userInitiated, bool& isPrivate) override;
    void OnCloseRequested(int tabId) override;
    void OnFullscreenChanged(bool on) override;
    void OnZoomChanged(int tabId, double zoom) override;
    void OnContentFocused(int tabId) override;
    bool OnAccelerator(int tabId, UINT vk, bool repeat) override;
    void OnDownloadStarting(int tabId, ICoreWebView2DownloadOperation* op, ICoreWebView2DownloadStartingEventArgs* args) override;
    void OnProcessFailed(int tabId, bool renderCrashed) override;

private:
    struct DownloadItem
    {
        wil::com_ptr<ICoreWebView2DownloadOperation> op;
        int64_t dbId = 0;
        int64_t received = 0;
        int64_t total = 0;
        std::string state = "progress";  // progress | paused | done | failed | canceled
        std::string path;
        bool cancelRequested = false;
    };

    // commands
    void Exec(const std::string& cmd, const json& args = json::object());
    int NewTab(const std::string& input, bool isPrivate, bool activate, int afterId = -1, bool resolve = true);
    void CloseTab(int id);
    void ActivateTab(int id);
    void EnsureView(int id);
    void NavigateTab(int id, const std::string& input, bool typed);
    void OpenPage(const std::string& name);
    void ToggleBookmark();
    void Zoom(int direction);  // -1, 0 (reset), +1
    void Find(const json& args);
    void SetUserFullscreen(bool on);

    // pages (hb://...) RPC
    json Rpc(int tabId, const std::string& id, const std::string& cmd, const json& args, bool& deferred);
    void Reply(int tabId, const std::string& id, bool ok, const json& result);

    // state
    json BuildState();
    void PushState();
    void RequestPush();
    void PostUi(const json& message);
    void Toast(const std::string& key);
    void UpdateTheme();
    void ApplyViewOptions();
    void OnSettingChanged(const std::string& key);
    void NotifyPages(const std::string& what);
    void ApplyZoom(int tabId);
    void SaveSession();
    void StartTabs();
    json DownloadsJson();
    void ShowWindowTitle();
    void Defer(std::function<void()> fn);
    void PushDownloads();

    std::string LanguageTag() const;
    std::filesystem::path DownloadDir() const;
    std::string SecurityOf(const Tab& t) const;
    bool IsInternalTab(int id) const;

    BrowserWindow& window_;
    StartupOptions options_;
    std::filesystem::path dataDir_;
    std::filesystem::path resDir_;
    std::unique_ptr<Settings> settings_;
    Storage storage_;
    TabManager tabs_;
    std::unique_ptr<WebViewManager> views_;
    std::map<int64_t, DownloadItem> downloads_;

    bool uiReady_ = false;
    bool envReady_ = false;
    bool dark_ = false;
    bool pushScheduled_ = false;
    bool bookmarksDirty_ = true;
    bool downloadsDirty_ = false;
    bool sessionDirty_ = false;
    bool uiFocused_ = false;
    bool userFullscreen_ = false;
    bool ignoreZoomEvent_ = false;
    bool shuttingDown_ = false;
    int tick_ = 0;
    std::string lastTyped_;
    std::string findText_;
    std::vector<std::string> pendingUrls_;
    std::map<int, int> crashCount_;
    std::vector<std::function<void()>> deferred_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

}  // namespace hb
