#include "Settings.h"
#include "Util.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace hb {

json Settings::Defaults()
{
    return json{
        {"theme", "system"},            // system | light | dark
        {"accent", "#4f7cff"},
        {"language", "auto"},           // auto | cs | en
        {"searchEngine", "google"},
        {"homepage", "hb://newtab"},
        {"startup", "restore"},         // newtab | home | restore
        {"showHomeButton", true},
        {"showBookmarksBar", true},
        {"singleInstance", true},
        {"downloadDir", ""},
        {"askDownloadLocation", false},
        {"memorySaver", true},
        {"sleepAfterMinutes", 15},
        {"defaultZoom", 100},
        {"trackingPrevention", "balanced"},  // off | basic | balanced | strict
        {"passwordAutosave", false},
        {"generalAutofill", false},
        {"devTools", true},
        {"customSearchEngines", json::array()},  // [{id:"custom-x", name, url with one %s}]
        {"clearOnExit", false},
        {"checkUpdates", true},               // look for a new version at every start and open its page
        {"liteMode", "auto"},                 // auto | on | off  (fewer processes, less memory; applied at next start)
        {"blockInsecureDownloads", true},     // refuse programs/scripts downloaded over plain http
        {"sitePermissions", json::object()},  // {origin: {permission: "allow"|"deny"}}
        {"pinned", json::array({
            {{"title", "Google"}, {"url", "https://www.google.com"}},
            {{"title", "YouTube"}, {"url", "https://www.youtube.com"}},
            {{"title", "Wikipedia"}, {"url", "https://www.wikipedia.org"}},
            {{"title", "GitHub"}, {"url", "https://github.com"}},
            {{"title", "Seznam"}, {"url", "https://www.seznam.cz"}},
        })},
        {"zoomSites", json::object()},
        {"window", json{{"x", 0}, {"y", 0}, {"w", 0}, {"h", 0}, {"maximized", false}}},
    };
}

static bool SameKind(const json& a, const json& b)
{
    if (a.is_number() && b.is_number()) return true;
    return a.type() == b.type();
}

