// Platform-neutral security helpers (no Win32), unit-tested in tests/core_test.cpp.
#pragma once
#include <string>

namespace hb {

// True for file types that run code when opened (exe, bat, js, msi, lnk, ps1 ...). Such downloads are never
// opened directly from the downloads page; the user can still reveal them in the folder.
bool IsDangerousFileName(const std::string& fileName);

// Removes path separators, control characters and Windows-reserved characters/names from a file name.
std::string SanitizeFileName(const std::string& name);

}  // namespace hb
