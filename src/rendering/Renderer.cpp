#include "Renderer.h"

#include <algorithm>
#include <cctype>
#include <cwchar>

void Renderer::SetStyleEngine(
    StyleEngine* styleEngine
)
{
    m_styleEngine = styleEngine;
}

std::wstring Renderer::UTF8ToWide(
    const std::string& text
)
{
    if (text.empty())
    {
        return {};
    }

    int size =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );

    if (size <= 0)
    {
        return {};
    }

    std::wstring result(
        size,
        L'\0'
    );

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        size
    );

    return result;
}

std::wstring Renderer::DecodeEntities(
    const std::wstring& text
)
{
    std::wstring result = text;

    const std::pair<
        const wchar_t*,
        const wchar_t*
    > entities[] =
    {
        { L"&amp;",  L"&" },
        { L"&lt;",   L"<" },
        { L"&gt;",   L">" },
        { L"&quot;", L"\"" },
        { L"&#39;",  L"'" },
        { L"&apos;", L"'" },
        { L"&nbsp;", L"\x00A0" }
    };

    for (
        const auto& entity :
        entities
        )
    {
        size_t position = 0;

        while (
            (
                position =
                result.find(
                    entity.first,
                    position
                )
                )
            !=
            std::wstring::npos
            )
        {
            result.replace(
                position,
                wcslen(entity.first),
                entity.second
            );

            position +=
                wcslen(entity.second);
        }
    }

    return result;
}

COLORREF Renderer::ParseColor(
    const std::string& color
)
{
    std::string value = color;

    for (
        char& character :
        value
        )
    {
        character =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        character
                        )
                )
                );
    }

    if (value == "red")
    {
        return RGB(
            255,
            0,
            0
        );
    }

    if (value == "green")
    {
        return RGB(
            0,
            128,
            0
        );
    }

    if (value == "blue")
    {
        return RGB(
            0,
            0,
            255
        );
    }

    if (value == "white")
    {
        return RGB(
            255,
            255,
            255
        );
    }

    if (value == "black")
    {
        return RGB(
            0,
            0,
            0
        );
    }

    if (
        value == "gray" ||
        value == "grey"
        )
    {
        return RGB(
            128,
            128,
            128
        );
    }

    if (value == "yellow")
    {
        return RGB(
            255,
            255,
            0
        );
    }

    if (value == "transparent")
    {
        return RGB(
            255,
            255,
            255
        );
    }

    if (
        value.size() == 7 &&
        value[0] == '#'
        )
    {
        try
        {
            int red =
                std::stoi(
                    value.substr(
                        1,
                        2
                    ),
                    nullptr,
                    16
                );

            int green =
                std::stoi(
                    value.substr(
                        3,
                        2
                    ),
                    nullptr,
                    16
                );

            int blue =
                std::stoi(
                    value.substr(
                        5,
                        2
                    ),
                    nullptr,
                    16
                );

            return RGB(
                red,
                green,
                blue
            );
        }
        catch (...)
        {
        }
    }

    return RGB(
        0,
        0,
        0
    );
}

void Renderer::Render(
    HDC hdc,
    DOMNode* document,
    int width,
    int height,
    int scrollY,
    int& documentHeight
)
{
    m_scrollY = scrollY;

    documentHeight = 0;

    if (!document)
    {
        return;
    }

    RECT rect =
    {
        0,
        0,
        width,
        height
    };

    FillRect(
        hdc,
        &rect,
        reinterpret_cast<HBRUSH>(
            GetStockObject(
                WHITE_BRUSH
            )
            )
    );

    int x = 30;
    int y = 30;

    for (
        auto& child :
        document->children
        )
    {
        RenderNode(
            hdc,
            child.get(),
            x,
            y,
            width
        );
    }

    documentHeight =
        y + 40;
}

void Renderer::RenderNode(
    HDC hdc,
    DOMNode* node,
    int& x,
    int& y,
    int width
)
{
    if (!node)
    {
        return;
    }

    if (
        node->type ==
        NodeType::Text
        )
    {
        return;
    }

    if (
        node->type ==
        NodeType::Element
        )
    {
        if (
            node->tagName == "style" ||
            node->tagName == "script" ||
            node->tagName == "head"
            )
        {
            return;
        }

        RenderElement(
            hdc,
            node,
            x,
            y,
            width
        );

        return;
    }

    for (
        auto& child :
        node->children
        )
    {
        RenderNode(
            hdc,
            child.get(),
            x,
            y,
            width
        );
    }
}

