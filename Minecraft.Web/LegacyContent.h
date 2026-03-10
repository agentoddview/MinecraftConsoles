#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace web {

class LegacyContent {
public:
    bool Load(const std::string& stringsPath, const std::string& splashesPath);

    bool IsLoaded() const;
    std::string GetString(std::string_view key, std::string_view fallback = {}) const;
    std::string PickMainMenuSplash() const;

private:
    static std::string ReadTextFile(const std::string& path);
    static std::string Trim(std::string value);
    static std::string DecodeEntities(std::string_view value);

    bool LoadStrings(const std::string& path);
    bool LoadSplashes(const std::string& path);

    std::unordered_map<std::string, std::string> strings_;
    std::vector<std::string> splashes_;
};

} // namespace web
