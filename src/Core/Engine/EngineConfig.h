// EngineConfig.h — 引擎配置文件 (engine.json)
//
// 职责: 定义引擎配置结构体，支持从 JSON 文件加载窗口和渲染参数。
// 依赖: nlohmann/json
// 层级: 编排层 (Phase 2)

#pragma once

#include <string>

namespace Tumbler {

// ============================================================================
// EngineConfig — 引擎配置文件 (engine.json)
// ============================================================================
struct EngineConfig {
    // Window
    int WindowWidth = 1280;
    int WindowHeight = 720;
    std::string WindowTitle = "Tumbler";

    // Render / Assets
    std::string CookedPath = "cooked/";                 // cooked 资产根目录
    std::string AssetMapPath = "cooked/asset_map.json"; // asset_map 路径

    // 从 JSON 文件加载配置
    [[nodiscard]] bool LoadFromFile(const std::string& configPath);
};

} // namespace Tumbler
