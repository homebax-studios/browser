#pragma once

#include "DOM.h"

#include <string>
#include <memory>

class HTMLParser
{
public:

    static std::unique_ptr<DOMNode> Parse(
        const std::string& html
    );

private:

    static void ParseChildren(
        const std::string& html,
        size_t& position,
        DOMNode* parent
    );

    static std::string ReadTagName(
        const std::string& html,
        size_t& position
    );

    static std::string ReadText(
        const std::string& html,
        size_t& position
    );

    static void SkipAttributes(
        const std::string& html,
        size_t& position
    );

    static void ReadAttributes(
        const std::string& html,
        size_t& position,
        DOMNode* node
    );
};