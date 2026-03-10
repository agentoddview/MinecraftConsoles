#include "LegacyContent.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

namespace {

std::string ReplaceAll(std::string text, std::string_view from, std::string_view to)
{
    std::size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }

    return text;
}

} // namespace

namespace web {

bool LegacyContent::Load(const std::string& stringsPath, const std::string& splashesPath)
{
    strings_.clear();
    splashes_.clear();

    const bool stringsLoaded = LoadStrings(stringsPath);
    const bool splashesLoaded = LoadSplashes(splashesPath);
    return stringsLoaded || splashesLoaded;
}

bool LegacyContent::IsLoaded() const
{
    return !strings_.empty() || !splashes_.empty();
}

std::string LegacyContent::GetString(std::string_view key, std::string_view fallback) const
{
    const auto iterator = strings_.find(std::string(key));
    if (iterator != strings_.end() && !iterator->second.empty()) {
        return iterator->second;
    }

    return std::string(fallback);
}

std::string LegacyContent::PickMainMenuSplash() const
{
    if (splashes_.empty()) {
        return {};
    }

    std::time_t now = std::time(nullptr);
    std::tm localTime {};

#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    if (const std::tm* currentLocalTime = std::localtime(&now)) {
        localTime = *currentLocalTime;
    }
#endif

    if (localTime.tm_mon == 10 && localTime.tm_mday == 9 && splashes_.size() > 0) {
        return splashes_[0];
    }

    if (localTime.tm_mon == 5 && localTime.tm_mday == 1 && splashes_.size() > 1) {
        return splashes_[1];
    }

    if (localTime.tm_mon == 11 && localTime.tm_mday == 24 && splashes_.size() > 2) {
        return splashes_[2];
    }

    if (localTime.tm_mon == 0 && localTime.tm_mday == 1 && splashes_.size() > 3) {
        return splashes_[3];
    }

    const std::size_t randomStartIndex = splashes_.size() > 5 ? 5 : 0;
    std::mt19937 randomEngine(static_cast<std::mt19937::result_type>(now));
    std::uniform_int_distribution<std::size_t> distribution(randomStartIndex, splashes_.size() - 1);
    return splashes_[distribution(randomEngine)];
}

std::string LegacyContent::ReadTextFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    std::string text = stream.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    return text;
}

std::string LegacyContent::Trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }

    return value;
}

std::string LegacyContent::DecodeEntities(std::string_view value)
{
    std::string decoded(value);
    decoded = ReplaceAll(std::move(decoded), "&amp;", "&");
    decoded = ReplaceAll(std::move(decoded), "&quot;", "\"");
    decoded = ReplaceAll(std::move(decoded), "&apos;", "'");
    decoded = ReplaceAll(std::move(decoded), "&lt;", "<");
    decoded = ReplaceAll(std::move(decoded), "&gt;", ">");
    decoded = ReplaceAll(std::move(decoded), "&#xD;", "");
    decoded = ReplaceAll(std::move(decoded), "&#xA;", "\n");
    decoded = ReplaceAll(std::move(decoded), "&#13;", "");
    decoded = ReplaceAll(std::move(decoded), "&#10;", "\n");
    return decoded;
}

bool LegacyContent::LoadStrings(const std::string& path)
{
    const std::string content = ReadTextFile(path);
    if (content.empty()) {
        return false;
    }

    std::size_t searchPosition = 0;
    while (true) {
        const std::size_t dataPosition = content.find("<data", searchPosition);
        if (dataPosition == std::string::npos) {
            break;
        }

        const std::size_t nameStart = content.find("name=\"", dataPosition);
        if (nameStart == std::string::npos) {
            break;
        }

        const std::size_t keyStart = nameStart + 6;
        const std::size_t keyEnd = content.find('"', keyStart);
        const std::size_t valueOpen = content.find("<value>", keyEnd);
        const std::size_t valueStart = valueOpen == std::string::npos ? std::string::npos : valueOpen + 7;
        const std::size_t valueEnd = valueStart == std::string::npos ? std::string::npos : content.find("</value>", valueStart);

        if (keyEnd == std::string::npos || valueStart == std::string::npos || valueEnd == std::string::npos) {
            break;
        }

        const std::string key = content.substr(keyStart, keyEnd - keyStart);
        const std::string value = DecodeEntities(content.substr(valueStart, valueEnd - valueStart));
        strings_[key] = Trim(value);

        searchPosition = valueEnd + 8;
    }

    return !strings_.empty();
}

bool LegacyContent::LoadSplashes(const std::string& path)
{
    const std::string content = ReadTextFile(path);
    if (content.empty()) {
        return false;
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (!line.empty()) {
            splashes_.push_back(line);
        }
    }

    return !splashes_.empty();
}

} // namespace web
