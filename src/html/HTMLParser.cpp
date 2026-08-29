#include "HTMLParser.h"

#include <cctype>

static std::string ToLower(
    std::string value
)
{
    for (
        char& c :
        value
        )
    {
        c =
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        c
                        )
                )
                );
    }

    return value;
}

std::unique_ptr<DOMNode>
HTMLParser::Parse(
    const std::string& html
)
{
    auto document =
        std::make_unique<DOMNode>(
            NodeType::Document
        );

    size_t position = 0;

    ParseChildren(
        html,
        position,
        document.get()
    );

    return document;
}

void HTMLParser::ParseChildren(
    const std::string& html,
    size_t& position,
    DOMNode* parent
)
{
    while (
        position < html.size()
        )
    {
        // ========================================================
        // COMMENT
        // ========================================================

        if (
            position + 3 < html.size() &&
            html.compare(
                position,
                4,
                "<!--"
            ) == 0
            )
        {
            size_t end =
                html.find(
                    "-->",
                    position + 4
                );

            if (
                end ==
                std::string::npos
                )
            {
                position =
                    html.size();

                return;
            }

            position =
                end + 3;

            continue;
        }

        // ========================================================
        // TAG
        // ========================================================

        if (
            html[position] == '<'
            )
        {
            // ----------------------------------------------------
            // DOCTYPE
            // ----------------------------------------------------

            if (
                html.compare(
                    position,
                    9,
                    "<!DOCTYPE"
                ) == 0 ||
                html.compare(
                    position,
                    9,
                    "<!doctype"
                ) == 0
                )
            {
                while (
                    position < html.size() &&
                    html[position] != '>'
                    )
                {
                    position++;
                }

                if (
                    position < html.size()
                    )
                {
                    position++;
                }

                continue;
            }

            // ----------------------------------------------------
            // CLOSING TAG
            // ----------------------------------------------------

            if (
                position + 1 < html.size() &&
                html[position + 1] == '/'
                )
            {
                while (
                    position < html.size() &&
                    html[position] != '>'
                    )
                {
                    position++;
                }

                if (
                    position < html.size()
                    )
                {
                    position++;
                }

                return;
            }

            position++;

            std::string tagName =
                ReadTagName(
                    html,
                    position
                );

            tagName =
                ToLower(
                    tagName
                );

            if (
                tagName.empty()
                )
            {
                position++;

                continue;
            }

            // ====================================================
            // SCRIPT
            // ====================================================

            if (
                tagName == "script"
                )
            {
                size_t end =
                    html.find(
                        "</script",
                        position
                    );

                if (
                    end ==
                    std::string::npos
                    )
                {
                    position =
                        html.size();

                    return;
                }

                position =
                    end;

                while (
                    position < html.size() &&
                    html[position] != '>'
                    )
                {
                    position++;
                }

                if (
                    position < html.size()
                    )
                {
                    position++;
                }

                continue;
            }

            // ====================================================
            // STYLE
            // ====================================================

            if (
                tagName == "style"
                )
            {
                auto styleNode =
                    std::make_unique<DOMNode>(
                        NodeType::Element
                    );

                styleNode->tagName =
                    "style";

                DOMNode* stylePointer =
                    parent->AddChild(
                        std::move(
                            styleNode
                        )
                    );

                size_t close =
                    html.find(
                        "</style",
                        position
                    );

                if (
                    close ==
                    std::string::npos
                    )
                {
                    position =
                        html.size();

                    return;
                }

                std::string css =
                    html.substr(
                        position,
                        close - position
                    );

                auto textNode =
                    std::make_unique<DOMNode>(
                        NodeType::Text
                    );

                textNode->text =
                    css;

                stylePointer->AddChild(
                    std::move(
                        textNode
                    )
                );

                position =
                    close;

                while (
                    position < html.size() &&
                    html[position] != '>'
                    )
                {
                    position++;
                }

                if (
                    position < html.size()
                    )
                {
                    position++;
                }

                continue;
            }

            // ====================================================
            // ELEMENT
            // ====================================================

            auto element =
                std::make_unique<DOMNode>(
                    NodeType::Element
                );

            element->tagName =
                tagName;

            DOMNode* elementPointer =
                parent->AddChild(
                    std::move(
                        element
                    )
                );

            // ====================================================
            // ATTRIBUTES
            // ====================================================

            bool selfClosing =
                false;

            ReadAttributes(
                html,
                position,
                elementPointer
            );

            // ====================================================
            // VOID ELEMENT
            // ====================================================

            if (
                tagName == "br" ||
                tagName == "img" ||
                tagName == "hr" ||
                tagName == "meta" ||
                tagName == "link" ||
                tagName == "input" ||
                tagName == "source" ||
                tagName == "area" ||
                tagName == "base" ||
                tagName == "col" ||
                tagName == "embed" ||
                tagName == "param" ||
                tagName == "track" ||
                tagName == "wbr"
                )
            {
                continue;
            }

            (void)selfClosing;

            // ====================================================
            // CHILDREN
            // ====================================================

            ParseChildren(
                html,
                position,
                elementPointer
            );
        }
        else
        {
            // ====================================================
            // TEXT
            // ====================================================

            std::string text =
                ReadText(
                    html,
                    position
                );

            if (
                !text.empty()
                )
            {
                auto textNode =
                    std::make_unique<DOMNode>(
                        NodeType::Text
                    );

                textNode->text =
                    text;

                parent->AddChild(
                    std::move(
                        textNode
                    )
                );
            }
        }
    }
}

