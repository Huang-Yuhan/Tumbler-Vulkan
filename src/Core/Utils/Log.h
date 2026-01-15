#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <Core/Utils/DesignPattern/Singleton.h>

// 1. 继承 Singleton<Log>
class Log : public Singleton<Log> {
    // 允许基类调用 Log 的私有构造函数 (如果是私有的话)
    friend class Singleton<Log>;

public:
    // 初始化函数 (不再是 static)
    void Init();

    // Getter (不再是 static)
    std::shared_ptr<spdlog::logger>& GetCoreLogger() { return CoreLogger; }

private:
    // 私有构造 (防止外部 Log log;)
    Log() = default;

    std::shared_ptr<spdlog::logger> CoreLogger;
};

// ==========================================
// 宏定义更新
// ==========================================
// 注意：现在需要通过 Log::Get() 来访问实例
#define LOG_TRACE(...)    ::Log::Get().GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::Log::Get().GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::Log::Get().GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::Log::Get().GetCoreLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Log::Get().GetCoreLogger()->critical(__VA_ARGS__)