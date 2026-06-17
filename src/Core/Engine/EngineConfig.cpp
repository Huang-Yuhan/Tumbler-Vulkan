#include "EngineConfig.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Tumbler {

bool EngineConfig::LoadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[EngineConfig] Failed to open config file: " << configPath << std::endl;
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

        std::cout << "[EngineConfig] Loaded: " << WindowWidth << "x" << WindowHeight << " '" << WindowTitle << "'"
                  << std::endl;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[EngineConfig] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Tumbler
