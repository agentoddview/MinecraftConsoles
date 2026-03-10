# Browser Prototype

This repository now includes a `MinecraftWeb` target for a browser-native prototype build.

## Scope

The current web target is intentionally limited to a local single-player POC:

- WebAssembly + WebGL 2 runtime under Emscripten
- Browser-native HTML/CSS/JS overlays for title, world select, pause, settings, HUD, inventory, and chat
- Browser-side save persistence through IDBFS under `/saves` and `/options`
- Keyboard and mouse input for a simple in-browser exploration loop

The web target does **not** currently include:

- The full Windows gameplay/runtime stack
- LAN or dedicated server support
- Leaderboards or platform identity
- Existing Windows save compatibility or import
- SWF/Iggy playback

## Build

Install the Emscripten SDK and activate it in your shell, then configure the web build:

```powershell
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target MinecraftWeb
```

The output bundle is generated as `build-web/MinecraftWeb.html` plus the matching JavaScript and WebAssembly artifacts.

## Run

Use a local HTTP server so the browser can load the generated assets correctly:

```powershell
emrun --no_browser --port 8080 build-web/MinecraftWeb.html
```

Then open the reported local URL in a desktop browser with WebGL 2 enabled.

## Deploy with Coolify

This repository includes a root-level `Dockerfile` so Coolify can deploy it with the Dockerfile build pack.

Recommended Coolify settings:

1. Create a new Application from your Git repository.
2. Choose the `Dockerfile` build pack.
3. Set the Base Directory to `/`.
4. Leave the Dockerfile path as `Dockerfile`.
5. In Network settings, set the Port to `80`.
6. Add your domain and enable automatic deployments if you want push-to-deploy behavior.
7. Optional: add a health check path of `/`.

Coolify will build the Emscripten site inside the build stage and serve the generated files from Nginx in the runtime stage.

## Notes

- The browser build relies on pointer lock for mouse-look while in the in-game mode.
- Save data is stored in IndexedDB by the browser profile that launched the page.
- The current renderer is a compatibility scaffold, not the final Minecraft rendering path.
