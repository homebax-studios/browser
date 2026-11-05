#include "BrowserController.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <thread>

#include "../core/Log.h"
#include "../core/Navigation.h"
#include "../core/Util.h"
#include "../../../platform/windows/WinUtil.h"

#ifndef HB_VERSION
#define HB_VERSION "2.1.2"
#endif

using Microsoft::WRL::Callback;

namespace hb {

namespace {

constexpr UINT_PTR kTimerHousekeeping = 1;
constexpr UINT_PTR kTimerPush = 2;
constexpr UINT_PTR kTimerDefer = 3;
const char* kUpdateUrl = "https://homebax.vercel.app/browser/info.json";  // {"version":"x.y.z","url":"https://..."}

const COLORREF kFrameDark = RGB(28, 29, 33);
const COLORREF kFrameLight = RGB(222, 226, 234);

std::string Dump(const nlohmann::json& j) { return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace); }

const std::vector<int>& ZoomLevels()
{
    static const std::vector<int> levels = {25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400, 500};
    return levels;
}

}  // namespace

BrowserController::BrowserController(BrowserWindow& window, StartupOptions options)
    : window_(window), options_(std::move(options))
{
}

BrowserController::~BrowserController() { *alive_ = false; }

// ---------------------------------------------------------------- lifecycle

bool BrowserController::Start()
{
    dataDir_ = win::ResolveDataDir();
    resDir_ = win::FindResources();
    InitLog(dataDir_ / L"logs" / L"browser.log");
    HB_INFO("HB Browser {} starting, data dir: {}", HB_VERSION, win::Narrow(dataDir_.wstring()));

    std::error_code ec;
    if (!std::filesystem::exists(resDir_ / L"ui" / L"chrome.html", ec))
    {
        MessageBoxW(window_.Handle(), (L"Missing resources folder:\n" + resDir_.wstring()).c_str(), L"HB Browser", MB_ICONERROR);
        return false;
    }

    settings_ = std::make_unique<Settings>(dataDir_ / L"settings.json");
    settings_->Load();
    settings_->onChanged = [this](const std::string& key) { OnSettingChanged(key); };

    storage_.Open(dataDir_ / L"browser.db");
    storage_.FailPendingDownloads();

    const json w = settings_->Get<json>("window");
    window_.ApplyPlacement(w.value("x", 0), w.value("y", 0), w.value("w", 0), w.value("h", 0), w.value("maximized", false));

    pendingUrls_ = options_.urls;
    views_ = std::make_unique<WebViewManager>(window_.UiHost(), window_.ContentHost(), *this, dataDir_ / L"WebView2", resDir_);
    UpdateTheme();
    ApplyViewOptions();
    views_->Initialize(win::Widen(LanguageTag()));
    return true;
}

void BrowserController::OnClosing()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;
    SaveSession();

    int x, y, w, h;
    bool maximized;
    window_.GetPlacement(x, y, w, h, maximized);
    settings_->Set("window", json{{"x", x}, {"y", y}, {"w", w}, {"h", h}, {"maximized", maximized}});

    if (settings_->Get<bool>("clearOnExit"))
    {
        storage_.ClearHistory(0);
        storage_.ClearDownloads();
    }
}

void BrowserController::Shutdown()
{
    shuttingDown_ = true;
    downloads_.clear();
    views_.reset();
    HB_INFO("HB Browser stopped");
}

std::string BrowserController::LanguageTag() const
{
    const auto lang = settings_->Get<std::string>("language");
    if (lang == "cs") return "cs";
    if (lang == "en") return "en-US";
    return "";
}

std::filesystem::path BrowserController::DownloadDir() const
{
    const auto custom = settings_->Get<std::string>("downloadDir");
    std::error_code ec;
    if (!custom.empty())
    {
        const std::filesystem::path p = win::Widen(custom);
        if (std::filesystem::is_directory(p, ec)) return p;
    }
    return win::DownloadsDir();
}

// ------------------------------------------------------------------- theme

void BrowserController::UpdateTheme()
{
    const auto mode = settings_->Get<std::string>("theme");
    dark_ = mode == "dark" || (mode != "light" && win::SystemUsesDarkTheme());
    const COLORREF frame = dark_ ? kFrameDark : kFrameLight;
    window_.SetTheme(dark_, frame);
    if (views_) views_->SetUiBackground(frame);
}

void BrowserController::ApplyViewOptions()
{
    ViewOptions o;
    const auto theme = settings_->Get<std::string>("theme");
    o.colorScheme = theme == "light" ? 1 : theme == "dark" ? 2 : 0;
    const auto tp = settings_->Get<std::string>("trackingPrevention");
    o.tracking = tp == "off" ? 0 : tp == "basic" ? 1 : tp == "strict" ? 3 : 2;
    o.passwordAutosave = settings_->Get<bool>("passwordAutosave");
    o.autofill = settings_->Get<bool>("generalAutofill");
    o.devTools = settings_->Get<bool>("devTools");
    views_->SetOptions(o);
}

void BrowserController::OnSettingChanged(const std::string& key)
{
    if (key == "theme" || key == "accent")
    {
        UpdateTheme();
        ApplyViewOptions();
    }
    else if (key == "trackingPrevention" || key == "passwordAutosave" || key == "generalAutofill" || key == "devTools")
        ApplyViewOptions();
    NotifyPages("settings");
    RequestPush();
}

void BrowserController::OnSystemThemeChanged()
{
    if (settings_->Get<std::string>("theme") != "system") return;
    UpdateTheme();
    ApplyViewOptions();
    NotifyPages("settings");
    PushState();
}

void BrowserController::NotifyPages(const std::string& what)
{
    if (!views_) return;
    const std::string msg = Dump(json{{"type", "event"}, {"name", what}});
    for (const Tab& t : tabs_.All())
        if (IsInternalUrl(t.url) && views_->HasTab(t.id)) views_->PostToTab(t.id, msg);
}

// --------------------------------------------------------------- state push

std::string BrowserController::SecurityOf(const Tab& t) const
{
    if (IsInternalUrl(t.url)) return "internal";
    const std::string scheme = SchemeOf(t.url);
    if (scheme == "https") return "secure";
    if (scheme == "http") return "insecure";
    if (scheme == "file") return "file";
    return "none";
}

