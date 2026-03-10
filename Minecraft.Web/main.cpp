#include "WebApp.h"
#include "WebInput.h"
#include "WebPlatform.h"
#include "WebRenderer.h"
#include "WebStorage.h"

#include <emscripten/emscripten.h>

namespace {
web::BrowserPlatform g_platform;
web::WebStorage g_storage("/saves", "/options");
web::WebInput g_input;
web::WebRenderer g_renderer;
web::WebApp g_app(g_platform, g_storage, g_input, g_renderer);

void MainLoop()
{
    g_app.Tick();
}
} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void webOnStorageReady(int success)
{
    g_app.OnStorageReady(success != 0);
}

EMSCRIPTEN_KEEPALIVE void webRequestUiSync()
{
    g_app.RequestUiSync();
}

EMSCRIPTEN_KEEPALIVE void webShowWorldSelection()
{
    g_app.ShowWorldSelection();
}

EMSCRIPTEN_KEEPALIVE void webReturnToTitle()
{
    g_app.ReturnToTitle();
}

EMSCRIPTEN_KEEPALIVE void webStartNewWorld(const char* name)
{
    g_app.StartNewWorld(name != nullptr ? name : "New World");
}

EMSCRIPTEN_KEEPALIVE void webOpenWorldById(int worldId)
{
    g_app.OpenWorldById(worldId);
}

EMSCRIPTEN_KEEPALIVE void webDeleteWorldById(int worldId)
{
    g_app.DeleteWorldById(worldId);
}

EMSCRIPTEN_KEEPALIVE void webPauseGame()
{
    g_app.PauseGame();
}

EMSCRIPTEN_KEEPALIVE void webResumeGame()
{
    g_app.ResumeGame();
}

EMSCRIPTEN_KEEPALIVE void webOpenSettings()
{
    g_app.OpenSettings();
}

EMSCRIPTEN_KEEPALIVE void webCloseOverlay()
{
    g_app.CloseOverlay();
}

EMSCRIPTEN_KEEPALIVE void webToggleInventory()
{
    g_app.ToggleInventory();
}

EMSCRIPTEN_KEEPALIVE void webToggleChat()
{
    g_app.ToggleChat();
}

EMSCRIPTEN_KEEPALIVE void webRequestFullscreen()
{
    g_app.RequestFullscreen();
}

EMSCRIPTEN_KEEPALIVE void webRequestPointerLock()
{
    g_app.RequestPointerLock();
}

EMSCRIPTEN_KEEPALIVE void webUpdateSettingInt(const char* key, int value)
{
    if (key != nullptr) {
        g_app.UpdateSettingInt(key, value);
    }
}
}

int main()
{
    if (!g_app.Initialize()) {
        return 1;
    }

    emscripten_set_main_loop(MainLoop, 0, 1);
    return 0;
}
