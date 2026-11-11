#include "WebViewManager.h"

#include <algorithm>

#include "../core/Log.h"
#include "../core/Util.h"
#include "../../../platform/windows/WinUtil.h"

using Microsoft::WRL::Callback;

namespace hb {

namespace {

constexpr wchar_t kUiUrl[] = L"https://hb.local/ui/shell.html";

std::wstring FindFixedRuntime()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = win::ExeDir() / L"WebView2Runtime";
    if (!fs::is_directory(root, ec)) return {};
    if (fs::exists(root / L"msedgewebview2.exe", ec)) return root.wstring();
    for (const auto& entry : fs::directory_iterator(root, ec))
        if (entry.is_directory(ec) && fs::exists(entry.path() / L"msedgewebview2.exe", ec)) return entry.path().wstring();
    return {};
}

std::string Str(const wil::unique_cotaskmem_string& s) { return s ? win::Narrow(s.get()) : std::string(); }

}  // namespace

WebViewManager::WebViewManager(HWND uiHost, HWND contentHost, IWebViewSink& sink, std::filesystem::path userDataDir,
                               std::filesystem::path resourcesDir)
    : uiHost_(uiHost), contentHost_(contentHost), sink_(sink), userDataDir_(std::move(userDataDir)),
      resourcesDir_(std::move(resourcesDir))
{
}

WebViewManager::~WebViewManager()
{
    for (auto& [id, v] : views_)
        if (v.controller) v.controller->Close();
    views_.clear();
    if (ui_.controller) ui_.controller->Close();
}

// ----------------------------------------------------------------- environment

void WebViewManager::Initialize(const std::wstring& language)
{
    if (initialized_) return;
    initialized_ = true;

    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    if (!language.empty()) options->put_Language(language.c_str());

    const std::wstring fixedRuntime = FindFixedRuntime();
    if (!fixedRuntime.empty()) HB_INFO("Using bundled WebView2 runtime: {}", win::Narrow(fixedRuntime));

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        fixedRuntime.empty() ? nullptr : fixedRuntime.c_str(), userDataDir_.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result) || !env)
                {
                    sink_.OnEnvironmentFailed(FAILED(result) ? result : E_FAIL);
                    return S_OK;
                }
                env_ = env;
                sink_.OnEnvironmentReady();
                return S_OK;
            }).Get());
    if (FAILED(hr)) sink_.OnEnvironmentFailed(hr);
}

void WebViewManager::CreateController(HWND host, bool isPrivate, std::function<void(ICoreWebView2Controller*)> done)
{
    auto handler = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [done](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT
        {
            if (FAILED(hr)) HB_WARN("CreateCoreWebView2Controller failed: 0x{:08x}", static_cast<unsigned>(hr));
            done(SUCCEEDED(hr) ? controller : nullptr);
            return S_OK;
        });

    if (isPrivate)
    {
        if (auto env10 = env_.try_query<ICoreWebView2Environment10>())
        {
            wil::com_ptr<ICoreWebView2ControllerOptions> opts;
            if (SUCCEEDED(env10->CreateCoreWebView2ControllerOptions(opts.put())))
            {
                opts->put_IsInPrivateModeEnabled(TRUE);
                if (SUCCEEDED(env10->CreateCoreWebView2ControllerWithOptions(host, opts.get(), handler.Get()))) return;
            }
        }
        privateSupported_ = false;  // old WebView2 runtime/SDK: fall back to a normal tab
    }
    env_->CreateCoreWebView2Controller(host, handler.Get());
}

// ------------------------------------------------------------------- creation

void WebViewManager::CreateUi()
{
    if (!env_ || ui_.controller) return;
    CreateController(uiHost_, false, [this](ICoreWebView2Controller* c)
    {
        if (!c) { sink_.OnEnvironmentFailed(E_FAIL); return; }
        ui_.controller = c;
        c->get_CoreWebView2(ui_.webView.put());
        SetupCommon(0, ui_, true);
        ui_.controller->put_Bounds(uiBounds_);
        ui_.controller->put_IsVisible(TRUE);
        ui_.webView->Navigate(kUiUrl);
    });
}

