#include "Security.h"
#include "Util.h"

#include <algorithm>
#include <array>

namespace hb {

bool IsDangerousFileName(const std::string& fileName)
{
    static const std::array<const char*, 36> ext = {
        "exe", "com", "scr", "pif", "bat", "cmd", "msi", "msp", "mst", "lnk", "url", "js", "jse", "vbs", "vbe", "wsf",
        "wsh", "ps1", "psm1", "reg", "hta", "cpl", "jar", "dll", "sys", "inf", "gadget", "application", "appx",
        "msix", "appinstaller", "msc", "scf", "chm", "vhd", "iso"};
    std::string name = ToLower(Trim(fileName));
    // Windows ignores trailing dots and spaces, so "setup.exe. " is still an executable.
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    const auto dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    const std::string e = name.substr(dot + 1);
    return std::any_of(ext.begin(), ext.end(), [&](const char* x) { return e == x; });
}

std::string SanitizeFileName(const std::string& name)
{
    std::string out;
    for (unsigned char c : name)
    {
        if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') out += '_';
        else out += static_cast<char>(c);
    }
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    out = Trim(out);
    static const char* reserved[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "lpt1", "lpt2", "lpt3"};
    std::string stem = ToLower(out.substr(0, out.find('.')));
    for (const char* r : reserved) if (stem == r) out.insert(0, "_");
    if (out.empty()) out = "download";
    if (out.size() > 200) out.resize(200);
    return out;
}

}  // namespace hb
