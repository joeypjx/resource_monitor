#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include "spdlog/spdlog.h"

class Logger {
public:
    /**
     * 默认构造函数
     */
    Logger() = default;

    static void initialize(const std::string& logger_name = "resource_monitor",
                           const std::string& file_path = "resource_monitor.log",
                           spdlog::level::level_enum level = spdlog::level::info);

    static std::shared_ptr<spdlog::logger>& getLogger();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

// A macro for easy access
#define LOG_INFO(...)    Logger::getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)    Logger::getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)   Logger::getLogger()->error(__VA_ARGS__)
#define LOG_DEBUG(...)   Logger::getLogger()->debug(__VA_ARGS__)

#endif // LOGGER_H 