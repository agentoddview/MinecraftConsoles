#include "WebStorage.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::string Trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }

    return value;
}

bool SplitWorldLine(const std::string& line, int& id, std::string& name, std::string& lastPlayed, int& ticksPlayed)
{
    std::stringstream stream(line);
    std::string idToken;
    std::string ticksToken;

    if (!std::getline(stream, idToken, '|') ||
        !std::getline(stream, name, '|') ||
        !std::getline(stream, lastPlayed, '|') ||
        !std::getline(stream, ticksToken)) {
        return false;
    }

    try {
        id = std::stoi(idToken);
        ticksPlayed = std::stoi(ticksToken);
    } catch (...) {
        return false;
    }

    return true;
}

} // namespace

namespace web {

WebStorage::WebStorage(std::string savesRoot, std::string optionsRoot)
    : savesRoot_(std::move(savesRoot))
    , optionsRoot_(std::move(optionsRoot))
{
}

bool WebStorage::Load()
{
    worlds_.clear();
    options_ = {};
    nextWorldId_ = 1;

    return LoadWorldManifest() && LoadOptionsFile();
}

bool WebStorage::Save() const
{
    return SaveWorldManifest() && SaveOptionsFile();
}

const std::vector<WorldRecord>& WebStorage::Worlds() const
{
    return worlds_;
}

bool WebStorage::HasWorld(int worldId) const
{
    return std::any_of(worlds_.begin(), worlds_.end(), [worldId](const WorldRecord& world) {
        return world.id == worldId;
    });
}

const WebOptions& WebStorage::Options() const
{
    return options_;
}

WebOptions& WebStorage::MutableOptions()
{
    return options_;
}

WorldRecord WebStorage::CreateWorld(std::string name)
{
    WorldRecord world;
    world.id = nextWorldId_++;
    world.name = SanitizeName(std::move(name));
    world.lastPlayedUtc = CurrentUtcTimestamp();
    world.ticksPlayed = 0;

    worlds_.push_back(world);
    SaveWorldSnapshot(world.id, WorldSnapshot {});
    Save();
    return world;
}

bool WebStorage::DeleteWorld(int worldId)
{
    const auto newEnd = std::remove_if(worlds_.begin(), worlds_.end(), [worldId](const WorldRecord& world) {
        return world.id == worldId;
    });

    if (newEnd == worlds_.end()) {
        return false;
    }

    worlds_.erase(newEnd, worlds_.end());
    std::remove(WorldStatePath(worldId).c_str());
    Save();
    return true;
}

void WebStorage::TouchWorld(int worldId, int ticksPlayed)
{
    for (WorldRecord& world : worlds_) {
        if (world.id == worldId) {
            world.lastPlayedUtc = CurrentUtcTimestamp();
            world.ticksPlayed = ticksPlayed;
            return;
        }
    }
}

bool WebStorage::LoadWorldSnapshot(int worldId, WorldSnapshot& outSnapshot) const
{
    std::ifstream input(WorldStatePath(worldId));
    if (!input.is_open()) {
        return false;
    }

    WorldSnapshot snapshot;
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        try {
            if (key == "player_x") {
                snapshot.playerX = std::stod(value);
            } else if (key == "player_y") {
                snapshot.playerY = std::stod(value);
            } else if (key == "player_z") {
                snapshot.playerZ = std::stod(value);
            } else if (key == "yaw") {
                snapshot.yaw = std::stod(value);
            } else if (key == "pitch") {
                snapshot.pitch = std::stod(value);
            } else if (key == "ticks_played") {
                snapshot.ticksPlayed = std::stoi(value);
            }
        } catch (...) {
        }
    }

    outSnapshot = snapshot;
    return true;
}

bool WebStorage::SaveWorldSnapshot(int worldId, const WorldSnapshot& snapshot)
{
    std::ofstream output(WorldStatePath(worldId), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "player_x=" << snapshot.playerX << '\n';
    output << "player_y=" << snapshot.playerY << '\n';
    output << "player_z=" << snapshot.playerZ << '\n';
    output << "yaw=" << snapshot.yaw << '\n';
    output << "pitch=" << snapshot.pitch << '\n';
    output << "ticks_played=" << snapshot.ticksPlayed << '\n';
    return true;
}

bool WebStorage::LoadWorldManifest()
{
    std::ifstream input(WorldManifestPath());
    if (!input.is_open()) {
        return true;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        if (line.rfind("next_id=", 0) == 0) {
            try {
                nextWorldId_ = std::max(1, std::stoi(line.substr(8)));
            } catch (...) {
                nextWorldId_ = 1;
            }
            continue;
        }

        int id = 0;
        int ticksPlayed = 0;
        std::string name;
        std::string lastPlayedUtc;
        if (!SplitWorldLine(line, id, name, lastPlayedUtc, ticksPlayed)) {
            continue;
        }

        WorldRecord world;
        world.id = id;
        world.name = SanitizeName(name);
        world.lastPlayedUtc = lastPlayedUtc;
        world.ticksPlayed = ticksPlayed;
        worlds_.push_back(world);
        nextWorldId_ = std::max(nextWorldId_, id + 1);
    }

    return true;
}

bool WebStorage::LoadOptionsFile()
{
    std::ifstream input(OptionsPath());
    if (!input.is_open()) {
        return true;
    }

    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));

        if (key == "show_hud") {
            options_.showHud = value != "0";
        } else if (key == "invert_mouse") {
            options_.invertMouse = value == "1";
        } else if (key == "render_scale") {
            try {
                options_.renderScale = std::clamp(std::stoi(value), 50, 150);
            } catch (...) {
                options_.renderScale = 100;
            }
        }
    }

    return true;
}

bool WebStorage::SaveWorldManifest() const
{
    std::ofstream output(WorldManifestPath(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "next_id=" << nextWorldId_ << '\n';
    for (const WorldRecord& world : worlds_) {
        output << world.id << '|'
               << SanitizeName(world.name) << '|'
               << world.lastPlayedUtc << '|'
               << world.ticksPlayed << '\n';
    }

    return true;
}

bool WebStorage::SaveOptionsFile() const
{
    std::ofstream output(OptionsPath(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "show_hud=" << (options_.showHud ? 1 : 0) << '\n';
    output << "invert_mouse=" << (options_.invertMouse ? 1 : 0) << '\n';
    output << "render_scale=" << std::clamp(options_.renderScale, 50, 150) << '\n';
    return true;
}

std::string WebStorage::WorldManifestPath() const
{
    return savesRoot_ + "/worlds.txt";
}

std::string WebStorage::OptionsPath() const
{
    return optionsRoot_ + "/options.txt";
}

std::string WebStorage::WorldStatePath(int worldId) const
{
    return savesRoot_ + "/world_" + std::to_string(worldId) + ".state";
}

std::string WebStorage::SanitizeName(std::string name)
{
    if (name.empty()) {
        return "New World";
    }

    for (char& character : name) {
        if (character == '|' || character == '\n' || character == '\r' ||
            std::iscntrl(static_cast<unsigned char>(character)) != 0) {
            character = ' ';
        }
    }

    name = Trim(name);
    if (name.empty()) {
        return "New World";
    }

    if (name.size() > 40) {
        name.resize(40);
        name = Trim(name);
    }

    return name;
}

std::string WebStorage::CurrentUtcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm utcTime {};

#if defined(_WIN32)
    gmtime_s(&utcTime, &now);
#else
    if (const std::tm* currentUtc = std::gmtime(&now)) {
        utcTime = *currentUtc;
    }
#endif

    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &utcTime);
    return buffer;
}

} // namespace web
