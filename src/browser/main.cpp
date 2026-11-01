#include "BrowserApp.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    BrowserApp app(instance);
    if (!app.Initialize(showCommand))
        return 1;

    return app.Run();
}