bool BrowserController::IsInternalTab(int id) const
{
    const Tab* t = tabs_.Find(id);
    return t && IsInternalUrl(t->url);
}

nlohmann::json BrowserController::BuildState()
{
    json s;
    s["type"] = "state";
    json list = json::array();
    for (const Tab& t : tabs_.All())
        list.push_back({{"id", t.id}, {"title", t.title}, {"url", ToDisplay(t.url)}, {"favicon", t.favicon},
                        {"loading", t.loading}, {"pinned", t.pinned}, {"muted", t.muted}, {"audible", t.audible},
                        {"sleeping", t.sleeping}, {"private", t.isPrivate}});
    s["tabs"] = list;
    s["activeId"] = tabs_.ActiveId();

    if (const Tab* a = tabs_.Active())
    {
        const double zoom = views_ ? views_->Zoom(a->id) : 1.0;
        s["nav"] = {{"canBack", a->canGoBack}, {"canForward", a->canGoForward}, {"loading", a->loading},
                    {"url", ToDisplay(a->url)}, {"title", a->title}, {"security", SecurityOf(*a)},
                    {"bookmarked", storage_.IsBookmarked(a->url)}, {"zoom", static_cast<int>(zoom * 100 + 0.5)},
                    {"private", a->isPrivate}};
    }
    s["window"] = {{"maximized", window_.IsMaximized()}, {"fullscreen", window_.IsFullscreen()}};
    s["dark"] = dark_;
    s["settings"] = {{"accent", settings_->Get<std::string>("accent")},
                     {"theme", settings_->Get<std::string>("theme")},
                     {"language", settings_->Get<std::string>("language")},
                     {"showBookmarksBar", settings_->Get<bool>("showBookmarksBar")},
                     {"showHomeButton", settings_->Get<bool>("showHomeButton")}};
    if (bookmarksDirty_)
    {
        s["bookmarks"] = storage_.Bookmarks();
        bookmarksDirty_ = false;
    }
    return s;
}

void BrowserController::PushState()
{
    if (!uiReady_ || shuttingDown_) return;
    pushScheduled_ = false;
    KillTimer(window_.Handle(), kTimerPush);
    views_->PostToUi(Dump(BuildState()));
}

void BrowserController::RequestPush()
{
    if (!uiReady_ || pushScheduled_) return;
    pushScheduled_ = true;
    SetTimer(window_.Handle(), kTimerPush, 40, nullptr);
}

void BrowserController::PostUi(const json& message)
{
    if (uiReady_ && views_) views_->PostToUi(Dump(message));
}

void BrowserController::Toast(const std::string& key) { PostUi({{"type", "toast"}, {"key", key}}); }

void BrowserController::Defer(std::function<void()> fn)
{
    deferred_.push_back(std::move(fn));
    SetTimer(window_.Handle(), kTimerDefer, 1, nullptr);
}

void BrowserController::ShowWindowTitle()
{
    const Tab* t = tabs_.Active();
    std::string title = t && !t->title.empty() ? t->title : "HB Browser";
    if (t && t->isPrivate) title += " (InPrivate)";
    if (title != "HB Browser") title += " - HB Browser";
    window_.SetTitle(win::Widen(title));
}

// ------------------------------------------------------------- window events

void BrowserController::OnLayout(const RECT& ui, const RECT& content)
{
    if (!views_) return;
    views_->SetUiBounds(ui);
    views_->SetContentBounds(content);
}

void BrowserController::OnWindowStateChanged() { RequestPush(); }

void BrowserController::FocusActive()
{
    if (!views_ || !uiReady_) return;
    if (uiFocused_) views_->FocusUi();
    else views_->FocusTab(tabs_.ActiveId());
}

void BrowserController::OnTimer(UINT_PTR id)
{
    if (shuttingDown_) return;
    if (id == kTimerPush)
    {
        PushState();
        return;
    }
    if (id == kTimerDefer)
    {
        KillTimer(window_.Handle(), kTimerDefer);
        auto work = std::move(deferred_);
        deferred_.clear();
        for (auto& fn : work) fn();
        return;
    }
    if (id != kTimerHousekeeping) return;

    ++tick_;
    if (downloadsDirty_) PushDownloads();
    if (tick_ % 10 == 0 && sessionDirty_) SaveSession();

    if (tick_ % 20 == 0 && settings_->Get<bool>("memorySaver"))
    {
        // Memory saver: suspend tabs you have not looked at for a while, drop them completely much later.
        const int64_t minutes = std::max(1, settings_->Get<int>("sleepAfterMinutes"));
        const int64_t now = NowMs();
        std::vector<int> discard;
        for (Tab& t : tabs_.All_Mutable())
        {
            if (t.id == tabs_.ActiveId() || t.audible || t.loading || !views_->HasTab(t.id)) continue;
            const int64_t idle = now - t.lastActive;
            if (idle > minutes * 3 * 60000) discard.push_back(t.id);
            else if (idle > minutes * 60000 && !views_->IsSuspended(t.id))
            {
                views_->Suspend(t.id);
                t.sleeping = true;
            }
        }
        for (int tid : discard)
        {
            views_->CloseTab(tid);  // the model keeps url/title; the page reloads when you come back
            if (Tab* t = tabs_.Find(tid)) t->sleeping = true;
        }
        if (!discard.empty() || tick_ % 40 == 0) RequestPush();
    }
}

// ------------------------------------------------------------------ startup

void BrowserController::OnEnvironmentReady() { views_->CreateUi(); }

void BrowserController::OnEnvironmentFailed(HRESULT hr)
{
    wchar_t msg[600];
    swprintf_s(msg,
               L"HB Browser needs the Microsoft Edge WebView2 Runtime and could not start it (error 0x%08X).\n\n"
               L"Install the \"Evergreen\" runtime from developer.microsoft.com/microsoft-edge/webview2\n"
               L"or copy a Fixed Version runtime into the \"WebView2Runtime\" folder next to HB Browser.exe.",
               static_cast<unsigned>(hr));
    HB_WARN("WebView2 environment failed: 0x{:08x}", static_cast<unsigned>(hr));
    MessageBoxW(window_.Handle(), msg, L"HB Browser", MB_ICONERROR);
    PostMessageW(window_.Handle(), WM_CLOSE, 0, 0);
}

