// Log.h — spdlog 日志封装
//
// 职责: 提供统一的日志宏 (LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL)，
//       对 spdlog 做一层薄封装，隔离第三方日志库的 include。
//
// 依赖: spdlog (通过 vcpkg)
// 层级: 平台/工具层 (Phase 1)，无项目内部依赖，被所有模块使用

#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace Tumbler {

void LogInit();
void LogShutdown();

} // namespace Tumbler

#define LOG_TRACE(...)    ::spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...)     ::spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)     ::spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
