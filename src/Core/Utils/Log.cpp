#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>

// 移除了 static 成员变量定义，因为现在是实例成员

void Log::Init() {
    spdlog::set_pattern("%^[%T] [%n] [%l]: %v%$");
    CoreLogger = spdlog::stdout_color_mt("TUMBLER");
    CoreLogger->set_level(spdlog::level::trace);
}