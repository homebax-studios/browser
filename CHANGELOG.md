# Changelog – 2.1.3 (update package, changed files only)

## Fixed
- **Browser UI could not load**: it was pointed at a remote website while its own guard only allows `https://hb.local/ui/`. Now loads `ui/shell.html`.
- `tests/CMakeLists.txt` built only C++ so SQLite could not compile → now `C CXX`.
- Bookmarks accepted any URL scheme; now only safe schemes.
- Session restore trusted `session.json`; now validated, and saved atomically (no corruption on crash).
- Typed `edge://` / `chrome://` addresses no longer pass through (now treated as searches).
- Settings accepted any value (e.g. CSS in `accent`, zero sleep minutes); now validated.

## Added
- **Custom search engines** (Settings → Search), usable from the address bar; Google stays default.
- Safe download handling: executables/scripts are never launched from the downloads list.
- `core/Security.*` with unit tests (all core tests pass on Linux).

## Changed
- Renamed `resources/ui/chrome.*` → `shell.*`, `SetChromeHeight` → `SetShellHeight`; neutral "WebView2 Runtime" wording in UI/README.
- Unavoidable engine identifiers remain (`window.chrome.webview`, `msedgewebview2.exe`, `ICoreWebView2*`, Microsoft's URL, licence notice).

## Delete these old files after copying
`resources/ui/chrome.html`, `resources/ui/chrome.css`, `resources/ui/chrome.js`
(also the stale copy under `installer/HB Browser/resources/` – rebuild the installer).
