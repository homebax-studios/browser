# Security Policy

## HB Browser

Homebax Studios takes the security of HB Browser seriously.

We appreciate responsible security research and reports that help keep HB Browser and its users safe.

## Supported Versions

Security updates are provided for the latest stable version of HB Browser.

Older versions may no longer receive security fixes.

| Version | Supported |
|---|---|
| Latest stable | Yes |
| Older versions | No |

## Reporting a Vulnerability

If you discover a security vulnerability in HB Browser, please report it responsibly.

When submitting a report, include:

- A description of the vulnerability.
- Steps to reproduce the issue.
- Potential security impact.
- Screenshots or proof of concept, if available.

Please do not publicly disclose vulnerabilities before Homebax Studios has had a reasonable opportunity to investigate them.

## Security Response

Homebax Studios will review submitted reports and may:

- Investigate the reported vulnerability.
- Develop and release a security fix.
- Publish security updates.
- Inform affected users when necessary.

Response times are not guaranteed.

## Scope

This policy covers security vulnerabilities directly related to HB Browser.

Third-party components, including Microsoft WebView2 and Windows, are subject to their respective security policies.

## Responsible Disclosure

Security researchers are expected to avoid:

- Unauthorized access to other people's data.
- Destructive testing.
- Disruption of services.
- Exploitation of vulnerabilities against other users.

## Contact

**Security Team:** Homebax Studios  
**Project:** HB Browser  
**Website:** https://homebax.vercel.app

If no dedicated security reporting email is available, use the project's official GitHub repository to report security issues privately where possible.

---

Copyright (c) 2026 Homebax Studios.
## Update 2.1.3 – hardening notes
- Settings values are validated (enums, `#RRGGBB` accent, number ranges, pinned/custom-engine URLs) both when set and when `settings.json` is loaded.
- `session.json` and bookmarks only accept http, https, file, ftp, view-source and `about:blank` URLs; session files are written atomically.
- Downloaded programs/scripts (exe, bat, js, msi, lnk, ps1 …) are never opened from the downloads page, only revealed in the folder.
- Custom search engines must be http(s) URLs with exactly one `%s`.

### Known limitations (honest list)
- Web content is rendered by the WebView2 runtime; its security depends on keeping that runtime updated.
- Login persistence relies on the site setting persistent cookies; "session" cookies are discarded on exit by the engine. Passwords use the runtime's own OS-protected store.
- No extension system, tab groups, native (non-HTML) UI, own DevTools panels or malicious-site database exist yet (see WORK_PROGRESS.md).
- Windows-only; this update was verified by unit tests of the platform-neutral core only, not by a Windows build.