void WebViewManager::CreateTab(int id, const std::wstring& url, bool isPrivate, std::shared_ptr<NewWindowAdopt> adopt)
{
    auto abort = [&adopt]()
    {
        if (!adopt) return;
        adopt->args->put_Handled(TRUE);
        adopt->deferral->Complete();
    };
    if (!env_ || views_.count(id) || pending_.count(id)) { abort(); return; }
    pending_.insert(id);
    cancelled_.erase(id);

    CreateController(contentHost_, isPrivate, [this, id, url, isPrivate, adopt, abort](ICoreWebView2Controller* c)
    {
        pending_.erase(id);
        if (!c || cancelled_.erase(id))
        {
            if (c) c->Close();
            abort();
            return;
        }
        View v;
        v.controller = c;
        v.isPrivate = isPrivate;
        c->get_CoreWebView2(v.webView.put());
        View& view = views_[id] = std::move(v);

        SetupCommon(id, view, false);
        view.controller->put_Bounds(contentBounds_);
        view.controller->put_IsVisible(id == activeId_);

        if (adopt)
        {
            // Popup (window.open): hand the new view to the page that asked for it so window.opener keeps working.
            adopt->args->put_NewWindow(view.webView.get());
            adopt->args->put_Handled(TRUE);
            adopt->deferral->Complete();
        }
        else if (!url.empty())
            view.webView->Navigate(url.c_str());

        sink_.OnTabReady(id);
    });
}

void WebViewManager::CloseTab(int id)
{
    if (pending_.count(id)) cancelled_.insert(id);
    auto it = views_.find(id);
    if (it == views_.end()) return;
    if (it->second.controller) it->second.controller->Close();
    views_.erase(it);
}

// -------------------------------------------------------------- configuration

void WebViewManager::ApplySettings(View& v, bool isUi)
{
    wil::com_ptr<ICoreWebView2Settings> st;
    if (FAILED(v.webView->get_Settings(st.put()))) return;
    st->put_IsScriptEnabled(TRUE);
    st->put_IsWebMessageEnabled(TRUE);
    st->put_AreHostObjectsAllowed(FALSE);
    st->put_AreDefaultScriptDialogsEnabled(TRUE);
    st->put_IsStatusBarEnabled(!isUi);
    st->put_IsZoomControlEnabled(!isUi);
    st->put_AreDefaultContextMenusEnabled(!isUi);
    if (auto s3 = st.try_query<ICoreWebView2Settings3>()) s3->put_AreBrowserAcceleratorKeysEnabled(!isUi);
    if (auto s6 = st.try_query<ICoreWebView2Settings6>()) s6->put_IsSwipeNavigationEnabled(!isUi);
#ifdef NDEBUG
    st->put_AreDevToolsEnabled(!isUi && options_.devTools);
#else
    st->put_AreDevToolsEnabled(TRUE);
#endif
    if (auto s4 = st.try_query<ICoreWebView2Settings4>())
    {
        s4->put_IsPasswordAutosaveEnabled(!isUi && options_.passwordAutosave);
        s4->put_IsGeneralAutofillEnabled(!isUi && options_.autofill);
    }
}

void WebViewManager::ApplyProfile(View& v)
{
    auto w13 = v.webView.try_query<ICoreWebView2_13>();
    if (!w13) return;
    wil::com_ptr<ICoreWebView2Profile> profile;
    if (FAILED(w13->get_Profile(profile.put())) || !profile) return;

    static const COREWEBVIEW2_PREFERRED_COLOR_SCHEME schemes[] = {
        COREWEBVIEW2_PREFERRED_COLOR_SCHEME_AUTO, COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT,
        COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK};
    profile->put_PreferredColorScheme(schemes[std::clamp(options_.colorScheme, 0, 2)]);

    if (auto p3 = profile.try_query<ICoreWebView2Profile3>())
    {
        static const COREWEBVIEW2_TRACKING_PREVENTION_LEVEL levels[] = {
            COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_NONE, COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_BASIC,
            COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_BALANCED, COREWEBVIEW2_TRACKING_PREVENTION_LEVEL_STRICT};
        p3->put_PreferredTrackingPreventionLevel(levels[std::clamp(options_.tracking, 0, 3)]);
    }
}

void WebViewManager::SetOptions(const ViewOptions& options)
{
    options_ = options;
    if (ui_.webView) { ApplySettings(ui_, true); ApplyProfile(ui_); }
    for (auto& [id, v] : views_) { ApplySettings(v, false); ApplyProfile(v); }
}

void WebViewManager::SetUiBackground(COLORREF color)
{
    uiBackground_ = color;
    if (!ui_.controller) return;
    if (auto c2 = ui_.controller.try_query<ICoreWebView2Controller2>())
        c2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{255, GetRValue(color), GetGValue(color), GetBValue(color)});
}