void BrowserController::StartTabs()
{
    const std::string startup = settings_->Get<std::string>("startup");

    if (!pendingUrls_.empty())
    {
        auto urls = std::move(pendingUrls_);
        pendingUrls_.clear();
        for (const auto& u : urls) NewTab(u, options_.privateMode, true);
    }
    else if (options_.privateMode)
        NewTab("", true, true);
    else if (startup == "restore")
    {
        std::ifstream in(dataDir_ / L"session.json", std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        const json session = json::parse(ss.str(), nullptr, false);
        if (session.is_object() && session.contains("tabs") && session["tabs"].is_array())
        {
            const int activeIndex = session.value("active", 0);
            int index = 0;
            for (const auto& item : session["tabs"])
            {
                const std::string url = item.value("url", "");
                if (url.empty()) continue;
                const bool active = index == activeIndex;
                const int id = tabs_.Add(url, false, active);
                if (Tab* t = tabs_.Find(id))
                {
                    t->title = item.value("title", "");
                    t->sleeping = !active;  // inactive tabs are only created when you open them
                }
                if (item.value("pinned", false)) tabs_.SetPinned(id, true);
                ++index;
            }
            if (tabs_.Count() > 0) ActivateTab(tabs_.ActiveId());
        }
    }

    if (tabs_.Count() == 0)
        NewTab(startup == "home" ? settings_->Get<std::string>("homepage") : "", false, true);
    PushState();
}

void BrowserController::OpenExternal(const std::vector<std::string>& urls)
{
    if (!uiReady_)
    {
        pendingUrls_.insert(pendingUrls_.end(), urls.begin(), urls.end());
        return;
    }
    for (const auto& u : urls) NewTab(u, false, true);
}

void BrowserController::SaveSession()
{
    sessionDirty_ = false;
    json tabs = json::array();
    int active = 0, index = 0;
    for (const Tab& t : tabs_.All())
    {
        if (t.isPrivate || t.url.empty() || t.url == "about:blank") continue;
        if (t.id == tabs_.ActiveId()) active = index;
        tabs.push_back({{"url", t.url}, {"title", t.title}, {"pinned", t.pinned}});
        ++index;
    }
    std::ofstream out(dataDir_ / L"session.json", std::ios::binary | std::ios::trunc);
    if (out) out << Dump(json{{"tabs", tabs}, {"active", active}});
}

// ------------------------------------------------------------------- tabs

int BrowserController::NewTab(const std::string& input, bool isPrivate, bool activate, int afterId, bool resolve)
{
    const std::string url = resolve ? ResolveInput(input, settings_->Get<std::string>("searchEngine")) : input;
    const int id = tabs_.Add(url, isPrivate, activate, afterId);
    views_->CreateTab(id, win::Widen(url), isPrivate);
    if (activate) views_->ShowTab(id);
    sessionDirty_ = true;
    ShowWindowTitle();
    PushState();
    return id;
}

void BrowserController::EnsureView(int id)
{
    const Tab* t = tabs_.Find(id);
    if (t && !views_->HasTab(id)) views_->CreateTab(id, win::Widen(t->url), t->isPrivate);
}

void BrowserController::ActivateTab(int id)
{
    Tab* t = tabs_.Find(id);
    if (!t) return;
    tabs_.Activate(id);
    t->sleeping = false;
    EnsureView(id);
    views_->ShowTab(id);
    crashCount_.erase(id);
    if (IsInternalUrl(t->url) && ToDisplay(t->url) == "hb://newtab")
    {
        views_->FocusUi();
        uiFocused_ = true;
        PostUi({{"type", "focusOmnibox"}});
    }
    else
    {
        views_->FocusTab(id);
        uiFocused_ = false;
    }
    sessionDirty_ = true;
    ShowWindowTitle();
    PushState();
}

void BrowserController::CloseTab(int id)
{
    if (!tabs_.Find(id)) return;
    const bool wasActive = id == tabs_.ActiveId();
    tabs_.Close(id);
    views_->CloseTab(id);
    if (tabs_.Count() == 0)
    {
        PostMessageW(window_.Handle(), WM_CLOSE, 0, 0);
        return;
    }
    sessionDirty_ = true;
    if (wasActive) ActivateTab(tabs_.ActiveId());
    else PushState();
}

void BrowserController::NavigateTab(int id, const std::string& input, bool typed)
{
    Tab* t = tabs_.Find(id);
    if (!t) return;
    const std::string url = ResolveInput(input, settings_->Get<std::string>("searchEngine"));
    if (typed) lastTyped_ = url;
    t->url = url;
    t->loading = true;
    t->sleeping = false;
    if (views_->HasTab(id)) views_->Navigate(id, win::Widen(url));
    else EnsureView(id);
    sessionDirty_ = true;
    PushState();
}

void BrowserController::OpenPage(const std::string& name)
{
    const std::string url = ToInternal("hb://" + name);
    const std::string base = url.substr(0, url.find_first_of("#?"));
    for (const Tab& t : tabs_.All())
        if (StartsWith(t.url, base))
        {
            if (t.url != url) NavigateTab(t.id, ToDisplay(url), false);
            ActivateTab(t.id);
            return;
        }
    const Tab* a = tabs_.Active();
    if (a && IsInternalUrl(a->url) && ToDisplay(a->url) == "hb://newtab") NavigateTab(a->id, ToDisplay(url), false);
    else NewTab(url, false, true, tabs_.ActiveId(), false);
}

void BrowserController::ToggleBookmark()
{
    const Tab* t = tabs_.Active();
    if (!t) return;
    const std::string scheme = SchemeOf(t->url);
    if (scheme != "http" && scheme != "https" && scheme != "file") { Toast("bookmarkNotAllowed"); return; }
    if (storage_.IsBookmarked(t->url))
    {
        storage_.RemoveBookmarkByUrl(t->url);
        Toast("bookmarkRemoved");
    }
    else
    {
        storage_.AddBookmark(t->url, t->title);
        Toast("bookmarkAdded");
    }
    bookmarksDirty_ = true;
    PushState();
}

void BrowserController::ApplyZoom(int tabId)
{
    const Tab* t = tabs_.Find(tabId);
    if (!t || !views_->HasTab(tabId)) return;
    int pct = settings_->Get<int>("defaultZoom");
    const json zs = settings_->Get<json>("zoomSites");
    const std::string host = HostOf(t->url);
    if (!host.empty() && zs.contains(host) && zs[host].is_number()) pct = zs[host].get<int>();
    if (std::abs(views_->Zoom(tabId) * 100 - pct) > 0.5) views_->SetZoom(tabId, pct / 100.0);
}

void BrowserController::Zoom(int direction)
{
    const int id = tabs_.ActiveId();
    if (!views_->HasTab(id)) return;
    const int defaultPct = settings_->Get<int>("defaultZoom");
    if (direction == 0)
    {
        views_->SetZoom(id, defaultPct / 100.0);
        return;
    }
    const int current = static_cast<int>(views_->Zoom(id) * 100 + 0.5);
    const auto& levels = ZoomLevels();
    int target = current;
    if (direction > 0)
    {
        for (int l : levels) if (l > current) { target = l; break; }
        if (target == current) target = levels.back();
    }
    else
    {
        for (auto it = levels.rbegin(); it != levels.rend(); ++it) if (*it < current) { target = *it; break; }
        if (target == current) target = levels.front();
    }
    views_->SetZoom(id, target / 100.0);
}

void BrowserController::OnZoomChanged(int tabId, double zoom)
{
    const Tab* t = tabs_.Find(tabId);
    if (t)
    {
        const std::string host = HostOf(t->url);
        const int pct = static_cast<int>(zoom * 100 + 0.5);
        if (!host.empty())
        {
            json zs = settings_->Get<json>("zoomSites");
            const int stored = zs.contains(host) && zs[host].is_number() ? zs[host].get<int>() : settings_->Get<int>("defaultZoom");
            if (stored != pct)
            {
                if (pct == settings_->Get<int>("defaultZoom")) zs.erase(host);
                else zs[host] = pct;
                settings_->Set("zoomSites", zs);
            }
        }
    }
    if (tabId == tabs_.ActiveId()) RequestPush();
}

void BrowserController::Find(const json& a)
{
    const std::string text = a.value("text", "");
    findText_ = text;
    const bool cs = a.value("caseSensitive", false);
    const bool back = a.value("backwards", false);
    const std::string t = Dump(json(text));
    const std::string js =
        "(function(){var t=" + t + ";if(!t){window.getSelection().removeAllRanges();return '0';}"
        "window.find(t," + (cs ? "true" : "false") + "," + (back ? "true" : "false") + ",true,false,false,false);"
        "var b=document.body?document.body.innerText:'';var h=" + (cs ? "b" : "b.toLowerCase()") + ",n=" +
        (cs ? "t" : "t.toLowerCase()") + ",c=0,i=0;while(n&&(i=h.indexOf(n,i))!==-1){c++;i+=n.length;}return String(c);})()";
    views_->ExecuteScript(tabs_.ActiveId(), win::Widen(js), [this](std::string res)
    {
        int count = 0;
        const json parsed = json::parse(res, nullptr, false);
        if (parsed.is_string()) count = std::atoi(parsed.get<std::string>().c_str());
        PostUi({{"type", "findResult"}, {"count", count}});
    });
}

void BrowserController::SetUserFullscreen(bool on)
{
    userFullscreen_ = on;
    window_.SetFullscreen(on);
    RequestPush();
}

void BrowserController::OnFullscreenChanged(bool on)
{
    userFullscreen_ = on;
    window_.SetFullscreen(on);
    RequestPush();
}

// ---------------------------------------------------------- UI / page messages

void BrowserController::OnUiMessage(const std::string& jsonText)
{
    const json j = json::parse(jsonText, nullptr, false);
    if (!j.is_object() || !j.contains("cmd") || !j["cmd"].is_string()) return;
    const std::string cmd = j["cmd"].get<std::string>();
    if (cmd == "ready")
    {
        uiReady_ = true;
        bookmarksDirty_ = true;
        StartTabs();
        return;
    }
    Exec(cmd, j);
}

void BrowserController::Exec(const std::string& c, const json& a)
{
    if (!views_ || shuttingDown_) return;
    const int aid = tabs_.ActiveId();
    const Tab* act = tabs_.Active();
    const int id = a.contains("id") && a["id"].is_number_integer() ? a["id"].get<int>() : aid;

    // --- window
    if (c == "minimize") window_.Minimize();
    else if (c == "maximize") window_.ToggleMaximize();
    else if (c == "close" || c == "exit") PostMessageW(window_.Handle(), WM_CLOSE, 0, 0);
    else if (c == "drag") window_.StartDrag();
    else if (c == "layout") window_.SetChromeHeight(a.value("height", 88));
    else if (c == "overlay")
    {
        std::vector<RECT> rects;
        if (a.contains("rects") && a["rects"].is_array())
            for (const auto& r : a["rects"])
                if (r.is_array() && r.size() == 4)
                {
                    const int x = static_cast<int>(r[0].get<double>()), y = static_cast<int>(r[1].get<double>());
                    rects.push_back(RECT{x, y, x + static_cast<int>(r[2].get<double>() + 1), y + static_cast<int>(r[3].get<double>() + 1)});
                }
        window_.SetOverlays(std::move(rects));
    }
    else if (c == "uiFocus") uiFocused_ = a.value("focused", false);
    else if (c == "focusContent") { uiFocused_ = false; views_->FocusTab(aid); }
    else if (c == "fullscreen") SetUserFullscreen(!window_.IsFullscreen());

    // --- tabs
    else if (c == "newTab") NewTab(a.value("url", ""), false, true, aid);
    else if (c == "newPrivateTab") NewTab("", true, true);
    else if (c == "newWindow" || c == "newPrivateWindow")
    {
        std::wstring args = L"--new-instance";
        if (c == "newPrivateWindow") args += L" --private";
        const std::wstring cmd = L"\"" + win::ExePath().wstring() + L"\" " + args;
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        std::wstring mutableCmd = cmd;
        if (CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    else if (c == "closeTab") Defer([this, id] { CloseTab(id); });
    else if (c == "reopenClosed")
    {
        ClosedTab ct;
        if (tabs_.PopClosed(ct))
        {
            const int nid = NewTab(ct.url, false, true, -1, false);
            if (ct.pinned) tabs_.SetPinned(nid, true);
            PushState();
        }
    }
    else if (c == "activateTab") ActivateTab(id);
    else if (c == "nextTab") ActivateTab(tabs_.Neighbor(1));
    else if (c == "prevTab") ActivateTab(tabs_.Neighbor(-1));
    else if (c == "tabIndex")
    {
        const int n = a.value("index", 0);
        const int target = n >= 8 ? tabs_.AtIndex(static_cast<int>(tabs_.Count()) - 1) : tabs_.AtIndex(n);
        if (target >= 0) ActivateTab(target);
    }
    else if (c == "moveTab") { tabs_.Move(id, a.value("index", 0)); sessionDirty_ = true; PushState(); }
    else if (c == "pinTab")
    {
        if (const Tab* t = tabs_.Find(id)) tabs_.SetPinned(id, a.value("pinned", !t->pinned));
        sessionDirty_ = true;
        PushState();
    }
    else if (c == "muteTab")
    {
        if (Tab* t = tabs_.Find(id))
        {
            t->muted = !t->muted;
            views_->SetMuted(id, t->muted);
            PushState();
        }
    }
    else if (c == "duplicateTab")
    {
        if (const Tab* t = tabs_.Find(id)) NewTab(t->url, t->isPrivate, true, id, false);
    }
    else if (c == "reloadTab") views_->Reload(id, false);
    else if (c == "closeOthers" || c == "closeRight")
    {
        std::vector<int> victims;
        const int from = tabs_.IndexOf(id);
        for (size_t i = 0; i < tabs_.All().size(); ++i)
        {
            const Tab& t = tabs_.All()[i];
            if (t.id == id || t.pinned) continue;
            if (c == "closeOthers" || static_cast<int>(i) > from) victims.push_back(t.id);
        }
        Defer([this, victims, id]
        {
            for (int v : victims) CloseTab(v);
            if (tabs_.Find(id)) ActivateTab(id);
        });
    }

    // --- navigation
    else if (c == "navigate")
    {
        const std::string text = a.value("text", "");
        if (a.value("newTab", false)) NewTab(text, act && act->isPrivate, true, aid);
        else NavigateTab(aid, text, true);
        uiFocused_ = false;
        views_->FocusTab(tabs_.ActiveId());
    }
    else if (c == "back") views_->GoBack(aid);
    else if (c == "forward") views_->GoForward(aid);
    else if (c == "reload") views_->Reload(aid, false);
    else if (c == "hardReload") views_->Reload(aid, true);
    else if (c == "stop") views_->Stop(aid);
    else if (c == "home") NavigateTab(aid, settings_->Get<std::string>("homepage"), false);
    else if (c == "focusOmnibox")
    {
        views_->FocusUi();
        uiFocused_ = true;
        PostUi({{"type", "focusOmnibox"}});
    }
    else if (c == "focusUi") { views_->FocusUi(); uiFocused_ = true; }
    else if (c == "downloadsRefresh") PushDownloads();
    else if (c == "suggest")
    {
        PostUi({{"type", "suggestions"}, {"seq", a.value("seq", 0)},
                {"items", storage_.Suggest(a.value("q", ""), 6)}});
    }

    // --- page tools
    else if (c == "find") Find(a);
    else if (c == "findClose")
    {
        views_->ExecuteScript(aid, L"window.getSelection().removeAllRanges()");
        views_->FocusTab(aid);
        uiFocused_ = false;
    }
    else if (c == "zoomIn") Zoom(1);
    else if (c == "zoomOut") Zoom(-1);
    else if (c == "zoomReset") Zoom(0);
    else if (c == "devtools") views_->OpenDevTools(aid);
    else if (c == "print") views_->ExecuteScript(aid, L"window.print()");
    else if (c == "viewSource") { if (act) NewTab("view-source:" + act->url, false, true, aid, false); }

    // --- bookmarks, pages, downloads
    else if (c == "bookmark") ToggleBookmark();
    else if (c == "bookmarkRemove")
    {
        storage_.RemoveBookmarkByUrl(a.value("url", ""));
        bookmarksDirty_ = true;
        PushState();
    }
    else if (c == "toggleBookmarksBar")
    {
        settings_->Set("showBookmarksBar", !settings_->Get<bool>("showBookmarksBar"));
    }
    else if (c == "openPage") OpenPage(a.value("name", "newtab"));
    else if (c == "downloadOpen" || c == "downloadShow" || c == "downloadCancel" || c == "downloadPause" ||
             c == "downloadResume" || c == "downloadRemove" || c == "downloadsClear")
    {
        bool deferred = false;
        std::string op = c.substr(c == "downloadsClear" ? 9 : 8);
        op[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(op[0])));
        Rpc(0, "", "downloads." + op, a, deferred);
    }
    else if (c == "setDefaultBrowser")
    {
        win::RegisterAsBrowser(win::ExePath().wstring());
        win::ShellOpen(L"ms-settings:defaultapps");
    }
}

bool BrowserController::OnAccelerator(int tabId, UINT vk, bool repeat)
{
    (void)tabId;
    if (!uiReady_) return false;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    auto run = [&](const char* cmd, json args = json::object()) {
        if (!repeat) Exec(cmd, args);
        return true;
    };

    if (ctrl && !alt)
    {
        switch (vk)
        {
        case 'T': return run(shift ? "reopenClosed" : "newTab");
        case 'N': return run(shift ? "newPrivateWindow" : "newWindow");
        case 'W': return run("closeTab");
        case VK_F4: return run("closeTab");
        case 'L': return run("focusOmnibox");
        case 'K': return run("focusOmnibox");
        case 'F': if (!repeat) PostUi({{"type", "openFind"}}); return true;
        case 'D': return run("bookmark");
        case 'B': return shift ? run("toggleBookmarksBar") : false;
        case 'H': return run("openPage", {{"name", "history"}});
        case 'J': return run("openPage", {{"name", "downloads"}});
        case VK_DELETE: return run("openPage", {{"name", "settings#privacy"}});
        case 'R': return run(shift ? "hardReload" : "reload");
        case VK_F5: return run("hardReload");
        case 'U': return run("viewSource");
        case 'P': return run("print");
        case 'M': return run("muteTab");
        case 'I': return shift ? run("devtools") : false;
        case VK_TAB: return run(shift ? "prevTab" : "nextTab");
        case VK_NEXT: return run("nextTab");
        case VK_PRIOR: return run("prevTab");
        case VK_OEM_PLUS: case VK_ADD: return run("zoomIn");
        case VK_OEM_MINUS: case VK_SUBTRACT: return run("zoomOut");
        case '0': case VK_NUMPAD0: return run("zoomReset");
        case VK_OEM_COMMA: return run("openPage", {{"name", "settings"}});
        default:
            if (vk >= '1' && vk <= '9') return run("tabIndex", {{"index", static_cast<int>(vk - '1')}});
            return false;
        }
    }
    if (alt && !ctrl)
    {
        switch (vk)
        {
        case VK_LEFT: return run("back");
        case VK_RIGHT: return run("forward");
        case VK_HOME: return run("home");
        case 'D': return run("focusOmnibox");
        default: return false;
        }
    }
    switch (vk)
    {
    case VK_F5: return run("reload");
    case VK_F6: return run("focusOmnibox");
    case VK_F11: return run("fullscreen");
    case VK_F12: return run("devtools");
    case VK_ESCAPE:
        if (const Tab* t = tabs_.Active()) if (t->loading && !repeat) Exec("stop");
        return false;  // the page still gets Esc
    default: return false;
    }
}

void BrowserController::OnPageMessage(int tabId, const std::string& source, const std::string& jsonText)
{
    // Only our own internal pages may talk to the browser. Websites can call postMessage too, so check the origin.
    if (!StartsWith(source, "https://hb.local/pages/")) return;
    const json j = json::parse(jsonText, nullptr, false);
    if (!j.is_object() || !j.contains("cmd") || !j["cmd"].is_string()) return;
    const std::string id = j.contains("id") ? Dump(j["id"]) : "null";
    json args = j.contains("args") && j["args"].is_object() ? j["args"] : json::object();

    bool deferred = false;
    json result;
    bool ok = true;
    try { result = Rpc(tabId, id, j["cmd"].get<std::string>(), args, deferred); }
    catch (const std::exception& e) { ok = false; result = e.what(); }
    if (!deferred) Reply(tabId, id, ok, result);
}

void BrowserController::Reply(int tabId, const std::string& id, bool ok, const json& result)
{
    if (!views_ || id.empty()) return;
    std::string out = "{\"type\":\"reply\",\"id\":" + id + ",\"ok\":" + (ok ? "true" : "false") + ",\"result\":" + Dump(result) + "}";
    views_->PostToTab(tabId, out);
}

nlohmann::json BrowserController::Rpc(int tabId, const std::string& id, const std::string& cmd, const json& a, bool& deferred)
{
    if (cmd == "app.info")
    {
        json engines = json::array();
        for (const auto& e : SearchEngines()) engines.push_back({{"id", e.id}, {"name", e.name}});
        LPWSTR ver = nullptr;
        std::string runtime;
        if (SUCCEEDED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver)) && ver)
        {
            runtime = win::Narrow(ver);
            CoTaskMemFree(ver);
        }
        return {{"version", HB_VERSION}, {"runtime", runtime}, {"dataDir", win::Narrow(dataDir_.wstring())},
                {"downloadDir", win::Narrow(DownloadDir().wstring())}, {"dark", dark_}, {"engines", engines},
                {"accent", settings_->Get<std::string>("accent")}, {"tabs", static_cast<int>(tabs_.Count())}};
    }
    if (cmd == "settings.get") return settings_->All();
    if (cmd == "settings.set")
    {
        const std::string key = a.value("key", "");
        if (key == "window" || !a.contains("value")) return false;
        return settings_->Set(key, a["value"]);
    }
    if (cmd == "settings.pickFolder")
    {
        const std::wstring chosen = win::PickFolder(window_.Handle(), DownloadDir().wstring());
        if (!chosen.empty()) settings_->Set("downloadDir", win::Narrow(chosen));
        return win::Narrow(chosen);
    }
    if (cmd == "settings.setDefaultBrowser") { Exec("setDefaultBrowser"); return true; }

    if (cmd == "bookmarks.list") return storage_.Bookmarks();
    if (cmd == "bookmarks.add" || cmd == "bookmarks.remove" || cmd == "bookmarks.update" || cmd == "bookmarks.import")
    {
        json r = true;
        if (cmd == "bookmarks.add") r = storage_.AddBookmark(a.value("url", ""), a.value("title", ""), a.value("folder", "")) > 0;
        else if (cmd == "bookmarks.remove") r = storage_.RemoveBookmark(a.value("id", int64_t{0}));
        else if (cmd == "bookmarks.update")
            r = storage_.UpdateBookmark(a.value("id", int64_t{0}), a.value("title", ""), a.value("url", ""), a.value("folder", ""));
        else r = storage_.ImportBookmarks(a.value("items", json::array()));
        bookmarksDirty_ = true;
        RequestPush();
        return r;
    }

    if (cmd == "history.list") return storage_.History(a.value("q", ""), std::clamp(a.value("limit", 200), 1, 1000), std::max(0, a.value("offset", 0)));
    if (cmd == "history.remove") { storage_.RemoveHistory(a.value("id", int64_t{0})); return true; }
    if (cmd == "history.clear")
    {
        const std::string range = a.value("range", "all");
        const int64_t now = NowMs();
        const int64_t since = range == "hour" ? now - 3600000LL : range == "day" ? now - 86400000LL
                              : range == "week" ? now - 7 * 86400000LL : 0;
        storage_.ClearHistory(since);
        return true;
    }
    if (cmd == "topsites") return storage_.TopSites(a.value("limit", 8));

    if (cmd == "privacy.clear")
    {
        unsigned kinds = 0;
        for (const auto& k : a.value("kinds", json::array()))
        {
            const std::string s = k.is_string() ? k.get<std::string>() : "";
            if (s == "cookies") kinds |= kClearCookies;
            else if (s == "siteData") kinds |= kClearSiteData;
            else if (s == "cache") kinds |= kClearCache;
            else if (s == "passwords") kinds |= kClearPasswords;
            else if (s == "autofill") kinds |= kClearAutofill;
            else if (s == "history") storage_.ClearHistory(0);
            else if (s == "downloads") storage_.ClearDownloads();
        }
        deferred = true;
        views_->ClearBrowsingData(kinds, [this, tabId, id](bool ok) { Reply(tabId, id, true, ok); });
        return nullptr;
    }

    if (cmd == "nav")
    {
        const std::string url = a.value("url", "");
        if (a.value("newTab", false)) NewTab(url, false, true, tabs_.ActiveId());
        else NavigateTab(tabId, url, false);
        return true;
    }
    if (cmd == "openPage") { OpenPage(a.value("name", "newtab")); return true; }

    if (cmd == "downloads.list") return DownloadsJson();
    if (cmd.rfind("downloads.", 0) == 0)
    {
        const int64_t did = a.value("id", int64_t{0});
        const std::string op = cmd.substr(10);
        auto it = downloads_.find(did);
        if (op == "clear")
        {
            storage_.ClearDownloads();
            for (auto i = downloads_.begin(); i != downloads_.end();)
                i = (i->second.state != "progress" && i->second.state != "paused") ? downloads_.erase(i) : std::next(i);
            downloadsDirty_ = true;
            return true;
        }
        if (op == "remove")
        {
            if (it == downloads_.end() || (it->second.state != "progress" && it->second.state != "paused"))
            {
                storage_.RemoveDownload(did);
                if (it != downloads_.end()) downloads_.erase(it);
            }
            downloadsDirty_ = true;
            return true;
        }
        if (op == "cancel" && it != downloads_.end()) { it->second.cancelRequested = true; it->second.op->Cancel(); return true; }
        if (op == "pause" && it != downloads_.end()) { it->second.op->Pause(); it->second.state = "paused"; downloadsDirty_ = true; return true; }
        if (op == "resume" && it != downloads_.end()) { it->second.op->Resume(); it->second.state = "progress"; downloadsDirty_ = true; return true; }
        if (op == "open" || op == "show")
        {
            for (const auto& row : storage_.Downloads())
                if (row.value("id", int64_t{0}) == did)
                {
                    const std::filesystem::path p = win::Widen(row.value("path", ""));
                    if (op == "show") win::ShowInFolder(p);
                    else if (row.value("state", "") == "done") win::ShellOpen(p.wstring());
                    return true;
                }
        }
        return false;
    }

    if (cmd == "update.check")
    {
        deferred = true;
        const HWND hwnd = window_.Handle();
        std::thread([hwnd, tabId, id]
        {
            std::string body;
            json out = {{"tab", tabId}, {"id", id}, {"ok", false}, {"result", nullptr}};
            if (win::HttpGet(win::Widen(kUpdateUrl), body))
            {
                const json info = json::parse(body, nullptr, false);
                if (info.is_object() && info.contains("version") && info["version"].is_string())
                {
                    const std::string latest = info["version"].get<std::string>();
                    out["ok"] = true;
                    out["result"] = {{"latest", latest}, {"current", HB_VERSION},
                                     {"available", CompareVersions(latest, HB_VERSION) > 0},
                                     {"url", info.value("url", "")}};
                }
            }
            auto* payload = new std::string(Dump(out));
            if (!PostMessageW(hwnd, BrowserWindow::kMsgUpdateResult, 0, reinterpret_cast<LPARAM>(payload))) delete payload;
        }).detach();
        return nullptr;
    }

    throw std::runtime_error("unknown command: " + cmd);
}

