#pragma once

#include "LegacyContent.h"
#include "WebInput.h"
#include "WebPlatform.h"
#include "WebRenderer.h"
#include "WebStorage.h"

#include <string>
#include <string_view>

namespace web {

class WebApp {
public:
    WebApp(BrowserPlatform& platform, WebStorage& storage, WebInput& input, WebRenderer& renderer);

    bool Initialize();
    void OnStorageReady(bool success);
    void Tick();
    void RequestUiSync();

    void ShowWorldSelection();
    void ShowCreateWorldMenu();
    void ReturnToTitle();
    void StartNewWorld(std::string_view name,
        std::string_view seed,
        int gameModeId,
        int difficulty,
        bool generateStructures,
        bool bonusChest);
    void OpenWorldById(int worldId);
    void DeleteWorldById(int worldId);

    void PauseGame();
    void ResumeGame();
    void OpenSettings();
    void CloseOverlay();
    void ToggleInventory();
    void ToggleChat();
    void RequestFullscreen();
    void RequestPointerLock();
    void UpdateSettingInt(std::string_view key, int value);

private:
    enum class Mode {
        Boot,
        Title,
        Worlds,
        CreateWorld,
        InGame,
        Paused,
        Settings,
        Inventory,
        Chat
    };

    void SetMode(Mode nextMode, bool requestPointerLock);
    void UpdateHotkeys();
    void UpdateSimulation(double deltaSeconds);
    void SaveCurrentWorld(bool flushPersistentStorage);
    void PublishUiState();
    std::string GetLegacyString(std::string_view key, std::string_view fallback) const;
    std::string GetDifficultyLabel(int difficulty) const;
    std::string GetGameModeLabel(int gameModeId) const;
    std::string GetCreatedModeLabel(const WorldRecord& world) const;

    static const char* ModeName(Mode mode);

    BrowserPlatform& platform_;
    WebStorage& storage_;
    WebInput& input_;
    WebRenderer& renderer_;
    LegacyContent legacyContent_;

    Mode mode_ = Mode::Boot;
    Mode modalReturnMode_ = Mode::Title;
    bool initialized_ = false;
    bool storageReady_ = false;
    bool rendererReady_ = false;
    bool inputReady_ = false;

    double lastFrameMs_ = 0.0;
    double lastUiPublishMs_ = 0.0;
    double lastPersistMs_ = 0.0;
    double worldTimeSeconds_ = 0.0;

    std::string status_ = "Booting browser runtime...";
    std::string mainMenuSplash_;
    int currentWorldId_ = -1;
    std::string currentWorldName_;
    WorldSnapshot player_;
};

} // namespace web
