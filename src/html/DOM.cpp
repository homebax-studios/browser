#include "DOM.h"

#include <sstream>

DOMNode::DOMNode(
    NodeType nodeType
)
    : type(nodeType)
{
}

DOMNode* DOMNode::AddChild(
    std::unique_ptr<DOMNode> child
)
{
    if (!child)
    {
        return nullptr;
    }

    child->parent = this;

    DOMNode* pointer =
        child.get();

    children.push_back(
        std::move(child)
    );

    return pointer;
}

std::string DOMNode::GetAttribute(
    const std::string& name
) const
{
    auto iterator =
        attributes.find(name);

    if (
        iterator ==
        attributes.end()
        )
    {
        return {};
    }

    return iterator->second;
}

bool DOMNode::HasAttribute(
    const std::string& name
) const
{
    return
        attributes.find(name)
        !=
        attributes.end();
}

bool DOMNode::HasClass(
    const std::string& className
) const
{
    std::string classes =
        GetAttribute("class");

    if (classes.empty())
    {
        return false;
    }

    std::istringstream stream(
        classes
    );

    std::string currentClass;

    while (
        stream >> currentClass
        )
    {
        if (
            currentClass ==
            className
            )
        {
            return true;
        }
    }

    return false;
}

bool DOMNode::IsElement(
    const std::string& name
) const
{
    return
        type == NodeType::Element &&
        tagName == name;
}