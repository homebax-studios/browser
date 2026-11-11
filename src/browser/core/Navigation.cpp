#include "Navigation.h"
#include "Util.h"

#include <algorithm>
#include <cctype>

namespace hb {

namespace {
std::vector<SearchEngine>& CustomEngines() { static std::vector<SearchEngine> v; return v; }
std::vector<SearchEngine>& MergedEngines() { static std::vector<SearchEngine> v; return v; }

const std::vector<SearchEngine>& BuiltinEngines()
{
    static const std::vector<SearchEngine> engines = {
        {"google", "Google", "https://www.google.com/search?q=%s"},
        {"bing", "Bing", "https://www.bing.com/search?q=%s"},
        {"duckduckgo", "DuckDuckGo", "https://duckduckgo.com/?q=%s"},
        {"seznam", "Seznam", "https://search.seznam.cz/?q=%s"},
        {"brave", "Brave Search", "https://search.brave.com/search?q=%s"},
        {"startpage", "Startpage", "https://www.startpage.com/do/search?q=%s"},
        {"ecosia", "Ecosia", "https://www.ecosia.org/search?q=%s"},
        {"hb-search", "HB Search", "https://homebax.vercel.app/search?=%s"},
    };
    return engines;
}

void RebuildMerged()
{
    auto& m = MergedEngines();
    m = BuiltinEngines();
    m.insert(m.end(), CustomEngines().begin(), CustomEngines().end());
}
}  // namespace

const std::vector<SearchEngine>& SearchEngines()
{
    if (MergedEngines().empty()) RebuildMerged();
    return MergedEngines();
}

void SetCustomSearchEngines(std::vector<SearchEngine> engines)
{
    // Custom engines can never shadow a built-in id and must carry a valid template.
    std::vector<SearchEngine> ok;
    for (auto& e : engines)
        if (StartsWith(e.id, "custom-") && !e.name.empty() && IsValidSearchTemplate(e.url)) ok.push_back(std::move(e));
    CustomEngines() = std::move(ok);
    RebuildMerged();
}

bool IsValidSearchTemplate(const std::string& url)
{
    if (url.empty() || url.size() > 2048) return false;
    const std::string lower = ToLower(url);
    if (!StartsWith(lower, "https://") && !StartsWith(lower, "http://")) return false;
    for (unsigned char c : url)
        if (c <= 0x20 || c == 0x7f) return false;
    const auto first = url.find("%s");
    if (first == std::string::npos || url.find("%s", first + 2) != std::string::npos) return false;
    return !HostOf(url).empty();
}

bool IsSafeTabUrl(const std::string& url)
{
    const std::string scheme = SchemeOf(url);
    if (scheme == "http" || scheme == "https") return !HostOf(url).empty();
    if (scheme == "file" || scheme == "ftp" || scheme == "view-source") return true;
    return ToLower(url) == "about:blank";
}

std::string SearchUrl(const std::string& engineId, const std::string& query)
{
    const auto& list = SearchEngines();
    auto it = std::find_if(list.begin(), list.end(), [&](const SearchEngine& e) { return e.id == engineId; });
    const SearchEngine& engine = it == list.end() ? list.front() : *it;
    return ReplaceAll(engine.url, "%s", UrlEncode(query));
}

namespace {

const std::string kInternalPrefix = std::string("https://") + kInternalHost + "/pages/";

bool IsIPv4(const std::string& h)
{
    int parts = 0;
    size_t i = 0;
    while (i < h.size())
    {
        size_t j = i;
        while (j < h.size() && std::isdigit(static_cast<unsigned char>(h[j]))) ++j;
        if (j == i || j - i > 3) return false;
        ++parts;
        if (j == h.size()) break;
        if (h[j] != '.') return false;
        i = j + 1;
        if (i == h.size()) return false;
    }
    return parts == 4;
}

bool IsHostChar(unsigned char c) { return std::isalnum(c) || c == '-' || c == '_' || c >= 0x80; }

// Splits "user@host:port/path" into host / hasPort.
bool SplitAuthority(const std::string& text, std::string& host, bool& hasPort)
{
    size_t e = text.find_first_of("/?#");
    std::string auth = text.substr(0, e);
    const auto at = auth.rfind('@');
    if (at != std::string::npos) auth.erase(0, at + 1);
    hasPort = false;
    if (!auth.empty() && auth[0] == '[')
    {
        const auto close = auth.find(']');
        if (close == std::string::npos) return false;
        host = auth.substr(0, close + 1);
        return true;
    }
    const auto colon = auth.rfind(':');
    if (colon != std::string::npos)
    {
        const std::string port = auth.substr(colon + 1);
        if (port.empty() || !std::all_of(port.begin(), port.end(), [](unsigned char c) { return std::isdigit(c); }))
            return false;
        hasPort = true;
        auth.erase(colon);
    }
    host = ToLower(auth);
    return !host.empty();
}

bool IsLocalHost(const std::string& host)
{
    return host == "localhost" || (!host.empty() && host[0] == '[') || IsIPv4(host) || EndsWith(host, ".local") ||
           EndsWith(host, ".lan") || EndsWith(host, ".localdomain") || EndsWith(host, ".localhost");
}

}  // namespace

bool LooksLikeHost(const std::string& text)
{
    if (text.empty() || text.find_first_of(" \t\r\n") != std::string::npos) return false;
    std::string host;
    bool hasPort = false;
    if (!SplitAuthority(text, host, hasPort)) return false;
    if (host[0] == '[') return true;  // IPv6
    if (host == "localhost" || IsIPv4(host)) return true;
    for (unsigned char c : host)
        if (!IsHostChar(c) && c != '.') return false;
    if (host.front() == '.' || host.back() == '.') return false;
    const auto dot = host.rfind('.');
    if (dot == std::string::npos) return hasPort; 
    const std::string tld = host.substr(dot + 1);
    if (tld.size() < 2) return false;
    return !std::all_of(tld.begin(), tld.end(), [](unsigned char c) { return std::isdigit(c); });
}

std::string ToInternal(const std::string& url)
{
    if (!StartsWith(ToLower(url.substr(0, 5)), "hb://")) return url;
    std::string rest = url.substr(5);
    const auto cut = rest.find_first_of("/?#");
    std::string name = rest.substr(0, cut);
    std::string tail = cut == std::string::npos ? "" : rest.substr(cut);
    if (!tail.empty() && tail[0] == '/') tail.erase(0, 1);
    name = ToLower(name);
    if (name.empty()) name = "newtab";
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '-') return "https://" + std::string(kInternalHost) + "/pages/newtab.html";
    return kInternalPrefix + name + ".html" + tail;
}

