#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>
#include <ctime>

inline std::string current_time() {
    std::ostringstream oss;
    std::time_t t = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#define LOG_INFO(fmt, ...)    do { printf("[%s] [INFO] " fmt "\n", current_time().c_str(), ##__VA_ARGS__); } while(0)
#define LOG_WARN(fmt, ...)    do { printf("[%s] [WARN] " fmt "\n", current_time().c_str(), ##__VA_ARGS__); } while(0)
#define LOG_ERROR(fmt, ...)   do { fprintf(stderr, "[%s] [ERROR] " fmt "\n", current_time().c_str(), ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(fmt, ...)   do { printf("[%s] [DEBUG] " fmt "\n", current_time().c_str(), ##__VA_ARGS__); } while(0)

#endif // LOGGER_H 