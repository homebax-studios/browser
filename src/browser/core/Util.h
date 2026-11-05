// Platform-neutral helpers (no Win32 here, so the core can be reused on Android/Linux later).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace hb {

std::string Trim(std::string_view s);
std::string ToLower(std::string s);
bool StartsWith(std::string_view s, std::string_view prefix);
bool EndsWith(std::string_view s, std::string_view suffix);
std::string ReplaceAll(std::string s, std::string_view from, std::string_view to);

// Percent-encode UTF-8 text. spaceAsPlus is used for search queries.
std::string UrlEncode(std::string_view s, bool spaceAsPlus = true);

// Lower-cased scheme without ':' ("" when none).
std::string SchemeOf(const std::string& url);
// Lower-cased host without port/userinfo ("" when none).
std::string HostOf(const std::string& url);

// <0, 0, >0 like strcmp for dotted versions ("2.1.10" > "2.1.2").
int CompareVersions(const std::string& a, const std::string& b);

int64_t NowMs();
std::string HumanSize(int64_t bytes);

}  // namespace hb
