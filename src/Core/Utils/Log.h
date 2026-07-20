#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace Tumbler {

class Log {
public:
    static void Init();
    static void Shutdown();

    static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
    static std::shared_ptr<spdlog::logger> s_Logger;
};

} // namespace Tumbler

#define LOG_TRACE(...) ::Tumbler::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)  ::Tumbler::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::Tumbler::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Tumbler::Log::GetLogger()->error(__VA_ARGS__)
