#pragma once

#include <string>
#include <vector>

struct CSSDeclaration
{
    std::string property;
    std::string value;
    bool important = false;
};

struct CSSRule
{
    std::string selector;

    std::vector<CSSDeclaration> declarations;
};

class CSSParser
{
public:

    std::vector<CSSRule> Parse(
        const std::string& css
    );

private:

    static std::string Trim(
        const std::string& value
    );

    static std::string ToLower(
        std::string value
    );

    static std::string RemoveComments(
        const std::string& css
    );

    static std::vector<std::string> SplitSelectors(
        const std::string& selector
    );

    static std::vector<CSSDeclaration> ParseDeclarations(
        const std::string& declarations
    );
};