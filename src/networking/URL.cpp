#include "URL.h"

URL::URL(
    const std::wstring& url
)
{
    std::wstring input = url;

    // ============================================================
    // ADD HTTPS
    // ============================================================

    if (
        input.find(L"://")
        ==
        std::wstring::npos
        )
    {
        input =
            L"https://" +
            input;
    }

    // ============================================================
    // PROTOCOL
    // ============================================================

    size_t protocolEnd =
        input.find(L"://");

    if (
        protocolEnd ==
        std::wstring::npos
        )
    {
        return;
    }

    m_protocol =
        input.substr(
            0,
            protocolEnd
        );

    // ============================================================
    // HOST
    // ============================================================

    size_t hostStart =
        protocolEnd + 3;

    size_t pathStart =
        input.find(
            L'/',
            hostStart
        );

    if (
        pathStart ==
        std::wstring::npos
        )
    {
        m_host =
            input.substr(
                hostStart
            );

        m_path =
            L"/";
    }
    else
    {
        m_host =
            input.substr(
                hostStart,
                pathStart - hostStart
            );

        m_path =
            input.substr(
                pathStart
            );
    }

    // ============================================================
    // VALID
    // ============================================================

    m_valid =
        !m_protocol.empty() &&
        !m_host.empty();
}

bool URL::IsValid() const
{
    return m_valid;
}

const std::wstring&
URL::GetProtocol() const
{
    return m_protocol;
}

const std::wstring&
URL::GetHost() const
{
    return m_host;
}

const std::wstring&
URL::GetPath() const
{
    return m_path;
}