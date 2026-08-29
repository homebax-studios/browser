#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

enum class NodeType
{
    Document,
    Element,
    Text
};

class DOMNode
{
public:

    NodeType type;

    std::string tagName;

    std::string text;

    std::map<std::string, std::string> attributes;

    std::vector<
        std::unique_ptr<DOMNode>
    > children;

    DOMNode* parent = nullptr;

    explicit DOMNode(
        NodeType nodeType
    );

    DOMNode* AddChild(
        std::unique_ptr<DOMNode> child
    );

    std::string GetAttribute(
        const std::string& name
    ) const;

    bool HasAttribute(
        const std::string& name
    ) const;

    bool HasClass(
        const std::string& className
    ) const;

    bool IsElement(
        const std::string& name
    ) const;
};