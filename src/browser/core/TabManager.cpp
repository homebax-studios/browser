#include "TabManager.h"
#include "Util.h"

#include <algorithm>

namespace hb {

int TabManager::IndexOf(int id) const
{
    for (size_t i = 0; i < tabs_.size(); ++i)
        if (tabs_[i].id == id) return static_cast<int>(i);
    return -1;
}

Tab* TabManager::Find(int id)
{
    const int i = IndexOf(id);
    return i < 0 ? nullptr : &tabs_[static_cast<size_t>(i)];
}

const Tab* TabManager::Find(int id) const
{
    const int i = IndexOf(id);
    return i < 0 ? nullptr : &tabs_[static_cast<size_t>(i)];
}

int TabManager::Add(const std::string& url, bool isPrivate, bool activate, int afterId)
{
    Tab tab;
    tab.id = nextId_++;
    tab.url = url;
    tab.isPrivate = isPrivate;
    tab.lastActive = NowMs();

    size_t pos = tabs_.size();
    const int after = IndexOf(afterId);
    if (after >= 0) pos = static_cast<size_t>(after) + 1;
    // Unpinned tabs never go in front of pinned ones.
    size_t firstUnpinned = 0;
    while (firstUnpinned < tabs_.size() && tabs_[firstUnpinned].pinned) ++firstUnpinned;
    pos = std::max(pos, firstUnpinned);

    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(pos), tab);
    if (activate || activeId_ < 0) Activate(tab.id);
    return tab.id;
}

int TabManager::Close(int id)
{
    const int index = IndexOf(id);
    if (index < 0) return activeId_;

    const Tab& t = tabs_[static_cast<size_t>(index)];
    if (!t.isPrivate && !t.url.empty() && t.url != "about:blank")
    {
        closed_.push_back({t.url, t.title, t.pinned});
        if (closed_.size() > 25) closed_.erase(closed_.begin());
    }

    const bool wasActive = id == activeId_;
    tabs_.erase(tabs_.begin() + index);
    if (tabs_.empty())
    {
        activeId_ = -1;
        return -1;
    }
    if (wasActive)
    {
        const size_t next = std::min(static_cast<size_t>(index), tabs_.size() - 1);  // right neighbour, else left
        Activate(tabs_[next].id);
    }
    return activeId_;
}

bool TabManager::Activate(int id)
{
    Tab* t = Find(id);
    if (!t) return false;
    activeId_ = id;
    t->lastActive = NowMs();
    return true;
}

bool TabManager::Move(int id, int newIndex)
{
    const int from = IndexOf(id);
    if (from < 0) return false;
    Tab tab = tabs_[static_cast<size_t>(from)];
    tabs_.erase(tabs_.begin() + from);

    size_t pinnedCount = 0;
    while (pinnedCount < tabs_.size() && tabs_[pinnedCount].pinned) ++pinnedCount;
    size_t lo = tab.pinned ? 0 : pinnedCount;
    size_t hi = tab.pinned ? pinnedCount : tabs_.size();
    size_t pos = static_cast<size_t>(std::max(0, newIndex));
    pos = std::min(std::max(pos, lo), hi);
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(pos), tab);
    return true;
}

bool TabManager::SetPinned(int id, bool pinned)
{
    Tab* t = Find(id);
    if (!t) return false;
    t->pinned = pinned;
    // Re-insert so pinned tabs stay grouped at the front.
    Tab copy = *t;
    const int from = IndexOf(id);
    tabs_.erase(tabs_.begin() + from);
    size_t pinnedCount = 0;
    while (pinnedCount < tabs_.size() && tabs_[pinnedCount].pinned) ++pinnedCount;
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(pinned ? pinnedCount : pinnedCount), copy);
    return true;
}

int TabManager::Neighbor(int offset) const
{
    if (tabs_.empty()) return -1;
    const int n = static_cast<int>(tabs_.size());
    int i = IndexOf(activeId_);
    if (i < 0) i = 0;
    return tabs_[static_cast<size_t>(((i + offset) % n + n) % n)].id;
}

int TabManager::AtIndex(int index) const
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return -1;
    return tabs_[static_cast<size_t>(index)].id;
}

bool TabManager::PopClosed(ClosedTab& out)
{
    if (closed_.empty()) return false;
    out = closed_.back();
    closed_.pop_back();
    return true;
}

}  // namespace hb
