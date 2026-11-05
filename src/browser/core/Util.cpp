#include "Util.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace hb {

std::string Trim(std::string_view s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::string ToLower(std::string s)
{
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

bool StartsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool EndsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }

std::string ReplaceAll(std::string s, std::string_view from, std::string_view to)
{
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string UrlEncode(std::string_view s, bool spaceAsPlus)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += static_cast<char>(c);
        else if (c == ' ' && spaceAsPlus)
            out += '+';
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string SchemeOf(const std::string& url)
{
    if (url.empty() || !std::isalpha(static_cast<unsigned char>(url[0]))) return {};
    size_t i = 0;
    while (i < url.size() && (std::isalnum(static_cast<unsigned char>(url[i])) || url[i] == '+' || url[i] == '-' || url[i] == '.')) ++i;
    if (i < url.size() && url[i] == ':') return ToLower(url.substr(0, i));
    return {};
}

std::string HostOf(const std::string& url)
{
    const auto p = url.find("://");
    if (p == std::string::npos) return {};
    size_t b = p + 3;
    size_t e = url.find_first_of("/?#", b);
    std::string auth = url.substr(b, e == std::string::npos ? std::string::npos : e - b);
    const auto at = auth.rfind('@');
    if (at != std::string::npos) auth.erase(0, at + 1);
    if (!auth.empty() && auth[0] == '[')
    {
        const auto close = auth.find(']');
        return close == std::string::npos ? auth : ToLower(auth.substr(0, close + 1));
    }
    const auto colon = auth.rfind(':');
    if (colon != std::string::npos) auth.erase(colon);
    return ToLower(auth);
}

int CompareVersions(const std::string& a, const std::string& b)
{
    auto next = [](const std::string& s, size_t& i) {
        while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i]))) ++i;  // skip "v" and similar
        long v = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) v = v * 10 + (s[i++] - '0');
        if (i < s.size() && s[i] == '.') ++i;
        return v;
    };
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size())
    {
        const long x = next(a, i), y = next(b, j);
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string HumanSize(int64_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes < 0 ? 0 : bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    char buf[32];
    std::snprintf(buf, sizeof(buf), u == 0 ? "%.0f %s" : "%.1f %s", v, units[u]);
    return buf;
}

}  // namespace hb
