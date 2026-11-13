#include "core/Navigation.h"
#include "core/Settings.h"
#include "core/Security.h"
#include "core/Storage.h"
#include "core/TabManager.h"
#include "core/Util.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
using namespace hb;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
#define EQ(a, b) do { std::string _a=(a), _b=(b); if (_a != _b) { std::printf("FAIL line %d: '%s' != '%s'\n", __LINE__, _a.c_str(), _b.c_str()); ++fails; } } while (0)
int main() {
  EQ(ResolveInput("example.com", "google"), "https://example.com");
  EQ(ResolveInput("  localhost:3000/x ", "google"), "http://localhost:3000/x");
  EQ(ResolveInput("192.168.0.1", "google"), "http://192.168.0.1");
  EQ(ResolveInput("https://a.cz/x y", "google"), "https://a.cz/x y");
  EQ(ResolveInput("kočka pes", "google"), "https://www.google.com/search?q=ko%C4%8Dka+pes");
  EQ(ResolveInput("kočka.cz", "google"), "https://kočka.cz");
  EQ(ResolveInput("hello", "duckduckgo"), "https://duckduckgo.com/?q=hello");
  EQ(ResolveInput("3.14", "google"), "https://www.google.com/search?q=3.14");
  EQ(ResolveInput("hb://settings#privacy", "google"), "https://hb.local/pages/settings.html#privacy");
  EQ(ResolveInput("", "google"), "https://hb.local/pages/newtab.html");
  EQ(ResolveInput("javascript:alert(1)", "google"), "https://www.google.com/search?q=javascript%3Aalert%281%29");
  EQ(ToDisplay("https://hb.local/pages/history.html?q=a"), "hb://history?q=a");
  EQ(ResolveInput("a.b/c?d=1 2", "bing"), "https://www.bing.com/search?q=a.b%2Fc%3Fd%3D1+2");
  EQ(HostOf("https://User:pw@WWW.Example.com:8080/a?b#c"), "www.example.com");
  EQ(SchemeOf("HTTPS://x"), "https");
  CHECK(CompareVersions("2.1.10","2.1.2") > 0); CHECK(CompareVersions("2.1.2","2.1.2")==0); CHECK(CompareVersions("2.0","2.0.1") < 0); CHECK(CompareVersions("v3","2.9.9")>0);

  // security / validation
  EQ(ResolveInput("edge://settings", "google"), "https://www.google.com/search?q=edge%3A%2F%2Fsettings");
  CHECK(IsSafeTabUrl("https://a.cz")); CHECK(!IsSafeTabUrl("javascript:alert(1)")); CHECK(!IsSafeTabUrl("data:text/html,x")); CHECK(IsSafeTabUrl("about:blank")); CHECK(!IsSafeTabUrl("about:config"));
  CHECK(IsValidSearchTemplate("https://x.cz/?q=%s")); CHECK(!IsValidSearchTemplate("https://x.cz/")); CHECK(!IsValidSearchTemplate("https://x.cz/%s%s")); CHECK(!IsValidSearchTemplate("javascript:%s")); CHECK(!IsValidSearchTemplate("https://x.cz/ ?%s"));
  SetCustomSearchEngines({{"custom-a", "A", "https://a.cz/?q=%s"}, {"google", "Evil", "https://evil/%s"}, {"custom-b", "B", "https://b.cz/"}});
  CHECK(SearchEngines().size() == 9); EQ(ResolveInput("pes", "custom-a"), "https://a.cz/?q=pes"); EQ(ResolveInput("pes", "gone"), "https://www.google.com/search?q=pes");
  SetCustomSearchEngines({});
  CHECK(IsAllowedNavigation("https://a.cz", false)); CHECK(!IsAllowedNavigation("ms-msdt:/id", true)); CHECK(!IsAllowedNavigation("vbscript:x", true)); CHECK(!IsAllowedNavigation("data:text/html,x", false)); CHECK(IsAllowedNavigation("data:text/html,x", true)); CHECK(IsAllowedNavigation("mailto:a@b.cz", false));
  CHECK(Settings::IsValid("liteMode", "on")); CHECK(!Settings::IsValid("liteMode", "x")); CHECK(Settings::IsValid("sitePermissions", json::parse(R"({"https://a.cz":{"geolocation":"deny"}})"))); CHECK(!Settings::IsValid("sitePermissions", json::parse(R"({"https://a.cz":{"geolocation":"maybe"}})")));
  CHECK(IsDangerousFileName("Setup.EXE")); CHECK(IsDangerousFileName("a.exe. ")); CHECK(IsDangerousFileName("x.ps1")); CHECK(!IsDangerousFileName("photo.png")); CHECK(!IsDangerousFileName("noext"));
  EQ(SanitizeFileName("..\\a/b:c.txt"), ".._a_b_c.txt"); EQ(SanitizeFileName("CON.txt"), "_CON.txt"); EQ(SanitizeFileName("  "), "download");
  CHECK(!Settings::IsValid("theme", "neon")); CHECK(!Settings::IsValid("accent", "red;}x{")); CHECK(Settings::IsValid("accent", "#4F7cff")); CHECK(!Settings::IsValid("sleepAfterMinutes", 0)); CHECK(!Settings::IsValid("defaultZoom", 9000));
  CHECK(!Settings::IsValid("pinned", json::parse(R"([{"title":"x","url":"javascript:1"}])"))); CHECK(Settings::IsValid("pinned", json::parse(R"([{"title":"x","url":"https://a.cz"}])")));
  CHECK(Settings::IsValid("customSearchEngines", json::parse(R"([{"id":"custom-a","name":"A","url":"https://a.cz/?q=%s"}])"))); CHECK(!Settings::IsValid("customSearchEngines", json::parse(R"([{"id":"google","name":"A","url":"https://a.cz/?q=%s"}])")));
  CHECK(Settings::CustomEngines(json::parse(R"([{"id":"custom-a","name":"A","url":"https://a.cz/?q=%s"}])")).size() == 1);

  auto tmp = std::filesystem::temp_directory_path() / "hbtest"; std::filesystem::remove_all(tmp);
  { Settings s(tmp / "settings.json"); s.Load(); CHECK(s.Get<bool>("showBookmarksBar")); CHECK(!s.Set("nope", 1)); CHECK(!s.Set("theme", 5)); CHECK(s.Set("theme", "dark")); CHECK(s.Set("sleepAfterMinutes", 5));
    Settings t(tmp / "settings.json"); t.Load(); EQ(t.Get<std::string>("theme"), "dark"); CHECK(t.Get<int>("sleepAfterMinutes") == 5); CHECK(t.Get<json>("pinned").size() == 5); }
  { Storage db; CHECK(db.Open(tmp / "b.db"));
    db.AddVisit("https://seznam.cz/", "Seznam"); db.AddVisit("https://seznam.cz/", ""); db.AddVisit("https://example.org/a_b", "Example", true);
    CHECK(db.History("", 10, 0).size() == 2); EQ(db.History("", 10, 0)[1]["title"].get<std::string>(), "Seznam"); CHECK(db.History("100%", 10, 0).empty()); CHECK(db.History("a_b", 10, 0).size() == 1);
    CHECK(db.TopSites(5)[0]["url"] == "https://seznam.cz/");
    CHECK(db.AddBookmark("javascript:alert(1)", "x") == 0); CHECK(db.AddBookmark("https://seznam.cz/", "Sez") > 0); CHECK(db.IsBookmarked("https://seznam.cz/")); CHECK(db.Suggest("sez", 5).size() == 1); CHECK(db.Suggest("sez", 5)[0]["type"] == "bookmark");
    CHECK(db.RemoveBookmarkByUrl("https://seznam.cz/")); CHECK(!db.IsBookmarked("https://seznam.cz/"));
    CHECK(db.ImportBookmarks(json::parse(R"([{"url":"https://a.cz","title":"A","folder":"F"},{"url":"javascript:1"},{"url":"https://a.cz"}])")) == 1);
    int64_t d = db.AddDownload("https://x/y.zip", "C:/y.zip", "application/zip", 100); db.UpdateDownload(d, 100, 100, "done"); EQ(db.Downloads()[0]["state"].get<std::string>(), "done");
    db.ClearHistory(); CHECK(db.History("", 10, 0).empty()); }
  { TabManager m; int a = m.Add("a", false, true), b = m.Add("b", false, true), c = m.Add("c", false, false);
    CHECK(m.ActiveId() == b); CHECK(m.Close(b) == c); CHECK(m.ActiveId() == c); CHECK(m.Close(c) == a);
    ClosedTab ct; CHECK(m.PopClosed(ct)); EQ(ct.url, "c"); CHECK(m.PopClosed(ct)); EQ(ct.url, "b");
    int d = m.Add("d", false, false); m.SetPinned(d, true); CHECK(m.AtIndex(0) == d); m.Move(a, 0); CHECK(m.AtIndex(0) == d); CHECK(m.Neighbor(1) == d);
    CHECK(m.Close(a) == d); CHECK(m.Close(d) == -1); CHECK(m.Count() == 0); }
  std::filesystem::remove_all(tmp);
  std::printf(fails ? "%d FAILURES\n" : "ALL OK\n", fails);
  return fails;
}
