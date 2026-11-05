# HB Browser

Přenosný (portable) prohlížeč pro Windows od Homebax Studios. C++20, Microsoft WebView2, bez instalátoru a bez AI.
Moderní vzhled inspirovaný Atlasem: karty v záhlaví okna, zaoblený adresní řádek, světlý/tmavý motiv, vlastní barva zvýraznění.

## Co umí

| Oblast | Funkce |
|---|---|
| Karty | nová / zavřít / znovu otevřít, přetahování pořadí, připnuté karty, ztlumení, duplikace, zavřít ostatní / vpravo, ikony webů, indikátor načítání a zvuku, vyskakovací okna s funkčním `window.opener` (přihlašování přes OAuth) |
| Adresní řádek | URL i vyhledávání (Google, Bing, DuckDuckGo, Seznam, Brave, Startpage, Ecosia), návrhy ze záložek a historie, ikona zabezpečení, `hb://` stránky, Alt+Enter = nová karta |
| Záložky | hvězdička, panel záložek se složkami, správce záložek, import/export (HTML Netscape i JSON) |
| Historie, stahování | historie v SQLite s vyhledáváním, stažené soubory s průběhem, pauzou, zrušením, otevřením a „zobrazit ve složce“ |
| Stránka | hledání na stránce (s počtem), přiblížení pro každý web zvlášť, tisk, zdroj stránky, DevTools, celá obrazovka (F11 i video) |
| Soukromí | anonymní karty / okna, ochrana proti sledování (4 úrovně), mazání cookies, dat webů, cache, hesel, historie |
| Paměť | **Šetřič paměti**: nepoužívané karty se po čase uspí, později úplně uvolní z paměti a načtou se až při návratu; obnovené karty z minulé relace se vytvoří teprve při otevření |
| Systém | jedna instance (další spuštění předá odkaz běžícímu oknu), obnovení relace a pozice okna, čeština i angličtina, registrace jako výchozí prohlížeč bez admina |
| Ikony | všechny ikony jsou vestavěné SVG – funguje i offline, žádné CDN ani webfonty |

Klávesové zkratky jsou vypsané na stránce `hb://about` (Ctrl+T, W, Shift+T, L, F, D, H, J, +/−/0, Ctrl+Tab, F11, F12 …).

## Sestavení

1. Visual Studio 2022 (nebo Build Tools) s C++ a CMake 3.25+.
2. Knihovny do `thirdparty/` podle `thirdparty/README.md`.
3. ```
   cmake -S . -B build -A x64
   cmake --build build --config Release
   ```
   Výsledek: `build/bin/Release/HB Browser.exe` + složka `resources/` hned vedle (kopíruje se automaticky).
4. Přenosný ZIP: `cpack -C Release --config build/CPackConfig.cmake` → `HB-Browser-<verze>-win-x64-portable.zip`.

**Běh bez instalace:** Windows 11 i většina Windows 10 už mají WebView2 Runtime. Pokud ne, vložte složku *Fixed Version* runtime
do `WebView2Runtime\` vedle exe – prohlížeč ji použije sám. Data (profil, záložky, historie, nastavení, log) jsou ve složce
`data\` vedle exe; když tam nejde zapisovat, použije se `%LOCALAPPDATA%\HB Browser`.

Přepínače: `--private` (anonymní okno), `--new-instance` (ignorovat „jedno okno“), adresy a soubory jako argumenty.

## Struktura

```
src/browser/
  core/            přenosné jádro bez Win32 (adresy, nastavení, SQLite, model karet) – otestované na Linuxu
  webview/         WebViewManager – jediné místo, které mluví s WebView2
  app/             BrowserController – příkazy, stav, stahování, RPC pro hb:// stránky
  BrowserWindow    okno bez systémového rámu, DPI, celá obrazovka, popupy nad stránkou
platform/windows/  WinUtil (cesty, registr, dialogy, HTTP)
resources/ui/      lišta prohlížeče (HTML/CSS/JS)    resources/pages/  hb:// stránky
resources/shared/  ikony, most k nativní části, překlady, CSS    resources/translations/  cs.json, en.json
tests/             testy jádra: cmake -S tests -B build-tests
```

### Jak to spolu mluví
* Lišta (`resources/ui`) je jeden WebView přes celé okno, oříznutý (`SetWindowRgn`) na pruh nástrojů + právě otevřené popupy,
  takže nabídky a návrhy se kreslí **nad** stránkou. Každá karta je vlastní WebView.
* UI → nativní kód: `{cmd:"newTab", ...}`; nativní kód → UI: jeden JSON `state` (karty, navigace, záložky, motiv).
* Stránky `hb://x` jsou soubory `resources/pages/x.html` na virtuálním hostu `hb.local`; volají `hb.call("settings.set", {...})`.
  Nativní strana přijme zprávu jen z `https://hb.local/pages/…`, takže weby ovládat prohlížeč nemohou.
* Nový příkaz: přidat větev do `BrowserController::Exec` (z UI/zkratek) nebo `Rpc` (ze stránek).

### Android později
`src/browser/core` nepoužívá nic z Windows (UTF-8, `std::filesystem`, SQLite). Pro Android stačí napsat nový „shell“
(WebView/GeckoView + JNI) nad stejnými třídami `Settings`, `Storage`, `TabManager`, `ResolveInput`; HTML lištu lze přepoužít.

## Stav ověření (upřímně)
* Jádro (`core/`) přeloženo a prošlo testy na Linuxu (`tests/`).
* Win32 část (`BrowserWindow`, `WinUtil`, `main`) prošla kontrolou překladačem mingw-w64.
* Každé volání WebView2 jsem zkontroloval proti rozhraním v SDK (existence metod i správné rozhraní).
* Lišta a všechny `hb://` stránky běží v simulovaném prostředí s falešným nativním mostem.
* **`WebViewManager.cpp` a `BrowserController.cpp` jsem v tomto prostředí nemohl přeložit** (chybí WebView2.h) a program jsem ani nespustil.
  Při prvním sestavení tedy čekejte drobné chyby překladu; hlavní riziko ke kontrole za běhu je ořez lišty přes `SetWindowRgn`
  (popupy nad stránkou) a okraj 5 px pro změnu velikosti okna.
* Adresa kontroly aktualizací je v `BrowserController.cpp` (`kUpdateUrl`), očekává `{"version":"x.y.z","url":"…"}`.