void WebViewManager::SetupCommon(int id, View& v, bool isUi)
{
    ICoreWebView2* wv = v.webView.get();
    ICoreWebView2Controller* ctl = v.controller.get();
    EventRegistrationToken token{};

    ApplySettings(v, isUi);
    ApplyProfile(v);

    if (auto w3 = v.webView.try_query<ICoreWebView2_3>())
        w3->SetVirtualHostNameToFolderMapping(L"hb.local", resourcesDir_.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    if (isUi) SetUiBackground(uiBackground_);

    ctl->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [this, id](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* a) -> HRESULT
            {
                COREWEBVIEW2_KEY_EVENT_KIND kind{};
                a->get_KeyEventKind(&kind);
                if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN && kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) return S_OK;
                UINT vk = 0;
                a->get_VirtualKey(&vk);
                COREWEBVIEW2_PHYSICAL_KEY_STATUS status{};
                a->get_PhysicalKeyStatus(&status);
                if (sink_.OnAccelerator(id, vk, status.WasKeyDown != 0)) a->put_Handled(TRUE);
                return S_OK;
            }).Get(), &token);

    if (isUi)
    {
        wv->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT
                {
                    wil::unique_cotaskmem_string src, msg;
                    a->get_Source(src.put());
                    a->get_WebMessageAsJson(msg.put());
                    if (StartsWith(Str(src), "https://hb.local/ui/")) sink_.OnUiMessage(Str(msg));
                    return S_OK;
                }).Get(), &token);

        // The browser UI must never navigate away or open windows.
        wv->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* a) -> HRESULT
                {
                    wil::unique_cotaskmem_string uri;
                    a->get_Uri(uri.put());
                    if (!StartsWith(Str(uri), "https://hb.local/ui/")) a->put_Cancel(TRUE);
                    return S_OK;
                }).Get(), &token);
        wv->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT
                {
                    a->put_Handled(TRUE);
                    return S_OK;
                }).Get(), &token);
        return;
    }

    wv->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT
            {
                wil::unique_cotaskmem_string src, msg;
                a->get_Source(src.put());
                a->get_WebMessageAsJson(msg.put());
                sink_.OnPageMessage(id, Str(src), Str(msg));
                return S_OK;
            }).Get(), &token);

    wv->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* a) -> HRESULT
            {
                wil::unique_cotaskmem_string uri;
                a->get_Uri(uri.put());
                sink_.OnNavigationStarting(id, Str(uri));
                return S_OK;
            }).Get(), &token);

    wv->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* a) -> HRESULT
            {
                BOOL ok = FALSE;
                a->get_IsSuccess(&ok);
                sink_.OnNavigationCompleted(id, ok != FALSE);
                return S_OK;
            }).Get(), &token);

    wv->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [this, id](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT
            {
                wil::unique_cotaskmem_string uri;
                sender->get_Source(uri.put());
                sink_.OnSourceChanged(id, Str(uri));
                return S_OK;
            }).Get(), &token);

    wv->add_DocumentTitleChanged(
        Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
            [this, id](ICoreWebView2* sender, IUnknown*) -> HRESULT
            {
                wil::unique_cotaskmem_string title;
                sender->get_DocumentTitle(title.put());
                sink_.OnTitleChanged(id, Str(title));
                return S_OK;
            }).Get(), &token);

    wv->add_HistoryChanged(
        Callback<ICoreWebView2HistoryChangedEventHandler>(
            [this, id](ICoreWebView2* sender, IUnknown*) -> HRESULT
            {
                BOOL back = FALSE, forward = FALSE;
                sender->get_CanGoBack(&back);
                sender->get_CanGoForward(&forward);
                sink_.OnHistoryChanged(id, back != FALSE, forward != FALSE);
                return S_OK;
            }).Get(), &token);

    if (auto w15 = v.webView.try_query<ICoreWebView2_15>())
        w15->add_FaviconChanged(
            Callback<ICoreWebView2FaviconChangedEventHandler>(
                [this, id](ICoreWebView2* sender, IUnknown*) -> HRESULT
                {
                    wil::com_ptr<ICoreWebView2> self(sender);
                    if (auto f = self.try_query<ICoreWebView2_15>())
                    {
                        wil::unique_cotaskmem_string uri;
                        f->get_FaviconUri(uri.put());
                        sink_.OnFaviconChanged(id, Str(uri));
                    }
                    return S_OK;
                }).Get(), &token);

    if (auto w8 = v.webView.try_query<ICoreWebView2_8>())
        w8->add_IsDocumentPlayingAudioChanged(
            Callback<ICoreWebView2IsDocumentPlayingAudioChangedEventHandler>(
                [this, id](ICoreWebView2* sender, IUnknown*) -> HRESULT
                {
                    wil::com_ptr<ICoreWebView2> self(sender);
                    if (auto a8 = self.try_query<ICoreWebView2_8>())
                    {
                        BOOL playing = FALSE;
                        a8->get_IsDocumentPlayingAudio(&playing);
                        sink_.OnAudioChanged(id, playing != FALSE);
                    }
                    return S_OK;
                }).Get(), &token);

    wv->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT
            {
                wil::unique_cotaskmem_string uri;
                a->get_Uri(uri.put());
                BOOL user = FALSE;
                a->get_IsUserInitiated(&user);
                bool priv = false;
                const int newId = sink_.OnNewWindowRequested(id, Str(uri), user != FALSE, priv);
                if (newId < 0)
                {
                    a->put_Handled(TRUE);
                    return S_OK;
                }
                auto adopt = std::make_shared<NewWindowAdopt>();
                adopt->args = a;
                a->GetDeferral(adopt->deferral.put());
                CreateTab(newId, L"", priv, adopt);
                return S_OK;
            }).Get(), &token);

    wv->add_WindowCloseRequested(
        Callback<ICoreWebView2WindowCloseRequestedEventHandler>(
            [this, id](ICoreWebView2*, IUnknown*) -> HRESULT
            {
                sink_.OnCloseRequested(id);
                return S_OK;
            }).Get(), &token);

    wv->add_ContainsFullScreenElementChanged(
        Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
            [this, id](ICoreWebView2* sender, IUnknown*) -> HRESULT
            {
                BOOL on = FALSE;
                sender->get_ContainsFullScreenElement(&on);
                if (id == activeId_) sink_.OnFullscreenChanged(on != FALSE);
                return S_OK;
            }).Get(), &token);

    wv->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [this, id](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* a) -> HRESULT
            {
                COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
                a->get_ProcessFailedKind(&kind);
                sink_.OnProcessFailed(id, kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED ||
                                              kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE);
                return S_OK;
            }).Get(), &token);

    if (auto w4 = v.webView.try_query<ICoreWebView2_4>())
        w4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [this, id](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* a) -> HRESULT
                {
                    wil::com_ptr<ICoreWebView2DownloadOperation> op;
                    if (SUCCEEDED(a->get_DownloadOperation(op.put())) && op) sink_.OnDownloadStarting(id, op.get(), a);
                    return S_OK;
                }).Get(), &token);

    ctl->add_GotFocus(
        Callback<ICoreWebView2FocusChangedEventHandler>(
            [this, id](ICoreWebView2Controller*, IUnknown*) -> HRESULT
            {
                sink_.OnContentFocused(id);
                return S_OK;
            }).Get(), &token);

    ctl->add_ZoomFactorChanged(
        Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
            [this, id](ICoreWebView2Controller* c, IUnknown*) -> HRESULT
            {
                double zoom = 1.0;
                c->get_ZoomFactor(&zoom);
                sink_.OnZoomChanged(id, zoom);
                return S_OK;
            }).Get(), &token);
}

