#ifndef DIR_UTILS_H
#define DIR_UTILS_H

#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

inline bool create_directories(const std::string& path) {
    size_t pos = 0;
    do {
        pos = path.find_first_of('/', pos + 1);
        std::string subdir = path.substr(0, pos);
        if (subdir.empty()) continue;
        if ((mkdir(subdir.c_str(), 0755)) && (errno != EEXIST)) {
            return false;
        }
    } while (pos != std::string::npos);
    return true;
}

#endif // DIR_UTILS_H 