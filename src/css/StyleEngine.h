#pragma once

#include <string>
#include <vector>

#include "../html/DOM.h"
#include "CSSParser.h"

struct ComputedStyle
{
    std::string color =
        "black";

    std::string backgroundColor =
        "transparent";

    std::string fontFamily =
        "Segoe UI";

    int fontSize =
        16;

    bool bold =
        false;

    int width =
        0;

    int height =
        0;

    int margin =
        0;

    int padding =
        0;

    int borderWidth =
        0;

    int borderRadius =
        0;

    std::string borderColor =
        "black";

    std::string textAlign =
        "left";

    std::string display =
        "block";

    bool visible =
        true;

    float opacity =
        1.0f;
};

class StyleEngine
{
public:

    void SetRules(
        const std::vector<CSSRule>& rules
    );

    void AddCSS(
        const std::string& css
    );

    ComputedStyle ComputeStyle(
        DOMNode* node
    );

private:

    std::vector<CSSRule> m_rules;

    struct MatchResult
    {
        bool matched = false;

        int idCount = 0;

        int classCount = 0;

        int elementCount = 0;
    };

    static std::string Trim(
        const std::string& value
    );

    static std::string ToLower(
        std::string value
    );

    static std::vector<std::string>
        SplitBySpaces(
            const std::string& value
        );

    MatchResult MatchSelector(
        DOMNode* node,
        const std::string& selector
    );

    bool MatchSimpleSelector(
        DOMNode* node,
        const std::string& selector
    );

    bool HasClass(
        DOMNode* node,
        const std::string& className
    );

    bool HasAttribute(
        DOMNode* node,
        const std::string& name
    );

    int ParsePixels(
        const std::string& value
    );

    int ParseFontSize(
        const std::string& value
    );

    int ParseFirstPixel(
        const std::string& value
    );

    void ApplyDeclaration(
        ComputedStyle& style,
        const CSSDeclaration& declaration
    );

    void ApplyShorthandMargin(
        ComputedStyle& style,
        const std::string& value
    );

    void ApplyShorthandPadding(
        ComputedStyle& style,
        const std::string& value
    );

    void ApplyShorthandBorder(
        ComputedStyle& style,
        const std::string& value
    );
};