void Renderer::RenderText(
    HDC hdc,
    DOMNode* node,
    int& x,
    int& y,
    int width,
    const ComputedStyle& style
)
{
    if (!node)
    {
        return;
    }

    std::wstring text =
        UTF8ToWide(
            node->text
        );

    text =
        DecodeEntities(
            text
        );

    if (text.empty())
    {
        return;
    }

    for (
        wchar_t& character :
        text
        )
    {
        if (
            character == L'\n' ||
            character == L'\r' ||
            character == L'\t'
            )
        {
            character = L' ';
        }
    }

    HFONT font =
        CreateFontW(
            style.fontSize,
            0,
            0,
            0,
            style.bold
            ? FW_BOLD
            : FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI"
        );

    HFONT oldFont =
        reinterpret_cast<HFONT>(
            SelectObject(
                hdc,
                font
            )
            );

    SetBkMode(
        hdc,
        TRANSPARENT
    );

    SetTextColor(
        hdc,
        ParseColor(
            style.color
        )
    );

    int availableWidth =
        width -
        x -
        40;

    if (style.width > 0)
    {
        availableWidth =
            style.width;
    }

    if (availableWidth < 100)
    {
        availableWidth = 100;
    }

    RECT textRect =
    {
        x + style.padding,

        y -
            m_scrollY +
            style.padding,

        x +
            availableWidth,

        y -
            m_scrollY +
            1000
    };

    DrawTextW(
        hdc,
        text.c_str(),
        static_cast<int>(
            text.length()
            ),
        &textRect,
        DT_WORDBREAK |
        DT_NOPREFIX
    );

    RECT measureRect =
    {
        0,
        0,
        availableWidth,
        0
    };

    DrawTextW(
        hdc,
        text.c_str(),
        static_cast<int>(
            text.length()
            ),
        &measureRect,
        DT_WORDBREAK |
        DT_CALCRECT |
        DT_NOPREFIX
    );

    int lineHeight =
        measureRect.bottom;

    if (
        lineHeight <
        style.fontSize + 4
        )
    {
        lineHeight =
            style.fontSize + 4;
    }

    y +=
        lineHeight +
        style.margin +
        style.padding;

    SelectObject(
        hdc,
        oldFont
    );

    DeleteObject(
        font
    );
}

void Renderer::RenderElement(
    HDC hdc,
    DOMNode* node,
    int& x,
    int& y,
    int width
)
{
    if (!node)
    {
        return;
    }

    ComputedStyle style;

    if (m_styleEngine)
    {
        style =
            m_styleEngine->ComputeStyle(
                node
            );
    }

    // ============================================================
    // MARGIN
    // ============================================================

    y +=
        style.margin;

    // ============================================================
    // BACKGROUND
    // ============================================================

    if (
        style.backgroundColor !=
        "transparent"
        )
    {
        int boxWidth =
            style.width > 0
            ? style.width
            : width - x - 40;

        int boxHeight =
            style.height > 0
            ? style.height
            : 0;

        if (boxHeight > 0)
        {
            RECT backgroundRect =
            {
                x,

                y -
                    m_scrollY,

                x +
                    boxWidth,

                y -
                    m_scrollY +
                    boxHeight
            };

            HBRUSH brush =
                CreateSolidBrush(
                    ParseColor(
                        style.backgroundColor
                    )
                );

            FillRect(
                hdc,
                &backgroundRect,
                brush
            );

            DeleteObject(
                brush
            );
        }
    }

    // ============================================================
    // TEXT + CHILDREN
    // ============================================================

    for (
        auto& child :
        node->children
        )
    {
        if (
            child->type ==
            NodeType::Text
            )
        {
            RenderText(
                hdc,
                child.get(),
                x,
                y,
                width,
                style
            );
        }
        else
        {
            // ----------------------------------------------------
            // DÙLEŽITÁ OPRAVA
            //
            // x + style.padding je výraz, nikoliv int promìnná.
            // RenderNode oèekává int&.
            // Proto vytvoøíme vlastní promìnnou.
            // ----------------------------------------------------

            int childX =
                x +
                style.padding;

            int childWidth =
                width -
                style.padding * 2;

            if (childWidth < 1)
            {
                childWidth = 1;
            }

            RenderNode(
                hdc,
                child.get(),
                childX,
                y,
                childWidth
            );
        }
    }

    // ============================================================
    // FIXED HEIGHT
    // ============================================================

    if (
        style.height > 0
        )
    {
        int required =
            y +
            style.height;

        if (
            required > y
            )
        {
            y =
                required;
        }
    }

    // ============================================================
    // PADDING
    // ============================================================

    y +=
        style.padding;

    // ============================================================
    // MARGIN
    // ============================================================

    y +=
        style.margin;
}