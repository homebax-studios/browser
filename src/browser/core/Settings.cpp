#include "Settings.h"

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
        {"clearOnExit", false},
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
        if (data_.contains(it.key()) && SameKind(data_[it.key()], it.value()))
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
    if (!defaults.contains(key) || !SameKind(defaults[key], value)) return false;
    if (data_.contains(key) && data_[key] == value) return true;
    data_[key] = value;
    Save();
    if (onChanged) onChanged(key);
    return true;
}

}  // namespace hb
