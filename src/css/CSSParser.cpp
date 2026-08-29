#include "CSSParser.h"

#include <algorithm>
#include <cctype>

std::string CSSParser::Trim(
    const std::string& value
)
{
    size_t start = 0;

    while (
        start < value.size() &&
        std::isspace(
            static_cast<unsigned char>(
                value[start]
                )
        )
        )
    {
        ++start;
    }

    size_t end =
        value.size();

    while (
        end > start &&
        std::isspace(
            static_cast<unsigned char>(
                value[end - 1]
                )
        )
        )
    {
        --end;
    }

    return value.substr(
        start,
        end - start
    );
}

std::string CSSParser::ToLower(
    std::string value
)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(
                std::tolower(c)
                );
        }
    );

    return value;
}

std::string CSSParser::RemoveComments(
    const std::string& css
)
{
    std::string result;

    size_t position = 0;

    while (position < css.size())
    {
        size_t start =
            css.find(
                "/*",
                position
            );

        if (
            start ==
            std::string::npos
            )
        {
            result +=
                css.substr(
                    position
                );

            break;
        }

        result +=
            css.substr(
                position,
                start - position
            );

        size_t end =
            css.find(
                "*/",
                start + 2
            );

        if (
            end ==
            std::string::npos
            )
        {
            break;
        }

        position =
            end + 2;
    }

    return result;
}

std::vector<std::string>
CSSParser::SplitSelectors(
    const std::string& selector
)
{
    std::vector<std::string> result;

    std::string current;

    int parentheses = 0;

    bool quote = false;

    char quoteCharacter = 0;

    for (char c : selector)
    {
        if (
            (c == '"' || c == '\'') &&
            !quote
            )
        {
            quote = true;
            quoteCharacter = c;

            current += c;

            continue;
        }

        if (
            quote &&
            c == quoteCharacter
            )
        {
            quote = false;

            current += c;

            continue;
        }

        if (!quote)
        {
            if (c == '(')
            {
                ++parentheses;
            }
            else if (c == ')')
            {
                if (parentheses > 0)
                {
                    --parentheses;
                }
            }

            if (
                c == ',' &&
                parentheses == 0
                )
            {
                std::string item =
                    Trim(
                        current
                    );

                if (!item.empty())
                {
                    result.push_back(
                        item
                    );
                }

                current.clear();

                continue;
            }
        }

        current += c;
    }

    std::string item =
        Trim(
            current
        );

    if (!item.empty())
    {
        result.push_back(
            item
        );
    }

    return result;
}

std::vector<CSSDeclaration>
CSSParser::ParseDeclarations(
    const std::string& declarations
)
{
    std::vector<CSSDeclaration> result;

    std::string current;

    int parentheses = 0;

    bool quote = false;

    char quoteCharacter = 0;

    auto process =
        [&]()
        {
            std::string declaration =
                Trim(
                    current
                );

            current.clear();

            if (declaration.empty())
            {
                return;
            }

            size_t colon =
                declaration.find(':');

            if (
                colon ==
                std::string::npos
                )
            {
                return;
            }

            std::string property =
                ToLower(
                    Trim(
                        declaration.substr(
                            0,
                            colon
                        )
                    )
                );

            std::string value =
                Trim(
                    declaration.substr(
                        colon + 1
                    )
                );

            if (
                property.empty() ||
                value.empty()
                )
            {
                return;
            }

            bool important =
                false;

            std::string lowerValue =
                ToLower(
                    value
                );

            size_t importantPosition =
                lowerValue.rfind(
                    "!important"
                );

            if (
                importantPosition !=
                std::string::npos &&
                importantPosition +
                10 ==
                lowerValue.size()
                )
            {
                important = true;

                value =
                    Trim(
                        value.substr(
                            0,
                            importantPosition
                        )
                    );
            }

            CSSDeclaration declarationObject;

            declarationObject.property =
                property;

            declarationObject.value =
                value;

            declarationObject.important =
                important;

            result.push_back(
                declarationObject
            );
        };

    for (char c : declarations)
    {
        if (
            (c == '"' || c == '\'') &&
            !quote
            )
        {
            quote = true;

            quoteCharacter = c;

            current += c;

            continue;
        }

        if (
            quote &&
            c == quoteCharacter
            )
        {
            quote = false;

            current += c;

            continue;
        }

        if (!quote)
        {
            if (c == '(')
            {
                ++parentheses;
            }
            else if (c == ')')
            {
                if (parentheses > 0)
                {
                    --parentheses;
                }
            }

            if (
                c == ';' &&
                parentheses == 0
                )
            {
                process();

                continue;
            }
        }

        current += c;
    }

    process();

    return result;
}

std::vector<CSSRule> CSSParser::Parse(
    const std::string& css
)
{
    std::vector<CSSRule> rules;

    std::string cleaned =
        RemoveComments(
            css
        );

    size_t position = 0;

    while (
        position <
        cleaned.size()
        )
    {
        while (
            position <
            cleaned.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    cleaned[position]
                    )
            )
            )
        {
            ++position;
        }

        if (
            position >=
            cleaned.size()
            )
        {
            break;
        }

        size_t open =
            cleaned.find(
                '{',
                position
            );

        if (
            open ==
            std::string::npos
            )
        {
            break;
        }

        std::string selector =
            Trim(
                cleaned.substr(
                    position,
                    open - position
                )
            );

        size_t close =
            open + 1;

        int depth = 1;

        bool quote = false;

        char quoteCharacter = 0;

        while (
            close <
            cleaned.size() &&
            depth > 0
            )
        {
            char c =
                cleaned[close];

            if (
                (c == '"' || c == '\'') &&
                !quote
                )
            {
                quote = true;

                quoteCharacter = c;
            }
            else if (
                quote &&
                c == quoteCharacter
                )
            {
                quote = false;
            }
            else if (!quote)
            {
                if (c == '{')
                {
                    ++depth;
                }
                else if (c == '}')
                {
                    --depth;
                }
            }

            ++close;
        }

        if (depth != 0)
        {
            break;
        }

        std::string body =
            cleaned.substr(
                open + 1,
                close - open - 2
            );

        // ========================================================
        // @media / @supports / @keyframes
        //
        // Zatím pøeskoèíme. Renderer je neumí správnì reprezentovat.
        // ========================================================

        if (
            selector.empty() ||
            selector[0] == '@'
            )
        {
            position =
                close;

            continue;
        }

        std::vector<std::string>
            selectors =
            SplitSelectors(
                selector
            );

        std::vector<CSSDeclaration>
            declarations =
            ParseDeclarations(
                body
            );

        for (
            const std::string&
            singleSelector :
            selectors
            )
        {
            if (
                singleSelector.empty()
                )
            {
                continue;
            }

            CSSRule rule;

            rule.selector =
                singleSelector;

            rule.declarations =
                declarations;

            rules.push_back(
                rule
            );
        }

        position =
            close;
    }

    return rules;
}