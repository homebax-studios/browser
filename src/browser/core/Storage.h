#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

struct sqlite3;

namespace hb {

using json = nlohmann::json;

// History, bookmarks and downloads in one SQLite file. All results are JSON so they can go straight to the UI.
class Storage
{
public:
    Storage() = default;
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool Open(const std::filesystem::path& file);
    void Close();

    // History
    void AddVisit(const std::string& url, const std::string& title, bool typed = false);
    void UpdateTitle(const std::string& url, const std::string& title);
    json History(const std::string& query, int limit, int offset);
    void RemoveHistory(int64_t id);
    void ClearHistory(int64_t sinceMs = 0);
    json TopSites(int limit);
    json Suggest(const std::string& query, int limit);  // [{type,url,title}]

    // Bookmarks
    json Bookmarks();
    bool IsBookmarked(const std::string& url);
    int64_t AddBookmark(const std::string& url, const std::string& title, const std::string& folder = "");
    bool RemoveBookmarkByUrl(const std::string& url);
    bool RemoveBookmark(int64_t id);
    bool UpdateBookmark(int64_t id, const std::string& title, const std::string& url, const std::string& folder);
    int ImportBookmarks(const json& items);  // [{url,title,folder}] -> number added

    // Downloads
    int64_t AddDownload(const std::string& url, const std::string& path, const std::string& mime, int64_t total);
    void UpdateDownload(int64_t id, int64_t received, int64_t total, const std::string& state, const std::string& path = "");
    json Downloads();
    void RemoveDownload(int64_t id);
    void ClearDownloads();
    void FailPendingDownloads();  // downloads that were running when the app closed

private:
    bool Exec(const char* sql);
    sqlite3* db_ = nullptr;
};

}  // namespace hb
