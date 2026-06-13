// Log.cpp — spdlog 日志初始化
//
// 职责: 初始化全局 logger (控制台输出 + 日志级别配置)，
//       在程序启动时调用一次 Init()。
//
// 依赖: Log.h, spdlog
// 层级: 平台/工具层 (Phase 1)

#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Tumbler {

void LogInit() {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("Tumbler", std::move(sink));

#ifdef NDEBUG
    logger->set_level(spdlog::level::info);
#else
    logger->set_level(spdlog::level::trace);
#endif

    spdlog::set_default_logger(std::move(logger));
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
}

void LogShutdown() {
    spdlog::shutdown();
}

} // namespace Tumbler