// ------------------------------------------------------------------ operations

void WebViewManager::ResumeIfSuspended(View& v)
{
    if (!v.suspended) return;
    if (auto w3 = v.webView.try_query<ICoreWebView2_3>()) w3->Resume();
    v.suspended = false;
}

void WebViewManager::ShowTab(int id)
{
    activeId_ = id;
    for (auto& [vid, v] : views_)
    {
        const bool visible = vid == id;
        if (visible) ResumeIfSuspended(v);
        v.controller->put_IsVisible(visible);
    }
}

void WebViewManager::Navigate(int id, const std::wstring& url)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->Navigate(url.c_str());
}

void WebViewManager::GoBack(int id)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->GoBack();
}

void WebViewManager::GoForward(int id)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->GoForward();
}

void WebViewManager::Reload(int id, bool ignoreCache)
{
    auto it = views_.find(id);
    if (it == views_.end()) return;
    if (ignoreCache)
        it->second.webView->CallDevToolsProtocolMethod(L"Page.reload", L"{\"ignoreCache\":true}", nullptr);
    else
        it->second.webView->Reload();
}

void WebViewManager::Stop(int id)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->Stop();
}

void WebViewManager::ExecuteScript(int id, const std::wstring& js, std::function<void(std::string)> done)
{
    auto it = views_.find(id);
    if (it == views_.end()) { if (done) done({}); return; }
    it->second.webView->ExecuteScript(
        js.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [done](HRESULT hr, LPCWSTR result) -> HRESULT
            {
                if (done) done(SUCCEEDED(hr) && result ? win::Narrow(result) : std::string());
                return S_OK;
            }).Get());
}