namespace {

bool OneOf(const json& v, std::initializer_list<const char*> allowed)
{
    if (!v.is_string()) return false;
    const auto& s = v.get_ref<const std::string&>();
    for (const char* a : allowed) if (s == a) return true;
    return false;
}

bool IsIdent(const std::string& s, size_t maxLen)
{
    if (s.empty() || s.size() > maxLen) return false;
    for (unsigned char c : s)
        if (!(std::isdigit(c) || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) return false;
    return true;
}

bool PlainText(const json& v, size_t maxLen, bool allowEmpty)
{
    if (!v.is_string()) return false;
    const auto& s = v.get_ref<const std::string&>();
    if ((s.empty() && !allowEmpty) || s.size() > maxLen) return false;
    for (unsigned char c : s)
        if (c < 0x20 || c == 0x7f) return false;
    return true;
}

bool IntIn(const json& v, long long lo, long long hi)
{
    if (!v.is_number_integer()) return v.is_number() && v.get<double>() >= lo && v.get<double>() <= hi;
    const long long n = v.get<long long>();
    return n >= lo && n <= hi;
}

}  // namespace

bool Settings::IsValid(const std::string& key, const json& v)
{
    if (key == "theme") return OneOf(v, {"system", "light", "dark"});
    if (key == "language") return OneOf(v, {"auto", "cs", "en"});
    if (key == "startup") return OneOf(v, {"newtab", "home", "restore"});
    if (key == "trackingPrevention") return OneOf(v, {"off", "basic", "balanced", "strict"});
    if (key == "accent")
    {
        if (!v.is_string()) return false;
        const auto& s = v.get_ref<const std::string&>();
        if (s.size() != 7 || s[0] != '#') return false;
        for (size_t i = 1; i < 7; ++i) if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }
    if (key == "searchEngine") return v.is_string() && IsIdent(v.get<std::string>(), 64);
    if (key == "homepage") return PlainText(v, 2048, true);
    if (key == "downloadDir") return PlainText(v, 1024, true);
    if (key == "sleepAfterMinutes") return IntIn(v, 1, 1440);
    if (key == "defaultZoom") return IntIn(v, 25, 500);
    if (key == "pinned")
    {
        if (!v.is_array() || v.size() > 64) return false;
        for (const auto& p : v)
        {
            if (!p.is_object() || !p.contains("url") || !p["url"].is_string() || !p.contains("title") || !PlainText(p["title"], 200, true)) return false;
            const std::string url = p["url"].get<std::string>();
            const std::string scheme = SchemeOf(url);
            if ((scheme != "http" && scheme != "https") || url.size() > 2048 || !IsSafeTabUrl(url)) return false;
        }
        return true;
    }
    if (key == "zoomSites")
    {
        if (!v.is_object() || v.size() > 2000) return false;
        for (auto it = v.begin(); it != v.end(); ++it)
            if (!it.value().is_number() || it.key().size() > 255) return false;
        return true;
    }
    if (key == "liteMode") return OneOf(v, {"auto", "on", "off"});
    if (key == "sitePermissions")
    {
        if (!v.is_object() || v.size() > 500) return false;
        for (auto o = v.begin(); o != v.end(); ++o)
        {
            if (o.key().size() > 300 || !o.value().is_object() || o.value().size() > 16) return false;
            for (auto k = o.value().begin(); k != o.value().end(); ++k)
                if (k.key().size() > 40 || !OneOf(k.value(), {"allow", "deny"})) return false;
        }
        return true;
    }
    if (key == "customSearchEngines")
    {
        if (!v.is_array() || v.size() > 20) return false;
        std::vector<std::string> seen;
        for (const auto& e : v)
        {
            if (!e.is_object() || !e.contains("id") || !e.contains("name") || !e.contains("url")) return false;
            if (!e["id"].is_string() || !e["url"].is_string() || !PlainText(e["name"], 60, false)) return false;
            const std::string id = e["id"].get<std::string>();
            if (!IsIdent(id, 40) || id.rfind("custom-", 0) != 0 || !IsValidSearchTemplate(e["url"].get<std::string>())) return false;
            if (std::find(seen.begin(), seen.end(), id) != seen.end()) return false;
            seen.push_back(id);
        }
        return true;
    }
    return true;  // booleans and the like: the type check is enough
}

std::vector<SearchEngine> Settings::CustomEngines(const json& list)
{
    std::vector<SearchEngine> out;
    if (!IsValid("customSearchEngines", list)) return out;
    for (const auto& e : list) out.push_back({e["id"].get<std::string>(), e["name"].get<std::string>(), e["url"].get<std::string>()});
    return out;
}

void Settings::Load()
{
    data_ = Defaults();
    std::ifstream in(file_, std::ios::binary);
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    json parsed = json::parse(ss.str(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return;
    for (auto it = parsed.begin(); it != parsed.end(); ++it)
        if (data_.contains(it.key()) && SameKind(data_[it.key()], it.value()) && IsValid(it.key(), it.value()))
            data_[it.key()] = it.value();
}

bool Settings::Save() const
{
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    const auto tmp = std::filesystem::path(file_).concat(".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << data_.dump(2);
        if (!out.good()) return false;
    }
    std::filesystem::rename(tmp, file_, ec);
    if (ec)
    {
        std::filesystem::remove(file_, ec);
        std::filesystem::rename(tmp, file_, ec);
    }
    return !ec;
}

bool Settings::Set(const std::string& key, const json& value)
{
    const json defaults = Defaults();
    if (!defaults.contains(key) || !SameKind(defaults[key], value) || !IsValid(key, value)) return false;
    if (data_.contains(key) && data_[key] == value) return true;
    data_[key] = value;
    Save();
    if (onChanged) onChanged(key);
    return true;
}

}  // namespace hb
