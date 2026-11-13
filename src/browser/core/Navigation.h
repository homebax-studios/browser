#pragma once
#include <string>
#include <vector>

namespace hb {

struct SearchEngine
{
    std::string id;
    std::string name;
    std::string url;  // %s = encoded query
};

// Built-in engines followed by the user's custom engines (ids start with "custom-").
const std::vector<SearchEngine>& SearchEngines();
void SetCustomSearchEngines(std::vector<SearchEngine> engines);
// http(s) URL that contains %s exactly once, no spaces/control characters, at most 2048 bytes.
bool IsValidSearchTemplate(const std::string& url);

// Schemes a tab may be restored to / a bookmark may point at: http, https, file, ftp, view-source, about:blank.
bool IsSafeTabUrl(const std::string& url);

// Top-level navigations a tab may perform. Everything else (ms-*, vbscript, unknown schemes ...) is cancelled;
// data: only when the user triggered it.
bool IsAllowedNavigation(const std::string& url, bool userInitiated);
std::string SearchUrl(const std::string& engineId, const std::string& query);

// Turns whatever the user typed into the address bar into a loadable URL.
// Handles hb:// pages, full URLs, bare hosts (example.com, localhost:3000, 192.168.0.1) and searches.
std::string ResolveInput(const std::string& input, const std::string& engineId);

// hb://settings  <->  https://hb.local/pages/settings.html
std::string ToInternal(const std::string& url);
std::string ToDisplay(const std::string& url);
bool IsInternalUrl(const std::string& url);   // https://hb.local/pages/...
bool LooksLikeHost(const std::string& text);  // example.com, localhost:8080, 10.0.0.1 ...

inline const char* kInternalHost = "hb.local";

}  // namespace hb