void BrowserController::OnUpdateResult(std::string* payload)
{
    std::unique_ptr<std::string> owned(payload);
    const json j = json::parse(*owned, nullptr, false);
    if (!j.is_object()) return;
    // Only reply with the update URL when it is http(s).
    json result = j.value("result", json());
    if (result.is_object())
    {
        const std::string scheme = SchemeOf(result.value("url", ""));
        if (scheme != "http" && scheme != "https") result["url"] = "";
    }
    const std::string id = j.value("id", "null");
    Reply(j.value("tab", 0), id, j.value("ok", false), result);
}

// ----------------------------------------------------------------- downloads

nlohmann::json BrowserController::DownloadsJson()
{
    json rows = storage_.Downloads();
    for (auto& row : rows)
    {
        const int64_t did = row.value("id", int64_t{0});
        auto it = downloads_.find(did);
        if (it != downloads_.end())
        {
            row["received"] = it->second.received;
            row["total"] = it->second.total;
            row["state"] = it->second.state;
        }
        const std::filesystem::path p = win::Widen(row.value("path", ""));
        row["name"] = win::Narrow(p.filename().wstring());
    }
    return rows;
}

void BrowserController::PushDownloads()
{
    downloadsDirty_ = false;
    json rows = DownloadsJson();
    while (rows.size() > 8) rows.erase(rows.size() - 1);
    PostUi({{"type", "downloads"}, {"items", rows}});
}

