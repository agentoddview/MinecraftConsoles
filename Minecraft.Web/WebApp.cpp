#include "WebApp.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

std::string JsonEscape(std::string_view input)
{
    std::string output;
    output.reserve(input.size() + 8);

    constexpr char kHexDigits[] = "0123456789abcdef";

    for (const unsigned char character : input) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        default:
            if (character < 0x20) {
                output += "\\u00";
                output += kHexDigits[(character >> 4) & 0x0f];
                output += kHexDigits[character & 0x0f];
            } else {
                output += static_cast<char>(character);
            }
            break;
        }
    }

    return output;
}

const web::WorldRecord* FindWorld(const std::vector<web::WorldRecord>& worlds, int worldId)
{
    for (const web::WorldRecord& world : worlds) {
        if (world.id == worldId) {
            return &world;
        }
    }

    return nullptr;
}

const char* DifficultyKey(int difficulty)
{
    switch (std::clamp(difficulty, 0, 3)) {
    case 0:
        return "IDS_DIFFICULTY_TITLE_PEACEFUL";
    case 1:
        return "IDS_DIFFICULTY_TITLE_EASY";
    case 2:
        return "IDS_DIFFICULTY_TITLE_NORMAL";
    case 3:
        return "IDS_DIFFICULTY_TITLE_HARD";
    default:
        return "IDS_DIFFICULTY_TITLE_NORMAL";
    }
}

const char* GameModeKey(int gameModeId)
{
    switch (std::clamp(gameModeId, 0, 2)) {
    case 0:
        return "IDS_GAMEMODE_SURVIVAL";
    case 1:
        return "IDS_GAMEMODE_CREATIVE";
    case 2:
        return "IDS_GAMEMODE_ADVENTURE";
    default:
        return "IDS_GAMEMODE_SURVIVAL";
    }
}

const char* GameModeFallback(int gameModeId)
{
    switch (std::clamp(gameModeId, 0, 2)) {
    case 0:
        return "Survival";
    case 1:
        return "Creative";
    case 2:
        return "Adventure";
    default:
        return "Survival";
    }
}

} // namespace

