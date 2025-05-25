#ifndef NODE_CONTROLLER_H
#define NODE_CONTROLLER_H

#include <nlohmann/json.hpp>

class NodeController {
public:
    nlohmann::json shutdown();
    nlohmann::json reboot();
};

#endif // NODE_CONTROLLER_H 