void BrowserController::OnDownloadStarting(int, ICoreWebView2DownloadOperation* op, ICoreWebView2DownloadStartingEventArgs* args)
{
    wil::unique_cotaskmem_string uri, mime, defaultPath;
    op->get_Uri(uri.put());
    op->get_MimeType(mime.put());
    args->get_ResultFilePath(defaultPath.put());
    INT64 total = 0;
    op->get_TotalBytesToReceive(&total);

    std::filesystem::path path = defaultPath ? std::filesystem::path(defaultPath.get()) : std::filesystem::path(L"download");
    if (!settings_->Get<bool>("askDownloadLocation"))
    {
        path = win::UniquePath(DownloadDir(), path.filename().wstring());
        args->put_ResultFilePath(path.c_str());
        args->put_Handled(TRUE);  // we show our own download UI
    }

    const int64_t dbId = storage_.AddDownload(uri ? win::Narrow(uri.get()) : "", win::Narrow(path.wstring()),
                                              mime ? win::Narrow(mime.get()) : "", total);
    DownloadItem item;
    item.op = op;
    item.dbId = dbId;
    item.total = total;
    item.path = win::Narrow(path.wstring());
    downloads_[dbId] = std::move(item);

    EventRegistrationToken token{};
    op->add_BytesReceivedChanged(
        Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
            [this, dbId](ICoreWebView2DownloadOperation* o, IUnknown*) -> HRESULT
            {
                auto it = downloads_.find(dbId);
                if (it == downloads_.end()) return S_OK;
                INT64 received = 0, tot = 0;
                o->get_BytesReceived(&received);
                o->get_TotalBytesToReceive(&tot);
                it->second.received = received;
                it->second.total = tot;
                downloadsDirty_ = true;
                return S_OK;
            }).Get(), &token);

    op->add_StateChanged(
        Callback<ICoreWebView2StateChangedEventHandler>(
            [this, dbId](ICoreWebView2DownloadOperation* o, IUnknown*) -> HRESULT
            {
                auto it = downloads_.find(dbId);
                if (it == downloads_.end()) return S_OK;
                COREWEBVIEW2_DOWNLOAD_STATE st{};
                o->get_State(&st);
                DownloadItem& d = it->second;
                wil::unique_cotaskmem_string finalPath;
                if (SUCCEEDED(o->get_ResultFilePath(finalPath.put())) && finalPath) d.path = win::Narrow(finalPath.get());
                if (st == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED)
                {
                    d.state = "done";
                    INT64 received = 0;
                    o->get_BytesReceived(&received);
                    d.received = received;
                    if (d.total <= 0) d.total = received;
                    storage_.UpdateDownload(dbId, d.received, d.total, "done", d.path);
                    Toast("downloadDone");
                }
                else if (st == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED)
                {
                    d.state = d.cancelRequested ? "canceled" : "failed";
                    storage_.UpdateDownload(dbId, d.received, d.total, d.state, d.path);
                }
                downloadsDirty_ = true;
                return S_OK;
            }).Get(), &token);

    downloadsDirty_ = true;
    PostUi({{"type", "openDownloads"}});
}