namespace web {

WebApp::WebApp(BrowserPlatform& platform, WebStorage& storage, WebInput& input, WebRenderer& renderer)
    : platform_(platform)
    , storage_(storage)
    , input_(input)
    , renderer_(renderer)
{
}

bool WebApp::Initialize()
{
    legacyContent_.Load("/legacy-media/en-EN.lang", "/legacy-media/splashes.txt");
    mainMenuSplash_ = legacyContent_.PickMainMenuSplash();
    if (mainMenuSplash_.empty()) {
        mainMenuSplash_ = "Reticulating splines!";
    }

    rendererReady_ = renderer_.Initialize();
    inputReady_ = input_.Initialize();

    initialized_ = true;
    mode_ = Mode::Boot;
    status_ = "Mounting browser storage for the legacy menu flow...";
    if (!legacyContent_.IsLoaded()) {
        status_ += " Using fallback menu labels.";
    }
    if (!rendererReady_) {
        status_ += " WebGL panorama unavailable.";
    }
    if (!inputReady_) {
        status_ += " Keyboard and mouse hotkeys unavailable.";
    }

    lastFrameMs_ = platform_.NowMs();
    lastUiPublishMs_ = 0.0;
    lastPersistMs_ = lastFrameMs_;
    platform_.SetStatus(status_);
    PublishUiState();
    platform_.InitializePersistentStorage();
    return true;
}

void WebApp::OnStorageReady(bool success)
{
    storageReady_ = success;
    storage_.Load();

    status_ = storageReady_
        ? "Legacy main menu ready. Browser saves mounted."
        : "Legacy main menu ready. Browser saves are ephemeral in this session.";

    if (!rendererReady_) {
        status_ += " WebGL panorama unavailable.";
    }
    if (!legacyContent_.IsLoaded()) {
        status_ += " Using fallback legacy labels.";
    }

    mode_ = Mode::Title;
    modalReturnMode_ = Mode::Title;
    platform_.SetStatus(status_);
    PublishUiState();
}

void WebApp::Tick()
{
    if (!initialized_) {
        return;
    }

    input_.BeginFrame();

    const double nowMs = platform_.NowMs();
    const double deltaSeconds = std::clamp((nowMs - lastFrameMs_) / 1000.0, 0.0, 0.1);
    lastFrameMs_ = nowMs;
    worldTimeSeconds_ += deltaSeconds;

    UpdateHotkeys();

    if (currentWorldId_ >= 0 && mode_ == Mode::InGame) {
        UpdateSimulation(deltaSeconds);
    }

    if (rendererReady_) {
        renderer_.Resize(storage_.Options().renderScale);

        RenderState renderState;
        renderState.timeSeconds = worldTimeSeconds_;
        renderState.playerX = player_.playerX;
        renderState.playerY = player_.playerY;
        renderState.playerZ = player_.playerZ;
        renderState.yawRadians = static_cast<float>(player_.yaw);
        renderState.pitchRadians = static_cast<float>(player_.pitch);
        renderState.dimStrength = (mode_ == Mode::Paused || mode_ == Mode::Settings ||
                                   mode_ == Mode::Inventory || mode_ == Mode::Chat)
            ? 0.48f
            : 0.0f;
        renderer_.Render(renderState);
    }

    platform_.Tick(deltaSeconds);

    if (currentWorldId_ >= 0 && (nowMs - lastPersistMs_) > 2000.0) {
        SaveCurrentWorld(false);
        lastPersistMs_ = nowMs;
    }

    if ((nowMs - lastUiPublishMs_) > 250.0) {
        PublishUiState();
    }
}

void WebApp::RequestUiSync()
{
    PublishUiState();
}

void WebApp::ShowWorldSelection()
{
    if (mode_ == Mode::Boot) {
        return;
    }

    status_ = GetLegacyString("IDS_LOAD_SAVED_WORLD", "Load Saved World");
    SetMode(Mode::Worlds, false);
}

void WebApp::ShowCreateWorldMenu()
{
    if (mode_ == Mode::Boot) {
        return;
    }

    status_ = GetLegacyString("IDS_CREATE_NEW_WORLD", "Create New World");
    SetMode(Mode::CreateWorld, false);
}

void WebApp::ReturnToTitle()
{
    if (currentWorldId_ >= 0) {
        SaveCurrentWorld(true);
    }

    currentWorldId_ = -1;
    currentWorldName_.clear();
    player_ = WorldSnapshot {};
    status_ = "Legacy main menu ready.";
    SetMode(Mode::Title, false);
}

void WebApp::StartNewWorld(
    std::string_view name,
    std::string_view seed,
    int gameModeId,
    int difficulty,
    bool generateStructures,
    bool bonusChest)
{
    WorldRecord world;
    world.name = std::string(name);
    world.seed = std::string(seed);
    world.gameModeId = std::clamp(gameModeId, 0, 2);
    world.difficulty = std::clamp(difficulty, 0, 3);
    world.generateStructures = generateStructures;
    world.bonusChest = bonusChest;

    const WorldRecord createdWorld = storage_.CreateWorld(std::move(world));
    currentWorldId_ = createdWorld.id;
    currentWorldName_ = createdWorld.name;
    player_ = WorldSnapshot {};
    worldTimeSeconds_ = 0.0;
    status_ = "Created world \"" + createdWorld.name + "\".";
    SaveCurrentWorld(true);
    SetMode(Mode::InGame, true);
}

void WebApp::OpenWorldById(int worldId)
{
    if (!storage_.HasWorld(worldId)) {
        return;
    }

    currentWorldId_ = worldId;
    if (const WorldRecord* world = FindWorld(storage_.Worlds(), worldId)) {
        currentWorldName_ = world->name;
    }

    if (!storage_.LoadWorldSnapshot(worldId, player_)) {
        player_ = WorldSnapshot {};
    }

    worldTimeSeconds_ = static_cast<double>(player_.ticksPlayed) / 20.0;
    status_ = "Loaded world \"" + currentWorldName_ + "\".";
    SetMode(Mode::InGame, true);
}

void WebApp::DeleteWorldById(int worldId)
{
    if (storage_.DeleteWorld(worldId)) {
        status_ = "Deleted world #" + std::to_string(worldId) + '.';
        PublishUiState();
    }
}

void WebApp::PauseGame()
{
    if (currentWorldId_ < 0) {
        return;
    }

    status_ = GetLegacyString("IDS_PAUSE_GAME", "Pause Menu");
    SetMode(Mode::Paused, false);
}

void WebApp::ResumeGame()
{
    if (currentWorldId_ < 0) {
        return;
    }

    status_ = "Exploring \"" + currentWorldName_ + "\".";
    SetMode(Mode::InGame, true);
}

void WebApp::OpenSettings()
{
    if (mode_ == Mode::Settings) {
        return;
    }

    if (mode_ == Mode::Paused || mode_ == Mode::Inventory || mode_ == Mode::Chat) {
        modalReturnMode_ = Mode::Paused;
    } else if (mode_ == Mode::InGame) {
        modalReturnMode_ = Mode::InGame;
    } else if (mode_ == Mode::CreateWorld) {
        modalReturnMode_ = Mode::CreateWorld;
    } else if (mode_ == Mode::Worlds) {
        modalReturnMode_ = Mode::Worlds;
    } else {
        modalReturnMode_ = Mode::Title;
    }

    status_ = GetLegacyString("IDS_HELP_AND_OPTIONS", "Help & Options");
    SetMode(Mode::Settings, false);
}

void WebApp::CloseOverlay()
{
    switch (mode_) {
    case Mode::Settings:
        SetMode(modalReturnMode_, modalReturnMode_ == Mode::InGame);
        break;
    case Mode::Inventory:
    case Mode::Chat:
        ResumeGame();
        break;
    case Mode::CreateWorld:
        ShowWorldSelection();
        break;
    case Mode::Worlds:
        ReturnToTitle();
        break;
    default:
        break;
    }
}

void WebApp::ToggleInventory()
{
    if (currentWorldId_ < 0) {
        return;
    }

    if (mode_ == Mode::Inventory) {
        ResumeGame();
        return;
    }

    if (mode_ != Mode::InGame) {
        return;
    }

    status_ = GetLegacyString("IDS_INVENTORY", "Inventory");
    SetMode(Mode::Inventory, false);
}

void WebApp::ToggleChat()
{
    if (currentWorldId_ < 0) {
        return;
    }

    if (mode_ == Mode::Chat) {
        ResumeGame();
        return;
    }

    if (mode_ != Mode::InGame) {
        return;
    }

    status_ = "Chat is UI-only in this browser milestone.";
    SetMode(Mode::Chat, false);
}

void WebApp::RequestFullscreen()
{
    platform_.RequestFullscreen();
}

void WebApp::RequestPointerLock()
{
    if (mode_ == Mode::InGame) {
        platform_.RequestPointerLock();
        PublishUiState();
    }
}

void WebApp::UpdateSettingInt(std::string_view key, int value)
{
    WebOptions& options = storage_.MutableOptions();

    if (key == "showHud") {
        options.showHud = value != 0;
    } else if (key == "invertMouse") {
        options.invertMouse = value != 0;
    } else if (key == "renderScale") {
        options.renderScale = std::clamp(value, 50, 150);
    } else {
        return;
    }

    storage_.Save();
    if (storageReady_) {
        platform_.FlushPersistentStorage();
    }

    status_ = GetLegacyString("IDS_SETTINGS", "Settings");
    PublishUiState();
}

void WebApp::SetMode(Mode nextMode, bool requestPointerLock)
{
    mode_ = nextMode;

    if (requestPointerLock) {
        platform_.RequestPointerLock();
    } else {
        platform_.ReleasePointerLock();
    }

    PublishUiState();
}

void WebApp::UpdateHotkeys()
{
    if (input_.ConsumeFullscreenToggle()) {
        RequestFullscreen();
    }

    if (input_.ConsumeHudToggle()) {
        storage_.MutableOptions().showHud = !storage_.Options().showHud;
        storage_.Save();
        if (storageReady_) {
            platform_.FlushPersistentStorage();
        }
        status_ = storage_.Options().showHud ? "HUD enabled." : "HUD disabled.";
        PublishUiState();
    }

    if (input_.ConsumePauseToggle()) {
        if (mode_ == Mode::InGame) {
            PauseGame();
        } else if (mode_ == Mode::Paused) {
            ResumeGame();
        } else if (mode_ == Mode::Settings) {
            SetMode(modalReturnMode_, modalReturnMode_ == Mode::InGame);
        } else if (mode_ == Mode::Inventory || mode_ == Mode::Chat) {
            SetMode(Mode::Paused, false);
        } else if (mode_ == Mode::CreateWorld) {
            ShowWorldSelection();
        } else if (mode_ == Mode::Worlds) {
            ReturnToTitle();
        }
    }

    if (input_.ConsumeInventoryToggle()) {
        ToggleInventory();
    }

    if (input_.ConsumeChatToggle()) {
        ToggleChat();
    }
}

void WebApp::UpdateSimulation(double deltaSeconds)
{
    double lookX = 0.0;
    double lookY = 0.0;
    input_.ConsumeLookDelta(lookX, lookY);

    if (platform_.IsPointerLocked()) {
        player_.yaw += lookX * 0.0025;
        const double pitchSign = storage_.Options().invertMouse ? 1.0 : -1.0;
        player_.pitch = std::clamp(player_.pitch + (lookY * 0.0025 * pitchSign), -1.2, 1.2);
    }

    double moveX = 0.0;
    double moveZ = 0.0;

    if (input_.MoveForward()) {
        moveZ -= 1.0;
    }
    if (input_.MoveBackward()) {
        moveZ += 1.0;
    }
    if (input_.StrafeLeft()) {
        moveX -= 1.0;
    }
    if (input_.StrafeRight()) {
        moveX += 1.0;
    }

    const double magnitude = std::sqrt((moveX * moveX) + (moveZ * moveZ));
    if (magnitude > 0.0) {
        moveX /= magnitude;
        moveZ /= magnitude;
    }

    const double walkSpeed = 6.0;
    const double sinYaw = std::sin(player_.yaw);
    const double cosYaw = std::cos(player_.yaw);

    player_.playerX += (moveX * cosYaw - moveZ * sinYaw) * walkSpeed * deltaSeconds;
    player_.playerZ += (moveZ * cosYaw + moveX * sinYaw) * walkSpeed * deltaSeconds;

    if (input_.MoveUp()) {
        player_.playerY += walkSpeed * 0.45 * deltaSeconds;
    }
    if (input_.MoveDown()) {
        player_.playerY -= walkSpeed * 0.45 * deltaSeconds;
    }

    player_.playerY = std::clamp(player_.playerY, 1.3, 8.0);
    player_.ticksPlayed = static_cast<int>(worldTimeSeconds_ * 20.0);
    storage_.TouchWorld(currentWorldId_, player_.ticksPlayed);

    if (!platform_.IsPointerLocked()) {
        status_ = "Click the canvas or use Capture Mouse to look around.";
    } else {
        status_ = "Exploring \"" + currentWorldName_ + "\".";
    }
}

void WebApp::SaveCurrentWorld(bool flushPersistentStorage)
{
    if (currentWorldId_ < 0) {
        return;
    }

    storage_.TouchWorld(currentWorldId_, player_.ticksPlayed);
    storage_.SaveWorldSnapshot(currentWorldId_, player_);
    storage_.Save();

    if (flushPersistentStorage && storageReady_) {
        platform_.FlushPersistentStorage();
    }
}

void WebApp::PublishUiState()
{
    std::ostringstream json;
    json << std::boolalpha;
    json << '{';
    json << "\"mode\":\"" << ModeName(mode_) << "\",";
    json << "\"status\":\"" << JsonEscape(status_) << "\",";
    json << "\"storageReady\":" << storageReady_ << ',';
    json << "\"rendererReady\":" << rendererReady_ << ',';
    json << "\"currentWorldId\":" << currentWorldId_ << ',';
    json << "\"currentWorldName\":\"" << JsonEscape(currentWorldName_) << "\",";
    json << "\"pointerLocked\":" << platform_.IsPointerLocked() << ',';
    json << "\"supportsMultiplayer\":" << platform_.SupportsMultiplayer() << ',';
    json << "\"supportsExistingSaveImport\":" << platform_.SupportsExistingSaveImport() << ',';
    json << "\"options\":{";
    json << "\"showHud\":" << storage_.Options().showHud << ',';
    json << "\"invertMouse\":" << storage_.Options().invertMouse << ',';
    json << "\"renderScale\":" << storage_.Options().renderScale;
    json << "},";
    json << "\"player\":{";
    json << "\"x\":" << std::fixed << std::setprecision(2) << player_.playerX << ',';
    json << "\"y\":" << std::fixed << std::setprecision(2) << player_.playerY << ',';
    json << "\"z\":" << std::fixed << std::setprecision(2) << player_.playerZ << ',';
    json << "\"yaw\":" << std::fixed << std::setprecision(2) << player_.yaw << ',';
    json << "\"pitch\":" << std::fixed << std::setprecision(2) << player_.pitch << ',';
    json << "\"ticksPlayed\":" << player_.ticksPlayed;
    json << "},";
    json << "\"legacy\":{";
    json << "\"loaded\":" << legacyContent_.IsLoaded() << ',';
    json << "\"title\":\"" << JsonEscape("Minecraft") << "\",";
    json << "\"edition\":\"" << JsonEscape("Legacy Console Edition") << "\",";
    json << "\"tagline\":\"" << JsonEscape("Browser port milestone sourced from the legacy clone repo") << "\",";
    json << "\"splash\":\"" << JsonEscape(mainMenuSplash_) << "\",";
    json << "\"playGame\":\"" << JsonEscape(GetLegacyString("IDS_PLAY_GAME", "Play Game")) << "\",";
    json << "\"leaderboards\":\"" << JsonEscape(GetLegacyString("IDS_LEADERBOARDS", "Leaderboards")) << "\",";
    json << "\"achievements\":\"" << JsonEscape(GetLegacyString("IDS_ACHIEVEMENTS", "Achievements")) << "\",";
    json << "\"helpAndOptions\":\"" << JsonEscape(GetLegacyString("IDS_HELP_AND_OPTIONS", "Help & Options")) << "\",";
    json << "\"downloadableContent\":\"" << JsonEscape(GetLegacyString("IDS_DOWNLOADABLECONTENT", "Downloadable Content")) << "\",";
    json << "\"exitGame\":\"" << JsonEscape(GetLegacyString("IDS_EXIT_GAME", "Exit Game")) << "\",";
    json << "\"loadSavedWorld\":\"" << JsonEscape(GetLegacyString("IDS_LOAD_SAVED_WORLD", "Load Saved World")) << "\",";
    json << "\"createNewWorld\":\"" << JsonEscape(GetLegacyString("IDS_CREATE_NEW_WORLD", "Create New World")) << "\",";
    json << "\"worldName\":\"" << JsonEscape(GetLegacyString("IDS_WORLD_NAME", "World Name")) << "\",";
    json << "\"defaultWorldName\":\"" << JsonEscape(GetLegacyString("IDS_DEFAULT_WORLD_NAME", "New World")) << "\",";
    json << "\"worldSeed\":\"" << JsonEscape(GetLegacyString("IDS_CREATE_NEW_WORLD_SEED", "Seed")) << "\",";
    json << "\"randomSeed\":\"" << JsonEscape(GetLegacyString("IDS_CREATE_NEW_WORLD_RANDOM_SEED", "Leave blank for a random seed")) << "\",";
    json << "\"moreOptions\":\"" << JsonEscape(GetLegacyString("IDS_MORE_OPTIONS", "More Options")) << "\",";
    json << "\"onlineGame\":\"" << JsonEscape(GetLegacyString("IDS_ONLINE_GAME", "Online Game")) << "\",";
    json << "\"generateStructures\":\"" << JsonEscape(GetLegacyString("IDS_GENERATE_STRUCTURES", "Generate Structures")) << "\",";
    json << "\"bonusChest\":\"" << JsonEscape(GetLegacyString("IDS_BONUS_CHEST", "Bonus Chest")) << "\",";
    json << "\"difficulty\":\"" << JsonEscape(GetLegacyString("IDS_SLIDER_DIFFICULTY", "Difficulty")) << "\",";
    json << "\"difficultyPeaceful\":\"" << JsonEscape(GetDifficultyLabel(0)) << "\",";
    json << "\"difficultyEasy\":\"" << JsonEscape(GetDifficultyLabel(1)) << "\",";
    json << "\"difficultyNormal\":\"" << JsonEscape(GetDifficultyLabel(2)) << "\",";
    json << "\"difficultyHard\":\"" << JsonEscape(GetDifficultyLabel(3)) << "\",";
    json << "\"gameModeSurvival\":\"" << JsonEscape(GetGameModeLabel(0)) << "\",";
    json << "\"gameModeCreative\":\"" << JsonEscape(GetGameModeLabel(1)) << "\",";
    json << "\"gameModeAdventure\":\"" << JsonEscape(GetGameModeLabel(2)) << "\",";
    json << "\"createdInSurvival\":\"" << JsonEscape(GetLegacyString("IDS_CREATED_IN_SURVIVAL", "Created in Survival")) << "\",";
    json << "\"createdInCreative\":\"" << JsonEscape(GetLegacyString("IDS_CREATED_IN_CREATIVE", "Created in Creative")) << "\",";
    json << "\"resumeGame\":\"" << JsonEscape(GetLegacyString("IDS_RESUME_GAME", "Resume Game")) << "\",";
    json << "\"load\":\"" << JsonEscape(GetLegacyString("IDS_LOAD", "Load")) << "\",";
    json << "\"settings\":\"" << JsonEscape(GetLegacyString("IDS_SETTINGS", "Settings")) << "\",";
    json << "\"inventory\":\"" << JsonEscape(GetLegacyString("IDS_INVENTORY", "Inventory")) << "\",";
    json << "\"chat\":\"" << JsonEscape("Chat") << "\",";
    json << "\"notSupported\":\"" << JsonEscape("This legacy menu item is not wired into the browser milestone yet.") << "\"";
    json << "},";
    json << "\"worlds\":[";

    for (std::size_t index = 0; index < storage_.Worlds().size(); ++index) {
        const WorldRecord& world = storage_.Worlds()[index];
        if (index > 0) {
            json << ',';
        }

        json << '{';
        json << "\"id\":" << world.id << ',';
        json << "\"name\":\"" << JsonEscape(world.name) << "\",";
        json << "\"lastPlayedUtc\":\"" << JsonEscape(world.lastPlayedUtc) << "\",";
        json << "\"ticksPlayed\":" << world.ticksPlayed << ',';
        json << "\"gameModeId\":" << std::clamp(world.gameModeId, 0, 2) << ',';
        json << "\"gameModeLabel\":\"" << JsonEscape(GetGameModeLabel(world.gameModeId)) << "\",";
        json << "\"createdModeLabel\":\"" << JsonEscape(GetCreatedModeLabel(world)) << "\",";
        json << "\"difficulty\":" << std::clamp(world.difficulty, 0, 3) << ',';
        json << "\"difficultyLabel\":\"" << JsonEscape(GetDifficultyLabel(world.difficulty)) << "\",";
        json << "\"generateStructures\":" << world.generateStructures << ',';
        json << "\"bonusChest\":" << world.bonusChest << ',';
        json << "\"seed\":\"" << JsonEscape(world.seed) << "\"";
        json << '}';
    }

    json << ']';
    json << '}';

    lastUiPublishMs_ = platform_.NowMs();
    platform_.SetStatus(status_);
    platform_.PublishUiState(json.str());
}

std::string WebApp::GetLegacyString(std::string_view key, std::string_view fallback) const
{
    return legacyContent_.GetString(key, fallback);
}

std::string WebApp::GetDifficultyLabel(int difficulty) const
{
    switch (std::clamp(difficulty, 0, 3)) {
    case 0:
        return GetLegacyString(DifficultyKey(0), "Peaceful");
    case 1:
        return GetLegacyString(DifficultyKey(1), "Easy");
    case 2:
        return GetLegacyString(DifficultyKey(2), "Normal");
    case 3:
        return GetLegacyString(DifficultyKey(3), "Hard");
    default:
        return GetLegacyString(DifficultyKey(2), "Normal");
    }
}

std::string WebApp::GetGameModeLabel(int gameModeId) const
{
    return GetLegacyString(GameModeKey(gameModeId), GameModeFallback(gameModeId));
}

std::string WebApp::GetCreatedModeLabel(const WorldRecord& world) const
{
    if (std::clamp(world.gameModeId, 0, 2) == 1) {
        return GetLegacyString("IDS_CREATED_IN_CREATIVE", "Created in Creative");
    }

    return GetLegacyString("IDS_CREATED_IN_SURVIVAL", "Created in Survival");
}

const char* WebApp::ModeName(Mode mode)
{
    switch (mode) {
    case Mode::Boot:
        return "boot";
    case Mode::Title:
        return "title";
    case Mode::Worlds:
        return "worlds";
    case Mode::CreateWorld:
        return "create_world";
    case Mode::InGame:
        return "in_game";
    case Mode::Paused:
        return "paused";
    case Mode::Settings:
        return "settings";
    case Mode::Inventory:
        return "inventory";
    case Mode::Chat:
        return "chat";
    default:
        return "boot";
    }
}

} // namespace web
