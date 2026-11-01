#include "Application.h"

namespace HB::Browser {
    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();
    void Shutdown();
}

Application::Application(HINSTANCE hInstance)
    : m_hInstance(hInstance) {
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize(int nCmdShow) {
    if (m_initialized) {
        return true;
    }

    if (!HB::Browser::Initialize(m_hInstance, nCmdShow)) {
        return false;
    }

    m_initialized = true;
    return true;
}

int Application::Run() {
    if (!m_initialized) {
        return 1;
    }

    return HB::Browser::Run();
}

void Application::Shutdown() {
    if (!m_initialized) {
        return;
    }

    HB::Browser::Shutdown();
    m_initialized = false;
}