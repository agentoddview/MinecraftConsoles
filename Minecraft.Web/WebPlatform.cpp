#include "WebPlatform.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <string>

namespace {
EM_JS(void, WebSetStatus, (const char* text), {
    if (window.MinecraftWebUI) {
        window.MinecraftWebUI.setStatus(UTF8ToString(text));
    }
});

EM_JS(void, WebPublishUiState, (const char* json), {
    if (window.MinecraftWebUI) {
        window.MinecraftWebUI.applyState(UTF8ToString(json));
    }
});

EM_JS(void, WebRequestPointerLock, (), {
    if (window.MinecraftWebUI) {
        window.MinecraftWebUI.requestPointerLock();
    }
});

EM_JS(void, WebReleasePointerLock, (), {
    if (document.pointerLockElement && document.exitPointerLock) {
        document.exitPointerLock();
    }
});

EM_JS(void, WebRequestFullscreen, (), {
    var canvas = Module.canvas || document.getElementById('game-canvas');
    if (canvas && canvas.requestFullscreen) {
        canvas.requestFullscreen();
    }
});

EM_JS(void, WebInitializePersistentStorage, (), {
    var ensureDir = function(path) {
        try {
            if (!FS.analyzePath(path).exists) {
                FS.mkdir(path);
            }
        } catch (err) {
        }
    };

    ensureDir('/saves');
    ensureDir('/options');

    try {
        FS.mount(IDBFS, {}, '/saves');
    } catch (err) {
    }

    try {
        FS.mount(IDBFS, {}, '/options');
    } catch (err) {
    }

    FS.syncfs(true, function(err) {
        Module.ccall('webOnStorageReady', null, ['number'], [err ? 0 : 1]);
    });
});

EM_JS(void, WebFlushPersistentStorage, (), {
    if (typeof FS === 'undefined' || !FS.syncfs) {
        return;
    }

    FS.syncfs(false, function(err) {
        if (err) {
            console.error('MinecraftWeb: failed to flush persistent storage.', err);
        }
    });
});

EM_JS(int, WebIsPointerLocked, (), {
    var canvas = Module.canvas || document.getElementById('game-canvas');
    return document.pointerLockElement === canvas ? 1 : 0;
});
} // namespace

namespace web {

double BrowserPlatform::NowMs() const
{
    return emscripten_get_now();
}

void BrowserPlatform::SetStatus(std::string_view text)
{
    const std::string status(text);
    WebSetStatus(status.c_str());
}

void BrowserPlatform::PublishUiState(std::string_view json)
{
    const std::string payload(json);
    WebPublishUiState(payload.c_str());
}

void BrowserPlatform::RequestPointerLock()
{
    WebRequestPointerLock();
}

void BrowserPlatform::ReleasePointerLock()
{
    WebReleasePointerLock();
}

void BrowserPlatform::RequestFullscreen()
{
    WebRequestFullscreen();
}

void BrowserPlatform::Tick(double)
{
}

bool BrowserPlatform::SupportsMultiplayer() const
{
    return false;
}

bool BrowserPlatform::SupportsExistingSaveImport() const
{
    return false;
}

void BrowserPlatform::InitializePersistentStorage() const
{
    WebInitializePersistentStorage();
}

void BrowserPlatform::FlushPersistentStorage() const
{
    WebFlushPersistentStorage();
}

bool BrowserPlatform::IsPointerLocked() const
{
    return WebIsPointerLocked() != 0;
}

} // namespace web