// ------------------------------------------------------------- tab callbacks

void BrowserController::OnTabReady(int tabId)
{
    Tab* t = tabs_.Find(tabId);
    if (!t) return;
    if (t->isPrivate && !views_->PrivateSupported())
    {
        t->isPrivate = false;
        Toast("privateUnsupported");
    }
    ApplyZoom(tabId);
    if (tabId == tabs_.ActiveId())
    {
        if (IsInternalUrl(t->url) && ToDisplay(t->url) == "hb://newtab")
        {
            views_->FocusUi();
            uiFocused_ = true;
            PostUi({{"type", "focusOmnibox"}});
        }
        else if (!uiFocused_)
            views_->FocusTab(tabId);
    }
    RequestPush();
}

void BrowserController::OnNavigationStarting(int tabId, const std::string&)
{
    if (Tab* t = tabs_.Find(tabId)) { t->loading = true; t->sleeping = false; }
    RequestPush();
}

void BrowserController::OnNavigationCompleted(int tabId, bool success)
{
    Tab* t = tabs_.Find(tabId);
    if (!t) return;
    t->loading = false;
    ApplyZoom(tabId);
    const std::string scheme = SchemeOf(t->url);
    if (success && !t->isPrivate && (scheme == "http" || scheme == "https"))
    {
        storage_.AddVisit(t->url, t->title, t->url == lastTyped_);
        if (t->url == lastTyped_) lastTyped_.clear();
    }
    sessionDirty_ = true;
    RequestPush();
}

