#include "EngineConfig.h"

#include "Core/Utils/Log.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Tumbler {

bool EngineConfig::LoadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file: {}", configPath);
        return false;
    }

    try {
        json j = json::parse(file);

        if (j.contains("window") && j["window"].is_object()) {
            const auto& w = j["window"];
            WindowWidth = w.value("width", 1280);
            WindowHeight = w.value("height", 720);
            WindowTitle = w.value("title", "Tumbler");
        }

        if (j.contains("render") && j["render"].is_object()) {
            const auto& r = j["render"];
            CookedPath = r.value("cookedPath", "cooked/");
            AssetMapPath = r.value("assetMap", "cooked/asset_map.json");
        }

        LOG_INFO("Loaded: {}x{} '{}'", WindowWidth, WindowHeight, WindowTitle);
        return true;
    } catch (const json::exception& e) {
        LOG_ERROR("JSON parse error: {}", e.what());
        return false;
    }
}

} // namespace Tumbler
