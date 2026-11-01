#pragma once

#include <windows.h>

class Application {
public:
    explicit Application(HINSTANCE hInstance);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize(int nCmdShow);
    int Run();
    void Shutdown();

private:
    HINSTANCE m_hInstance = nullptr;
    bool m_initialized = false;
};