#include "utils/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <vector>

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::initialize(const std::string& logger_name, const std::string& file_path, spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, 1024 * 1024 * 10, 3); // 10MB file, 3 rotated files
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(console_sink);
    sinks.push_back(file_sink);

    logger_ = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
    logger_->set_level(level);
    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
}

std::shared_ptr<spdlog::logger>& Logger::getLogger() {
    if (!logger_) {
        // Fallback to a default logger if not initialized
        initialize();
    }
    return logger_;
} 