void BrowserController::OnSourceChanged(int tabId, const std::string& url)
{
    if (Tab* t = tabs_.Find(tabId))
    {
        t->url = url;
        sessionDirty_ = true;
        RequestPush();
    }
}

void BrowserController::OnTitleChanged(int tabId, const std::string& title)
{
    Tab* t = tabs_.Find(tabId);
    if (!t) return;
    t->title = title;
    const std::string scheme = SchemeOf(t->url);
    if (!t->isPrivate && (scheme == "http" || scheme == "https")) storage_.UpdateTitle(t->url, title);
    if (tabId == tabs_.ActiveId()) ShowWindowTitle();
    sessionDirty_ = true;
    RequestPush();
}

void BrowserController::OnFaviconChanged(int tabId, const std::string& uri)
{
    if (Tab* t = tabs_.Find(tabId)) { t->favicon = uri; RequestPush(); }
}

void BrowserController::OnHistoryChanged(int tabId, bool canBack, bool canForward)
{
    if (Tab* t = tabs_.Find(tabId)) { t->canGoBack = canBack; t->canGoForward = canForward; RequestPush(); }
}

void BrowserController::OnAudioChanged(int tabId, bool playing)
{
    if (Tab* t = tabs_.Find(tabId)) { t->audible = playing; RequestPush(); }
}

int BrowserController::OnNewWindowRequested(int fromTab, const std::string&, bool userInitiated, bool& isPrivate)
{
    if (!userInitiated) return -1;  // popup blocker
    const Tab* from = tabs_.Find(fromTab);
    isPrivate = from && from->isPrivate;
    const int id = tabs_.Add("", isPrivate, true, fromTab);
    views_->ShowTab(id);
    sessionDirty_ = true;
    PushState();
    return id;
}

void BrowserController::OnCloseRequested(int tabId)
{
    Defer([this, tabId] { CloseTab(tabId); });
}

void BrowserController::OnContentFocused(int)
{
    if (uiFocused_) uiFocused_ = false;
    PostUi({{"type", "dismiss"}});
}

void BrowserController::OnProcessFailed(int tabId, bool renderCrashed)
{
    HB_WARN("Tab {} process failed (render={})", tabId, renderCrashed);
    if (renderCrashed && crashCount_[tabId]++ < 3)
        Defer([this, tabId] { if (views_ && views_->HasTab(tabId)) views_->Reload(tabId, false); });
    else
        Toast("tabCrashed");
}

}  // namespace hb