void WebViewManager::OpenDevTools(int id)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->OpenDevToolsWindow();
}

void WebViewManager::FocusTab(int id)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void WebViewManager::FocusUi()
{
    if (ui_.controller) ui_.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void WebViewManager::SetContentBounds(const RECT& r)
{
    contentBounds_ = r;
    for (auto& [id, v] : views_) v.controller->put_Bounds(r);
}

void WebViewManager::SetUiBounds(const RECT& r)
{
    uiBounds_ = r;
    if (ui_.controller) ui_.controller->put_Bounds(r);
}

double WebViewManager::Zoom(int id) const
{
    auto it = views_.find(id);
    double zoom = 1.0;
    if (it != views_.end()) it->second.controller->get_ZoomFactor(&zoom);
    return zoom;
}

void WebViewManager::SetZoom(int id, double factor)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.controller->put_ZoomFactor(std::clamp(factor, 0.25, 5.0));
}

void WebViewManager::SetMuted(int id, bool muted)
{
    auto it = views_.find(id);
    if (it == views_.end()) return;
    if (auto w8 = it->second.webView.try_query<ICoreWebView2_8>()) w8->put_IsMuted(muted ? TRUE : FALSE);
}

void WebViewManager::Suspend(int id)
{
    auto it = views_.find(id);
    if (it == views_.end() || id == activeId_ || it->second.suspended) return;
    auto w3 = it->second.webView.try_query<ICoreWebView2_3>();
    if (!w3) return;
    it->second.controller->put_IsVisible(FALSE);
    w3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
        [this, id](HRESULT hr, BOOL ok) -> HRESULT
        {
            auto v = views_.find(id);
            if (v != views_.end() && SUCCEEDED(hr) && ok) v->second.suspended = true;
            return S_OK;
        }).Get());
}

bool WebViewManager::IsSuspended(int id) const
{
    auto it = views_.find(id);
    return it != views_.end() && it->second.suspended;
}

void WebViewManager::PostToUi(const std::string& json)
{
    if (ui_.webView) ui_.webView->PostWebMessageAsJson(win::Widen(json).c_str());
}

void WebViewManager::PostToTab(int id, const std::string& json)
{
    auto it = views_.find(id);
    if (it != views_.end()) it->second.webView->PostWebMessageAsJson(win::Widen(json).c_str());
}

void WebViewManager::ClearBrowsingData(unsigned kinds, std::function<void(bool)> done)
{
    wil::com_ptr<ICoreWebView2Profile2> profile;
    if (auto w13 = ui_.webView.try_query<ICoreWebView2_13>())
    {
        wil::com_ptr<ICoreWebView2Profile> p;
        if (SUCCEEDED(w13->get_Profile(p.put()))) profile = p.try_query<ICoreWebView2Profile2>();
    }
    if (!profile) { if (done) done(false); return; }

    unsigned mask = 0;
    if (kinds & kClearCookies) mask |= COREWEBVIEW2_BROWSING_DATA_KINDS_COOKIES;
    if (kinds & kClearSiteData) mask |= COREWEBVIEW2_BROWSING_DATA_KINDS_ALL_DOM_STORAGE;
    if (kinds & kClearCache) mask |= COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE;
    if (kinds & kClearPasswords) mask |= COREWEBVIEW2_BROWSING_DATA_KINDS_PASSWORD_AUTOSAVE;
    if (kinds & kClearAutofill) mask |= COREWEBVIEW2_BROWSING_DATA_KINDS_GENERAL_AUTOFILL;
    if (!mask) { if (done) done(true); return; }

    profile->ClearBrowsingData(
        static_cast<COREWEBVIEW2_BROWSING_DATA_KINDS>(mask),
        Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>(
            [done](HRESULT hr) -> HRESULT
            {
                if (done) done(SUCCEEDED(hr));
                return S_OK;
            }).Get());
}

}  // namespace hb
