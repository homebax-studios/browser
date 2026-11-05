#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace hb {

struct Tab
{
    int id = 0;
    std::string title;
    std::string url;       // real URL (https://hb.local/pages/... for internal pages)
    std::string favicon;
    bool loading = false;
    bool pinned = false;
    bool muted = false;
    bool audible = false;
    bool sleeping = false;
    bool isPrivate = false;
    bool canGoBack = false;
    bool canGoForward = false;
    int64_t lastActive = 0;
};

struct ClosedTab
{
    std::string url;
    std::string title;
    bool pinned = false;
};

// Pure data model of the tab strip. The Windows layer owns the actual WebViews.
class TabManager
{
public:
    // Creates a tab (not navigated yet). afterId = insert position hint, -1 = at the end.
    int Add(const std::string& url, bool isPrivate, bool activate, int afterId = -1);
    // Returns the id of the tab that became active, or -1 when no tab is left.
    int Close(int id);
    bool Activate(int id);
    bool Move(int id, int newIndex);
    bool SetPinned(int id, bool pinned);

    Tab* Find(int id);
    const Tab* Find(int id) const;
    Tab* Active() { return Find(activeId_); }
    const Tab* Active() const { return Find(activeId_); }
    int ActiveId() const noexcept { return activeId_; }
    int IndexOf(int id) const;
    const std::vector<Tab>& All() const noexcept { return tabs_; }
    std::vector<Tab>& All_Mutable() noexcept { return tabs_; }
    size_t Count() const noexcept { return tabs_.size(); }

    int Neighbor(int offset) const;        // id of tab at (active + offset) wrapping around
    int AtIndex(int index) const;          // -1 when out of range
    bool PopClosed(ClosedTab& out);

private:
    std::vector<Tab> tabs_;
    std::vector<ClosedTab> closed_;
    int nextId_ = 1;
    int activeId_ = -1;
};

}  // namespace hb
