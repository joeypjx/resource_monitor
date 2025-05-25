#include "node_controller.h"
#include <cstdlib>
#include <iostream>

nlohmann::json NodeController::shutdown() {
    // 模拟执行
    std::cout << "Shutting down node" << std::endl;
    int ret = 0;
    // int ret = std::system("shutdown -h now");
    if (ret == 0) {
        return {{"status", "success"}, {"message", "Node is shutting down"}};
    } else {
        return {{"status", "error"}, {"message", "Failed to shutdown node"}};
    }
}

nlohmann::json NodeController::reboot() {
    // 模拟执行
    std::cout << "Rebooting node" << std::endl;
    int ret = 0;
    // int ret = std::system("reboot");
    if (ret == 0) {
        return {{"status", "success"}, {"message", "Node is rebooting"}};
    } else {
        return {{"status", "error"}, {"message", "Failed to reboot node"}};
    }
} 