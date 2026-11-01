#include "WebViewManager.h"

#include <algorithm>
#include <utility>

#include <wrl.h>

using Microsoft::WRL::Callback;

WebViewManager::WebViewManager(HWND parent)
    : parent_(parent)
{
}

WebViewManager::~WebViewManager()
{
    views_.clear();
    environment_.reset();
}

void WebViewManager::Initialize()
{
    if (environmentRequested_)
        return;

    environmentRequested_ = true;

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT
            {
                if (FAILED(result) || !environment)
                    return result;

                environment_ = environment;

                auto pending = std::move(pendingTabs_);
                pendingTabs_.clear();

                for (const auto& item : pending)
                    CreateController(item.first, item.second);

                return S_OK;
            }).Get()
    );
}

void WebViewManager::CreateTab(int id, const std::wstring& url)
{
    if (!environment_)
    {
        pendingTabs_.emplace_back(id, url);
        Initialize();
        return;
    }

    CreateController(id, url);
}

void WebViewManager::CreateController(int id, const std::wstring& url)
{
    if (!environment_ || views_.contains(id))
        return;

    environment_->CreateCoreWebView2Controller(
        parent_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, id, url](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
            {
                if (FAILED(result) || !controller)
                    return result;

                View view;
                view.controller = controller;
                view.initialUrl = url;

                if (FAILED(controller->get_CoreWebView2(view.webView.put())))
                    return E_FAIL;

                views_.emplace(id, std::move(view));

                auto& created = views_.at(id);
                created.controller->put_Bounds(bounds_);
                created.controller->put_IsVisible(id == activeId_);
                created.webView->Navigate(url.c_str());

                return S_OK;
            }).Get()
    );
}

void WebViewManager::CloseTab(int id)
{
    views_.erase(id);
    pendingTabs_.erase(
        std::remove_if(pendingTabs_.begin(), pendingTabs_.end(),
            [id](const auto& item) { return item.first == id; }),
        pendingTabs_.end()
    );
}

void WebViewManager::SetActiveTab(int id)
{
    activeId_ = id;
    ApplyVisibility();
}

void WebViewManager::ApplyVisibility()
{
    for (auto& [id, view] : views_)
        view.controller->put_IsVisible(id == activeId_);
}

void WebViewManager::Navigate(int id, const std::wstring& url)
{
    const auto it = views_.find(id);
    if (it != views_.end() && it->second.webView)
        it->second.webView->Navigate(url.c_str());
    else
    {
        for (auto& pending : pendingTabs_)
            if (pending.first == id)
                pending.second = url;
    }
}

void WebViewManager::GoBack(int id)
{
    const auto it = views_.find(id);
    if (it != views_.end() && it->second.webView)
    {
        BOOL canGoBack = FALSE;
        it->second.webView->get_CanGoBack(&canGoBack);
        if (canGoBack) it->second.webView->GoBack();
    }
}

void WebViewManager::GoForward(int id)
{
    const auto it = views_.find(id);
    if (it != views_.end() && it->second.webView)
    {
        BOOL canGoForward = FALSE;
        it->second.webView->get_CanGoForward(&canGoForward);
        if (canGoForward) it->second.webView->GoForward();
    }
}

void WebViewManager::Reload(int id)
{
    const auto it = views_.find(id);
    if (it != views_.end() && it->second.webView)
        it->second.webView->Reload();
}

void WebViewManager::UpdateBounds(const RECT& bounds)
{
    bounds_ = bounds;
    for (auto& [id, view] : views_)
        view.controller->put_Bounds(bounds_);
}
