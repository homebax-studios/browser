#pragma once

#include <string>

class URL
{
public:

    explicit URL(
        const std::wstring& url
    );

    bool IsValid() const;

    const std::wstring& GetProtocol() const;

    const std::wstring& GetHost() const;

    const std::wstring& GetPath() const;

private:

    std::wstring m_protocol;

    std::wstring m_host;

    std::wstring m_path;

    bool m_valid = false;
};