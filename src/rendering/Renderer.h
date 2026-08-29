#pragma once

#include "../html/DOM.h"
#include "../css/StyleEngine.h"

#include <windows.h>
#include <string>

class Renderer
{
public:

    void SetStyleEngine(
        StyleEngine* styleEngine
    );

    void Render(
        HDC hdc,
        DOMNode* document,
        int width,
        int height,
        int scrollY,
        int& documentHeight
    );

private:

    void RenderNode(
        HDC hdc,
        DOMNode* node,
        int& x,
        int& y,
        int width
    );

    void RenderText(
        HDC hdc,
        DOMNode* node,
        int& x,
        int& y,
        int width,
        const ComputedStyle& style
    );

    void RenderElement(
        HDC hdc,
        DOMNode* node,
        int& x,
        int& y,
        int width
    );

    static std::wstring UTF8ToWide(
        const std::string& text
    );

    static std::wstring DecodeEntities(
        const std::wstring& text
    );

    static COLORREF ParseColor(
        const std::string& color
    );

private:

    StyleEngine* m_styleEngine =
        nullptr;

    int m_scrollY =
        0;
};