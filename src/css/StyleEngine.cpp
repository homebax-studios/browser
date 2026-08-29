#include "StyleEngine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

std::string StyleEngine::Trim(
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

std::string StyleEngine::ToLower(
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

std::vector<std::string>
StyleEngine::SplitBySpaces(
    const std::string& value
)
{
    std::vector<std::string> result;

    std::stringstream stream(
        value
    );

    std::string item;

    while (
        stream >> item
        )
    {
        result.push_back(
            item
        );
    }

    return result;
}

void StyleEngine::SetRules(
    const std::vector<CSSRule>& rules
)
{
    m_rules =
        rules;
}

void StyleEngine::AddCSS(
    const std::string& css
)
{
    CSSParser parser;

    std::vector<CSSRule> rules =
        parser.Parse(
            css
        );

    for (
        const CSSRule& rule :
        rules
        )
    {
        m_rules.push_back(
            rule
        );
    }
}

bool StyleEngine::HasAttribute(
    DOMNode* node,
    const std::string& name
)
{
    if (!node)
    {
        return false;
    }

    return
        node->attributes.find(
            name
        ) !=
        node->attributes.end();
}

bool StyleEngine::HasClass(
    DOMNode* node,
    const std::string& className
)
{
    if (!node)
    {
        return false;
    }

    auto it =
        node->attributes.find(
            "class"
        );

    if (
        it ==
        node->attributes.end()
        )
    {
        return false;
    }

    std::vector<std::string>
        classes =
        SplitBySpaces(
            it->second
        );

    for (
        const std::string& item :
        classes
        )
    {
        if (
            item ==
            className
            )
        {
            return true;
        }
    }

    return false;
}

bool StyleEngine::MatchSimpleSelector(
    DOMNode* node,
    const std::string& selector
)
{
    if (!node)
    {
        return false;
    }

    std::string s =
        Trim(
            selector
        );

    if (s.empty())
    {
        return false;
    }

    if (s == "*")
    {
        return true;
    }

    // ============================================================
    // ATTRIBUTE SELECTORS
    //
    // [disabled]
    // [type="button"]
    // ============================================================

    size_t attributeStart =
        s.find('[');

    if (
        attributeStart !=
        std::string::npos
        )
    {
        size_t attributeEnd =
            s.find(
                ']',
                attributeStart
            );

        if (
            attributeEnd !=
            std::string::npos
            )
        {
            std::string inside =
                s.substr(
                    attributeStart + 1,
                    attributeEnd -
                    attributeStart -
                    1
                );

            inside =
                Trim(
                    inside
                );

            size_t equals =
                inside.find('=');

            if (
                equals ==
                std::string::npos
                )
            {
                if (
                    !HasAttribute(
                        node,
                        inside
                    )
                    )
                {
                    return false;
                }
            }
            else
            {
                std::string name =
                    Trim(
                        inside.substr(
                            0,
                            equals
                        )
                    );

                std::string expected =
                    Trim(
                        inside.substr(
                            equals + 1
                        )
                    );

                if (
                    expected.size() >= 2 &&
                    (
                        (
                            expected.front() ==
                            '"' &&
                            expected.back() ==
                            '"'
                            ) ||
                        (
                            expected.front() ==
                            '\'' &&
                            expected.back() ==
                            '\''
                            )
                        )
                    )
                {
                    expected =
                        expected.substr(
                            1,
                            expected.size() - 2
                        );
                }

                auto it =
                    node->attributes.find(
                        name
                    );

                if (
                    it ==
                    node->attributes.end() ||
                    it->second != expected
                    )
                {
                    return false;
                }
            }

            s.erase(
                attributeStart,
                attributeEnd -
                attributeStart +
                1
            );
        }
    }

    // ============================================================
    // TAG
    // ============================================================

    size_t position = 0;

    while (
        position <
        s.size() &&
        s[position] != '#' &&
        s[position] != '.'
        )
    {
        ++position;
    }

    if (position > 0)
    {
        std::string tag =
            ToLower(
                s.substr(
                    0,
                    position
                )
            );

        if (
            !tag.empty() &&
            ToLower(
                node->tagName
            ) != tag
            )
        {
            return false;
        }
    }

    // ============================================================
    // ID + CLASS
    // ============================================================

    while (
        position <
        s.size()
        )
    {
        char type =
            s[position];

        if (
            type != '#' &&
            type != '.'
            )
        {
            ++position;

            continue;
        }

        size_t start =
            position + 1;

        size_t end =
            start;

        while (
            end <
            s.size() &&
            s[end] != '#' &&
            s[end] != '.'
            )
        {
            ++end;
        }

        std::string value =
            s.substr(
                start,
                end - start
            );

        if (
            value.empty()
            )
        {
            return false;
        }

        if (type == '#')
        {
            auto it =
                node->attributes.find(
                    "id"
                );

            if (
                it ==
                node->attributes.end() ||
                it->second != value
                )
            {
                return false;
            }
        }
        else
        {
            if (
                !HasClass(
                    node,
                    value
                )
                )
            {
                return false;
            }
        }

        position =
            end;
    }

    return true;
}

StyleEngine::MatchResult
StyleEngine::MatchSelector(
    DOMNode* node,
    const std::string& selector
)
{
    MatchResult result;

    if (!node)
    {
        return result;
    }

    std::string s =
        Trim(
            selector
        );

    if (s.empty())
    {
        return result;
    }

    // ============================================================
    // SIMPLE SELECTOR
    // ============================================================

    if (
        s.find(' ') ==
        std::string::npos &&
        s.find('>') ==
        std::string::npos
        )
    {
        if (
            MatchSimpleSelector(
                node,
                s
            )
            )
        {
            result.matched = true;

            if (
                s.find('#') !=
                std::string::npos
                )
            {
                result.idCount++;
            }

            if (
                s.find('.') !=
                std::string::npos
                )
            {
                result.classCount++;
            }

            if (
                s != "*" &&
                s[0] != '.' &&
                s[0] != '#'
                )
            {
                result.elementCount++;
            }
        }

        return result;
    }

    // ============================================================
    // DESCENDANT SELECTOR
    //
    // .container p
    // body .title
    // ============================================================

    std::vector<std::string>
        parts =
        SplitBySpaces(
            s
        );

    if (parts.empty())
    {
        return result;
    }

    DOMNode* current =
        node;

    int specificityId = 0;
    int specificityClass = 0;
    int specificityElement = 0;

    for (
        int i =
        static_cast<int>(
            parts.size()
            ) - 1;
        i >= 0;
        --i
        )
    {
        const std::string&
            part =
            parts[i];

        if (!current)
        {
            return result;
        }

        if (
            !MatchSimpleSelector(
                current,
                part
            )
            )
        {
            // Find matching ancestor.
            //
            // DOMNode needs parent support for a complete
            // selector engine. Without it we cannot safely
            // walk upward here.
            return result;
        }

        if (
            part.find('#') !=
            std::string::npos
            )
        {
            ++specificityId;
        }

        if (
            part.find('.') !=
            std::string::npos ||
            part.find('[') !=
            std::string::npos
            )
        {
            ++specificityClass;
        }

        if (
            part != "*" &&
            part[0] != '.' &&
            part[0] != '#'
            )
        {
            ++specificityElement;
        }

        break;
    }

    result.matched = true;

    result.idCount =
        specificityId;

    result.classCount =
        specificityClass;

    result.elementCount =
        specificityElement;

    return result;
}

int StyleEngine::ParsePixels(
    const std::string& value
)
{
    std::string v =
        ToLower(
            Trim(
                value
            )
        );

    if (
        v.empty() ||
        v == "auto" ||
        v == "normal"
        )
    {
        return 0;
    }

    if (
        v == "0"
        )
    {
        return 0;
    }

    std::string number;

    for (char c : v)
    {
        if (
            std::isdigit(
                static_cast<unsigned char>(c)
            ) ||
            c == '.' ||
            c == '-'
            )
        {
            number += c;
        }
        else
        {
            break;
        }
    }

    if (number.empty())
    {
        return 0;
    }

    try
    {
        double valueNumber =
            std::stod(
                number
            );

        if (
            v.find("rem") !=
            std::string::npos
            )
        {
            return static_cast<int>(
                valueNumber * 16.0
                );
        }

        if (
            v.find("em") !=
            std::string::npos
            )
        {
            return static_cast<int>(
                valueNumber * 16.0
                );
        }

        if (
            v.find("pt") !=
            std::string::npos
            )
        {
            return static_cast<int>(
                valueNumber * 1.333
                );
        }

        return static_cast<int>(
            valueNumber
            );
    }
    catch (...)
    {
        return 0;
    }
}

int StyleEngine::ParseFirstPixel(
    const std::string& value
)
{
    std::vector<std::string>
        values =
        SplitBySpaces(
            value
        );

    if (values.empty())
    {
        return 0;
    }

    return ParsePixels(
        values[0]
    );
}

int StyleEngine::ParseFontSize(
    const std::string& value
)
{
    std::string v =
        ToLower(
            Trim(
                value
            )
        );

    if (v == "xx-small")
        return 9;

    if (v == "x-small")
        return 10;

    if (v == "small")
        return 13;

    if (v == "medium")
        return 16;

    if (v == "large")
        return 18;

    if (v == "x-large")
        return 24;

    if (v == "xx-large")
        return 32;

    return ParsePixels(
        v
    );
}

void StyleEngine::ApplyShorthandMargin(
    ComputedStyle& style,
    const std::string& value
)
{
    std::vector<std::string>
        values =
        SplitBySpaces(
            value
        );

    if (values.size() == 1)
    {
        style.margin =
            ParsePixels(
                values[0]
            );
    }
    else if (values.size() == 2)
    {
        style.margin =
            ParsePixels(
                values[0]
            );
    }
    else if (values.size() == 3)
    {
        style.margin =
            ParsePixels(
                values[1]
            );
    }
    else if (values.size() >= 4)
    {
        style.margin =
            ParsePixels(
                values[1]
            );
    }
}

void StyleEngine::ApplyShorthandPadding(
    ComputedStyle& style,
    const std::string& value
)
{
    std::vector<std::string>
        values =
        SplitBySpaces(
            value
        );

    if (values.size() == 1)
    {
        style.padding =
            ParsePixels(
                values[0]
            );
    }
    else if (values.size() == 2)
    {
        style.padding =
            ParsePixels(
                values[0]
            );
    }
    else if (values.size() == 3)
    {
        style.padding =
            ParsePixels(
                values[1]
            );
    }
    else if (values.size() >= 4)
    {
        style.padding =
            ParsePixels(
                values[1]
            );
    }
}

void StyleEngine::ApplyShorthandBorder(
    ComputedStyle& style,
    const std::string& value
)
{
    std::vector<std::string>
        values =
        SplitBySpaces(
            value
        );

    for (
        const std::string& item :
        values
        )
    {
        std::string lower =
            ToLower(
                item
            );

        if (
            lower.find("px") !=
            std::string::npos ||
            std::isdigit(
                static_cast<unsigned char>(
                    lower[0]
                    )
            )
            )
        {
            style.borderWidth =
                ParsePixels(
                    lower
                );
        }

        if (
            lower == "red" ||
            lower == "blue" ||
            lower == "green" ||
            lower == "black" ||
            lower == "white" ||
            lower[0] == '#'
            )
        {
            style.borderColor =
                lower;
        }
    }
}

void StyleEngine::ApplyDeclaration(
    ComputedStyle& style,
    const CSSDeclaration& declaration
)
{
    const std::string&
        property =
        declaration.property;

    const std::string&
        value =
        declaration.value;

    if (
        property ==
        "color"
        )
    {
        style.color =
            value;
    }
    else if (
        property ==
        "background-color"
        )
    {
        style.backgroundColor =
            value;
    }
    else if (
        property ==
        "background"
        )
    {
        // Simple background-color extraction.
        style.backgroundColor =
            value;
    }
    else if (
        property ==
        "font-size"
        )
    {
        style.fontSize =
            ParseFontSize(
                value
            );
    }
    else if (
        property ==
        "font-weight"
        )
    {
        std::string v =
            ToLower(
                value
            );

        style.bold =
            v == "bold" ||
            v == "bolder" ||
            v == "600" ||
            v == "700" ||
            v == "800" ||
            v == "900";
    }
    else if (
        property ==
        "font-family"
        )
    {
        style.fontFamily =
            value;
    }
    else if (
        property ==
        "width"
        )
    {
        style.width =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "height"
        )
    {
        style.height =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "margin"
        )
    {
        ApplyShorthandMargin(
            style,
            value
        );
    }
    else if (
        property ==
        "margin-left" ||
        property ==
        "margin-right" ||
        property ==
        "margin-top" ||
        property ==
        "margin-bottom"
        )
    {
        style.margin =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "padding"
        )
    {
        ApplyShorthandPadding(
            style,
            value
        );
    }
    else if (
        property ==
        "padding-left" ||
        property ==
        "padding-right" ||
        property ==
        "padding-top" ||
        property ==
        "padding-bottom"
        )
    {
        style.padding =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "border"
        )
    {
        ApplyShorthandBorder(
            style,
            value
        );
    }
    else if (
        property ==
        "border-width"
        )
    {
        style.borderWidth =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "border-color"
        )
    {
        style.borderColor =
            value;
    }
    else if (
        property ==
        "border-radius"
        )
    {
        style.borderRadius =
            ParsePixels(
                value
            );
    }
    else if (
        property ==
        "text-align"
        )
    {
        style.textAlign =
            ToLower(
                value
            );
    }
    else if (
        property ==
        "display"
        )
    {
        style.display =
            ToLower(
                value
            );
    }
    else if (
        property ==
        "visibility"
        )
    {
        style.visible =
            ToLower(value) !=
            "hidden";
    }
    else if (
        property ==
        "opacity"
        )
    {
        try
        {
            style.opacity =
                std::stof(
                    value
                );
        }
        catch (...)
        {
        }
    }
}

ComputedStyle StyleEngine::ComputeStyle(
    DOMNode* node
)
{
    ComputedStyle style;

    if (!node)
    {
        return style;
    }

    // ============================================================
    // DEFAULT HTML STYLES
    // ============================================================

    std::string tag =
        ToLower(
            node->tagName
        );

    if (tag == "h1")
    {
        style.fontSize = 32;
        style.bold = true;
        style.margin = 20;
    }
    else if (tag == "h2")
    {
        style.fontSize = 24;
        style.bold = true;
        style.margin = 18;
    }
    else if (tag == "h3")
    {
        style.fontSize = 20;
        style.bold = true;
        style.margin = 16;
    }
    else if (
        tag == "h4" ||
        tag == "h5" ||
        tag == "h6"
        )
    {
        style.fontSize = 18;
        style.bold = true;
        style.margin = 14;
    }
    else if (tag == "p")
    {
        style.margin = 10;
    }
    else if (tag == "button")
    {
        style.padding = 8;
        style.margin = 5;
        style.backgroundColor =
            "#eeeeee";
    }

    // ============================================================
    // CASCADE
    // ============================================================

    struct Candidate
    {
        const CSSDeclaration*
            declaration;

        int id;

        int classes;

        int elements;

        size_t order;
    };

    std::vector<Candidate>
        candidates;

    size_t order = 0;

    for (
        const CSSRule& rule :
        m_rules
        )
    {
        MatchResult match =
            MatchSelector(
                node,
                rule.selector
            );

        if (!match.matched)
        {
            ++order;

            continue;
        }

        for (
            const CSSDeclaration&
            declaration :
            rule.declarations
            )
        {
            Candidate candidate;

            candidate.declaration =
                &declaration;

            candidate.id =
                match.idCount;

            candidate.classes =
                match.classCount;

            candidate.elements =
                match.elementCount;

            candidate.order =
                order;

            candidates.push_back(
                candidate
            );
        }

        ++order;
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& a,
            const Candidate& b)
        {
            if (
                a.declaration->important !=
                b.declaration->important
                )
            {
                return
                    !a.declaration->important &&
                    b.declaration->important;
            }

            if (a.id != b.id)
            {
                return a.id < b.id;
            }

            if (a.classes != b.classes)
            {
                return
                    a.classes <
                    b.classes;
            }

            if (a.elements != b.elements)
            {
                return
                    a.elements <
                    b.elements;
            }

            return
                a.order <
                b.order;
        }
    );

    for (
        const Candidate& candidate :
        candidates
        )
    {
        ApplyDeclaration(
            style,
            *candidate.declaration
        );
    }

    return style;
}