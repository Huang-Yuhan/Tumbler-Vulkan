#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Tumbler {

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Init() {
    s_Logger = spdlog::stdout_color_mt("Tumbler");
    s_Logger->set_level(spdlog::level::debug);
    s_Logger->set_pattern("%^[%L] %v%$");  // [I] message (colored)
}

void Log::Shutdown() {
    s_Logger.reset();
    spdlog::shutdown();
}

} // namespace Tumbler