std::string HTMLParser::ReadTagName(
    const std::string& html,
    size_t& position
)
{
    std::string result;

    while (
        position < html.size()
        )
    {
        char c =
            html[position];

        if (
            std::isalnum(
                static_cast<unsigned char>(
                    c
                    )
            ) ||
            c == '-' ||
            c == ':'
            )
        {
            result +=
                static_cast<char>(
                    std::tolower(
                        static_cast<unsigned char>(
                            c
                            )
                    )
                    );

            position++;
        }
        else
        {
            break;
        }
    }

    return result;
}

std::string HTMLParser::ReadText(
    const std::string& html,
    size_t& position
)
{
    std::string result;

    while (
        position < html.size() &&
        html[position] != '<'
        )
    {
        result +=
            html[position];

        position++;
    }

    return result;
}

void HTMLParser::SkipAttributes(
    const std::string& html,
    size_t& position
)
{
    while (
        position < html.size() &&
        html[position] != '>'
        )
    {
        position++;
    }

    if (
        position < html.size()
        )
    {
        position++;
    }
}

void HTMLParser::ReadAttributes(
    const std::string& html,
    size_t& position,
    DOMNode* node
)
{
    if (!node)
    {
        return;
    }

    while (
        position < html.size()
        )
    {
        // ========================================================
        // END TAG
        // ========================================================

        if (
            html[position] == '>'
            )
        {
            position++;

            return;
        }

        // ========================================================
        // SELF CLOSING
        // ========================================================

        if (
            html[position] == '/'
            )
        {
            position++;

            if (
                position < html.size() &&
                html[position] == '>'
                )
            {
                position++;

                return;
            }

            continue;
        }

        // ========================================================
        // WHITESPACE
        // ========================================================

        if (
            std::isspace(
                static_cast<unsigned char>(
                    html[position]
                    )
            )
            )
        {
            position++;

            continue;
        }

        // ========================================================
        // NAME
        // ========================================================

        std::string name;

        while (
            position < html.size() &&
            !std::isspace(
                static_cast<unsigned char>(
                    html[position]
                    )
            ) &&
            html[position] != '=' &&
            html[position] != '>'
            )
        {
            name +=
                html[position];

            position++;
        }

        name =
            ToLower(
                name
            );

        // ========================================================
        // WHITESPACE
        // ========================================================

        while (
            position < html.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    html[position]
                    )
            )
            )
        {
            position++;
        }

        // ========================================================
        // NO VALUE
        // ========================================================

        if (
            position >= html.size() ||
            html[position] != '='
            )
        {
            node->attributes[name] =
                "";

            continue;
        }

        position++;

        // ========================================================
        // WHITESPACE
        // ========================================================

        while (
            position < html.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    html[position]
                    )
            )
            )
        {
            position++;
        }

        // ========================================================
        // VALUE
        // ========================================================

        std::string value;

        if (
            position < html.size() &&
            (
                html[position] == '"' ||
                html[position] == '\''
                )
            )
        {
            char quote =
                html[position];

            position++;

            while (
                position < html.size() &&
                html[position] != quote
                )
            {
                value +=
                    html[position];

                position++;
            }

            if (
                position < html.size()
                )
            {
                position++;
            }
        }
        else
        {
            while (
                position < html.size() &&
                !std::isspace(
                    static_cast<unsigned char>(
                        html[position]
                        )
                ) &&
                html[position] != '>'
                )
            {
                value +=
                    html[position];

                position++;
            }
        }

        node->attributes[name] =
            value;
    }
}