std::string ToDisplay(const std::string& url)
{
    if (!StartsWith(url, kInternalPrefix)) return url;
    std::string rest = url.substr(kInternalPrefix.size());
    const auto cut = rest.find_first_of("?#");
    std::string name = rest.substr(0, cut);
    std::string tail = cut == std::string::npos ? "" : rest.substr(cut);
    if (EndsWith(name, ".html")) name.erase(name.size() - 5);
    return "hb://" + name + tail;
}

bool IsInternalUrl(const std::string& url) { return StartsWith(url, kInternalPrefix); }

std::string ResolveInput(const std::string& input, const std::string& engineId)
{
    const std::string in = Trim(input);
    if (in.empty()) return ToInternal("hb://newtab");

    const std::string lower = ToLower(in);
    if (StartsWith(lower, "hb://")) return ToInternal(in);

    for (const char* scheme : {"http://", "https://", "file://", "ftp://", "view-source:", "about:", "data:", "blob:"})
        if (StartsWith(lower, scheme)) return in;

    if (LooksLikeHost(in))
    {
        std::string host;
        bool hasPort = false;
        SplitAuthority(in, host, hasPort);
        return std::string(IsLocalHost(host) ? "http://" : "https://") + in;
    }
    return SearchUrl(engineId, in);
}

}  // namespace hb
