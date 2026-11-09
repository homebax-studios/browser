# HB Browser

<p align="center">
  <img src="assets/readme/banner.png" alt="HB Browser Banner" width="100%">
</p>

<h1 align="center">HB Browser</h1>

<p align="center">
  <strong>A modern browsing experience, built from scratch.</strong>
  <br>
  Designed for simplicity, performance, customization and the future of the web.
</p>

<p align="center">
  <a href="https://github.com/homebax-studios/browser/releases">
    <img src="https://img.shields.io/github/v/release/homebax-studios/browser?include_prereleases&style=for-the-badge&color=7c6fff&label=LATEST%20RELEASE" alt="Latest Release">
  </a>
  <a href="https://github.com/homebax-studios/browser">
    <img src="https://img.shields.io/github/stars/homebax-studios/browser?style=for-the-badge&color=7c6fff" alt="GitHub Stars">
  </a>
  <a href="https://github.com/homebax-studios/browser/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/LICENSE-GPL--3.0-7c6fff?style=for-the-badge" alt="License">
  </a>
  <img src="https://img.shields.io/badge/PLATFORM-Windows-0078D6?style=for-the-badge&logo=windows" alt="Platform">
</p>

<p align="center">
  <a href="https://github.com/homebax-studios/browser/releases">Download</a> •
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#technology-stack">Technology</a> •
  <a href="#community">Community</a>
</p>

---

## About

**HB Browser** is an independent web browser developed by **Homebax Studios**.

Built from scratch with a focus on modern design, usability and extensibility, HB Browser aims to deliver a fresh alternative to traditional web browsers.

The project combines native C++ technologies with modern web technologies to create a flexible desktop browsing experience.

Our goal is simple: build a browser that we actually want to use.

## Features

- 100% anonymity with no data theft (we do not track websites on our end)

### Browsing

- Multi-tab browsing.
- Modern web page rendering.
- Support for modern websites.
- Improved compatibility with 3D web content and games.
- Integrated browser navigation.

### Built-in Pages

HB Browser includes its own internal browser pages.

Examples:

| Page | Description |
|---|---|
| `hb://about` | Browser information |
| `hb://settings` | Browser settings |

Internal pages are designed specifically for HB Browser.

### User Experience

- Modern and clean interface.
- Dedicated settings interface.
- Native desktop application.
- Custom browser components.
- Continuously evolving functionality.

## Technology Stack

HB Browser uses a combination of native and web technologies.

| Technology | Purpose |
|---|---|
| C++ | Core application |
| CMake | Build system |
| Microsoft WebView2 | Web rendering |
| HTML | Interface structure |
| CSS | Interface styling |
| JavaScript | Interface functionality |
| SQLite | Data storage |
| nlohmann/json | JSON handling |
| spdlog | Logging |
| Microsoft WIL | Windows development utilities |

### Rendering Engine

HB Browser currently uses **Microsoft WebView2** for web content rendering.

This provides compatibility with modern web technologies while allowing the browser application to be developed using native Windows technologies.

## Installation

### Windows

1. Open the [Releases](https://github.com/homebax-studios/browser/releases) page.
2. Download the latest available installer.
3. Run the installer.
4. Follow the installation instructions.
5. Launch HB Browser.

**Note:** Microsoft Edge WebView2 Runtime is required for web rendering.

## Development

Want to contribute or build HB Browser yourself?

### Requirements

- Windows 10 or Windows 11.
- Visual Studio 2022.
- C++ Desktop Development workload.
- CMake.
- Git.
- Microsoft WebView2 SDK.

### Clone the repository

```bash
git clone
```

## Star History

<a href="https://www.star-history.com/?repos=homebax-studios%2Fbrowser&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=homebax-studios/browser&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=homebax-studios/browser&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=homebax-studios/browser&type=date&legend=top-left" />
 </picture>
</a>

