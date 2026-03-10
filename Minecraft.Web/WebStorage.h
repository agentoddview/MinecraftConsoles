#pragma once

#include <string>
#include <vector>

namespace web {

struct WebOptions {
    bool showHud = true;
    bool invertMouse = false;
    int renderScale = 100;
};

struct WorldRecord {
    int id = 0;
    std::string name;
    std::string lastPlayedUtc;
    int ticksPlayed = 0;
};

struct WorldSnapshot {
    double playerX = 0.0;
    double playerY = 1.7;
    double playerZ = 6.0;
    double yaw = 3.14159265358979323846;
    double pitch = -0.18;
    int ticksPlayed = 0;
};

class WebStorage {
public:
    WebStorage(std::string savesRoot, std::string optionsRoot);

    bool Load();
    bool Save() const;

    const std::vector<WorldRecord>& Worlds() const;
    bool HasWorld(int worldId) const;

    const WebOptions& Options() const;
    WebOptions& MutableOptions();

    WorldRecord CreateWorld(std::string name);
    bool DeleteWorld(int worldId);
    void TouchWorld(int worldId, int ticksPlayed);

    bool LoadWorldSnapshot(int worldId, WorldSnapshot& outSnapshot) const;
    bool SaveWorldSnapshot(int worldId, const WorldSnapshot& snapshot);

private:
    bool LoadWorldManifest();
    bool LoadOptionsFile();
    bool SaveWorldManifest() const;
    bool SaveOptionsFile() const;

    std::string WorldManifestPath() const;
    std::string OptionsPath() const;
    std::string WorldStatePath(int worldId) const;

    static std::string SanitizeName(std::string name);
    static std::string CurrentUtcTimestamp();

    std::string savesRoot_;
    std::string optionsRoot_;
    std::vector<WorldRecord> worlds_;
    WebOptions options_;
    int nextWorldId_ = 1;
};

} // namespace web
