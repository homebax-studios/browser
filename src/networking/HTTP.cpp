#include "HTTP.h"

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

std::string HTTP::Get(
    const URL& url
)
{
    // ============================================================
    // SESSION
    // ============================================================

    HINTERNET session =
        WinHttpOpen(
            L"HomebaxBrowser/0.1",

            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,

            WINHTTP_NO_PROXY_NAME,

            WINHTTP_NO_PROXY_BYPASS,

            0
        );

    if (!session)
    {
        return {};
    }

    // ============================================================
    // HTTPS
    // ============================================================

    bool https =
        url.GetProtocol() ==
        L"https";

    // ============================================================
    // CONNECTION
    // ============================================================

    HINTERNET connection =
        WinHttpConnect(
            session,

            url.GetHost().c_str(),

            https
            ? INTERNET_DEFAULT_HTTPS_PORT
            : INTERNET_DEFAULT_HTTP_PORT,

            0
        );

    if (!connection)
    {
        WinHttpCloseHandle(session);

        return {};
    }

    // ============================================================
    // REQUEST
    // ============================================================

    HINTERNET request =
        WinHttpOpenRequest(
            connection,

            L"GET",

            url.GetPath().c_str(),

            nullptr,

            WINHTTP_NO_REFERER,

            WINHTTP_DEFAULT_ACCEPT_TYPES,

            https
            ? WINHTTP_FLAG_SECURE
            : 0
        );

    if (!request)
    {
        WinHttpCloseHandle(connection);

        WinHttpCloseHandle(session);

        return {};
    }

    // ============================================================
    // USER AGENT / ACCEPT
    // ============================================================

    const wchar_t* headers =
        L"Accept: text/html,application/xhtml+xml,"
        L"application/xml;q=0.9,*/*;q=0.8\r\n";

    // ============================================================
    // SEND
    // ============================================================

    BOOL sent =
        WinHttpSendRequest(
            request,

            headers,

            static_cast<DWORD>(-1),

            nullptr,

            0,

            0,

            0
        );

    if (!sent)
    {
        WinHttpCloseHandle(request);

        WinHttpCloseHandle(connection);

        WinHttpCloseHandle(session);

        return {};
    }

    // ============================================================
    // RECEIVE
    // ============================================================

    BOOL received =
        WinHttpReceiveResponse(
            request,
            nullptr
        );

    if (!received)
    {
        WinHttpCloseHandle(request);

        WinHttpCloseHandle(connection);

        WinHttpCloseHandle(session);

        return {};
    }

    // ============================================================
    // READ
    // ============================================================

    std::string result;

    while (true)
    {
        DWORD available = 0;

        if (
            !WinHttpQueryDataAvailable(
                request,
                &available
            )
            )
        {
            break;
        }

        if (available == 0)
        {
            break;
        }

        std::string buffer(
            available,
            '\0'
        );

        DWORD downloaded = 0;

        if (
            !WinHttpReadData(
                request,

                buffer.data(),

                available,

                &downloaded
            )
            )
        {
            break;
        }

        result.append(
            buffer.data(),
            downloaded
        );
    }

    // ============================================================
    // CLEANUP
    // ============================================================

    WinHttpCloseHandle(request);

    WinHttpCloseHandle(connection);

    WinHttpCloseHandle(session);

    return result;
}