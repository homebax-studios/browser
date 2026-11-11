#include "Storage.h"
#include "Util.h"
#include "Navigation.h"

#include <sqlite3.h>

#include <set>

namespace hb {

namespace {

class Stmt
{
public:
    Stmt(sqlite3* db, const char* sql) { if (sqlite3_prepare_v2(db, sql, -1, &s_, nullptr) != SQLITE_OK) s_ = nullptr; }
    ~Stmt() { if (s_) sqlite3_finalize(s_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    Stmt& Text(int i, const std::string& v) { if (s_) sqlite3_bind_text(s_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT); return *this; }
    Stmt& Int(int i, int64_t v) { if (s_) sqlite3_bind_int64(s_, i, v); return *this; }
    bool Row() { return s_ && sqlite3_step(s_) == SQLITE_ROW; }
    bool Done() { return s_ && sqlite3_step(s_) == SQLITE_DONE; }
    std::string Str(int c) const { const auto* p = sqlite3_column_text(s_, c); return p ? reinterpret_cast<const char*>(p) : ""; }
    int64_t Num(int c) const { return sqlite3_column_int64(s_, c); }
    bool Ok() const { return s_ != nullptr; }

private:
    sqlite3_stmt* s_ = nullptr;
};

std::string Like(const std::string& q)
{
    std::string out = "%";
    for (char c : q)
    {
        if (c == '%' || c == '_' || c == '\\') out += '\\';
        out += c;
    }
    return out + "%";
}

}  // namespace

Storage::~Storage() { Close(); }

void Storage::Close()
{
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool Storage::Exec(const char* sql) { return db_ && sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }

bool Storage::Open(const std::filesystem::path& file)
{
    Close();
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const auto u8 = file.u8string();
    const std::string path(reinterpret_cast<const char*>(u8.c_str()), u8.size());
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) { Close(); return false; }
    sqlite3_busy_timeout(db_, 2000);
    Exec("PRAGMA journal_mode=WAL;");
    Exec("PRAGMA synchronous=NORMAL;");
    return Exec(
        "CREATE TABLE IF NOT EXISTS history("
        " id INTEGER PRIMARY KEY, url TEXT NOT NULL UNIQUE, title TEXT NOT NULL DEFAULT '',"
        " visit_count INTEGER NOT NULL DEFAULT 1, last_visit INTEGER NOT NULL, typed INTEGER NOT NULL DEFAULT 0);"
        "CREATE INDEX IF NOT EXISTS idx_history_last ON history(last_visit DESC);"
        "CREATE TABLE IF NOT EXISTS bookmarks("
        " id INTEGER PRIMARY KEY, url TEXT NOT NULL UNIQUE, title TEXT NOT NULL DEFAULT '',"
        " folder TEXT NOT NULL DEFAULT '', position INTEGER NOT NULL DEFAULT 0, created INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS downloads("
        " id INTEGER PRIMARY KEY, url TEXT NOT NULL, path TEXT NOT NULL DEFAULT '', mime TEXT NOT NULL DEFAULT '',"
        " total INTEGER NOT NULL DEFAULT 0, received INTEGER NOT NULL DEFAULT 0,"
        " state TEXT NOT NULL DEFAULT 'progress', started INTEGER NOT NULL);");
}

// ---------------------------------------------------------------- history

void Storage::AddVisit(const std::string& url, const std::string& title, bool typed)
{
    if (!db_ || url.empty()) return;
    Stmt s(db_,
           "INSERT INTO history(url,title,visit_count,last_visit,typed) VALUES(?1,?2,1,?3,?4)"
           " ON CONFLICT(url) DO UPDATE SET visit_count=visit_count+1, last_visit=excluded.last_visit,"
           " typed=typed+excluded.typed, title=CASE WHEN excluded.title<>'' THEN excluded.title ELSE title END");
    s.Text(1, url).Text(2, title).Int(3, NowMs()).Int(4, typed ? 1 : 0).Done();
}

void Storage::UpdateTitle(const std::string& url, const std::string& title)
{
    if (!db_ || url.empty() || title.empty()) return;
    Stmt s(db_, "UPDATE history SET title=?1 WHERE url=?2");
    s.Text(1, title).Text(2, url).Done();
}

json Storage::History(const std::string& query, int limit, int offset)
{
    json out = json::array();
    if (!db_) return out;
    Stmt s(db_,
           "SELECT id,url,title,visit_count,last_visit FROM history"
           " WHERE (?1='' OR url LIKE ?2 ESCAPE '\\' OR title LIKE ?2 ESCAPE '\\')"
           " ORDER BY last_visit DESC, id DESC LIMIT ?3 OFFSET ?4");
    s.Text(1, query).Text(2, Like(query)).Int(3, limit).Int(4, offset);
    while (s.Row())
        out.push_back({{"id", s.Num(0)}, {"url", s.Str(1)}, {"title", s.Str(2)}, {"visits", s.Num(3)}, {"time", s.Num(4)}});
    return out;
}

void Storage::RemoveHistory(int64_t id)
{
    if (!db_) return;
    Stmt s(db_, "DELETE FROM history WHERE id=?1");
    s.Int(1, id).Done();
}

void Storage::ClearHistory(int64_t sinceMs)
{
    if (!db_) return;
    Stmt s(db_, "DELETE FROM history WHERE last_visit>=?1");
    s.Int(1, sinceMs).Done();
}

json Storage::TopSites(int limit)
{
    json out = json::array();
    if (!db_) return out;
    Stmt s(db_,
           "SELECT url,title FROM history WHERE url LIKE 'http%' AND url NOT LIKE '%hb.local%'"
           " ORDER BY visit_count DESC, last_visit DESC LIMIT ?1");
    s.Int(1, limit);
    while (s.Row()) out.push_back({{"url", s.Str(0)}, {"title", s.Str(1)}});
    return out;
}

json Storage::Suggest(const std::string& query, int limit)
{
    json out = json::array();
    if (!db_ || query.empty()) return out;
    std::set<std::string> seen;
    const std::string like = Like(query);
    {
        Stmt s(db_, "SELECT url,title FROM bookmarks WHERE url LIKE ?1 ESCAPE '\\' OR title LIKE ?1 ESCAPE '\\' LIMIT ?2");
        s.Text(1, like).Int(2, limit);
        while (s.Row())
            if (seen.insert(s.Str(0)).second) out.push_back({{"type", "bookmark"}, {"url", s.Str(0)}, {"title", s.Str(1)}});
    }
    {
        Stmt s(db_,
               "SELECT url,title FROM history WHERE (url LIKE ?1 ESCAPE '\\' OR title LIKE ?1 ESCAPE '\\')"
               " AND url NOT LIKE '%hb.local%' ORDER BY typed DESC, visit_count DESC, last_visit DESC LIMIT ?2");
        s.Text(1, like).Int(2, limit);
        while (s.Row())
            if (seen.insert(s.Str(0)).second) out.push_back({{"type", "history"}, {"url", s.Str(0)}, {"title", s.Str(1)}});
    }
    while (static_cast<int>(out.size()) > limit) out.erase(out.size() - 1);
    return out;
}

// -------------------------------------------------------------- bookmarks

json Storage::Bookmarks()
{
    json out = json::array();
    if (!db_) return out;
    Stmt s(db_, "SELECT id,url,title,folder FROM bookmarks ORDER BY folder, position, id");
    while (s.Row()) out.push_back({{"id", s.Num(0)}, {"url", s.Str(1)}, {"title", s.Str(2)}, {"folder", s.Str(3)}});
    return out;
}

bool Storage::IsBookmarked(const std::string& url)
{
    if (!db_) return false;
    Stmt s(db_, "SELECT 1 FROM bookmarks WHERE url=?1");
    s.Text(1, url);
    return s.Row();
}

int64_t Storage::AddBookmark(const std::string& url, const std::string& title, const std::string& folder)
{
    if (!db_ || url.empty() || !IsSafeTabUrl(url)) return 0;
    {
        Stmt s(db_,
               "INSERT OR IGNORE INTO bookmarks(url,title,folder,position,created)"
               " VALUES(?1,?2,?3,(SELECT COALESCE(MAX(position),0)+1 FROM bookmarks),?4)");
        s.Text(1, url).Text(2, title.empty() ? url : title).Text(3, folder).Int(4, NowMs()).Done();
    }
    Stmt s(db_, "SELECT id FROM bookmarks WHERE url=?1");
    s.Text(1, url);
    return s.Row() ? s.Num(0) : 0;
}

bool Storage::RemoveBookmarkByUrl(const std::string& url)
{
    if (!db_) return false;
    Stmt s(db_, "DELETE FROM bookmarks WHERE url=?1");
    s.Text(1, url);
    return s.Done() && sqlite3_changes(db_) > 0;
}

bool Storage::RemoveBookmark(int64_t id)
{
    if (!db_) return false;
    Stmt s(db_, "DELETE FROM bookmarks WHERE id=?1");
    s.Int(1, id);
    return s.Done() && sqlite3_changes(db_) > 0;
}

bool Storage::UpdateBookmark(int64_t id, const std::string& title, const std::string& url, const std::string& folder)
{
    if (!db_ || url.empty() || !IsSafeTabUrl(url)) return false;
    Stmt s(db_, "UPDATE OR IGNORE bookmarks SET title=?1,url=?2,folder=?3 WHERE id=?4");
    s.Text(1, title).Text(2, url).Text(3, folder).Int(4, id);
    return s.Done() && sqlite3_changes(db_) > 0;
}

int Storage::ImportBookmarks(const json& items)
{
    if (!db_ || !items.is_array()) return 0;
    int added = 0;
    Exec("BEGIN;");
    for (const auto& it : items)
    {
        if (!it.is_object() || !it.contains("url") || !it["url"].is_string()) continue;
        const std::string url = it["url"].get<std::string>();
        const std::string scheme = SchemeOf(url);
        if (scheme != "http" && scheme != "https") continue;
        const std::string title = it.value("title", "");
        const std::string folder = it.value("folder", "");
        const bool existed = IsBookmarked(url);
        AddBookmark(url, title, folder);
        if (!existed) ++added;
    }
    Exec("COMMIT;");
    return added;
}

// -------------------------------------------------------------- downloads

int64_t Storage::AddDownload(const std::string& url, const std::string& path, const std::string& mime, int64_t total)
{
    if (!db_) return 0;
    Stmt s(db_, "INSERT INTO downloads(url,path,mime,total,received,state,started) VALUES(?1,?2,?3,?4,0,'progress',?5)");
    s.Text(1, url).Text(2, path).Text(3, mime).Int(4, total).Int(5, NowMs());
    return s.Done() ? sqlite3_last_insert_rowid(db_) : 0;
}

void Storage::UpdateDownload(int64_t id, int64_t received, int64_t total, const std::string& state, const std::string& path)
{
    if (!db_) return;
    Stmt s(db_, "UPDATE downloads SET received=?1,total=CASE WHEN ?2>0 THEN ?2 ELSE total END,state=?3,path=CASE WHEN ?4<>'' THEN ?4 ELSE path END WHERE id=?5");
    s.Int(1, received).Int(2, total).Text(3, state).Text(4, path).Int(5, id).Done();
}

json Storage::Downloads()
{
    json out = json::array();
    if (!db_) return out;
    Stmt s(db_, "SELECT id,url,path,mime,total,received,state,started FROM downloads ORDER BY started DESC LIMIT 300");
    while (s.Row())
        out.push_back({{"id", s.Num(0)}, {"url", s.Str(1)}, {"path", s.Str(2)}, {"mime", s.Str(3)},
                       {"total", s.Num(4)}, {"received", s.Num(5)}, {"state", s.Str(6)}, {"started", s.Num(7)}});
    return out;
}

void Storage::RemoveDownload(int64_t id)
{
    if (!db_) return;
    Stmt s(db_, "DELETE FROM downloads WHERE id=?1");
    s.Int(1, id).Done();
}

void Storage::ClearDownloads()
{
    Exec("DELETE FROM downloads WHERE state<>'progress';");
}

void Storage::FailPendingDownloads()
{
    Exec("UPDATE downloads SET state='failed' WHERE state='progress' OR state='paused';");
}

}  // namespace hb
