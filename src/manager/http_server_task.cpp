#include "http_server.h"
#include "database_manager.h"
#include "business_manager.h"
#include <nlohmann/json.hpp>
#include <uuid/uuid.h>

// 初始化任务组路由
void HTTPServer::initTaskGroupRoutes() {
    // 任务组模板API
    server_.Post("/api/task/task_group", [this](const httplib::Request &req, httplib::Response &res)
                 { handleTaskGroupTemplate(req, res); });

    // 任务组查询API
    server_.Post("/api/task/query", [this](const httplib::Request &req, httplib::Response &res)
                 { handleTaskGroupQuery(req, res); });

    // 任务组部署API
    server_.Post("/api/task/task_group/deploy", [this](const httplib::Request &req, httplib::Response &res)
                 { handleTaskGroupDeploy(req, res); });

    // 任务组状态查询API
    server_.Post("/api/task/task_group/query", [this](const httplib::Request &req, httplib::Response &res)
                 { handleTaskGroupStatus(req, res); });

    // 任务组删除API
    server_.Post("/api/task/task_group/stop", [this](const httplib::Request &req, httplib::Response &res)
                 { handleTaskGroupStop(req, res); });

    // 节点列表查询API
    server_.Get("/api/task/node", [this](const httplib::Request &req, httplib::Response &res)
                { handleTaskNodeList(req, res); });
}

void HTTPServer::handleTaskGroupTemplate(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求体
        auto json = nlohmann::json::parse(req.body);
        
        // 验证必要字段
        if (!json.contains("name") || !json.contains("task_groups") || 
            !json["task_groups"].is_array() || json["task_groups"].empty() ||
            !json["task_groups"][0].contains("tasks") || !json["task_groups"][0]["tasks"].is_array() ||
            json["task_groups"][0]["tasks"].empty()) {
            sendErrorResponse(res, "Invalid request format");
            return;
        }

        // 1. 创建所有组件模板
        nlohmann::json component_ids = nlohmann::json::array();
        for (const auto& task : json["task_groups"][0]["tasks"]) {
            nlohmann::json component_template;
            component_template["template_name"] = task["name"];
            component_template["type"] = "binary";
            // task["config"]["command"] [192.168.1.100:]/bin/agent or /bin/agent
            std::string ip_address = "";
            std::string binary_path = "";
            std::string command = task["config"]["command"].get<std::string>();
            if (command.find(":") != std::string::npos) {
                ip_address = command.substr(0, command.find(":"));
                binary_path = command.substr(command.find(":") + 1);
                component_template["config"] = {
                    {"affinity", {
                        {"ip_address", ip_address}
                    }},
                    {"binary_path", binary_path},
                    {"binary_url", ""}
                };
            } else {
                binary_path = command;
                component_template["config"] = {
                    {"binary_path", binary_path},
                    {"binary_url", ""}
                };
            }

            auto component_result = db_manager_->saveComponentTemplate(component_template);
            if (component_result["status"] != "success") {
                sendErrorResponse(res, "Failed to create component template: " + component_result["message"].get<std::string>());
                return;
            }
            component_ids.push_back({
                {"component_template_id", component_result["component_template_id"]}
            });
        }

        // 2. 创建业务模板
        nlohmann::json business_template;
        business_template["template_name"] = json["name"];
        business_template["components"] = component_ids;

        auto business_result = db_manager_->saveBusinessTemplate(business_template);
        if (business_result["status"] != "success") {
            sendErrorResponse(res, "Failed to create business template: " + business_result["message"].get<std::string>());
            return;
        }

        // 返回成功响应
        nlohmann::json response = {
            {"status", "success"}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleTaskGroupQuery(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求体
        auto json = nlohmann::json::parse(req.body);
        
        // 验证必要字段
        if (!json.contains("name")) {
            sendErrorResponse(res, "Missing business name");
            return;
        }

        // 获取所有业务模板
        auto templates_result = db_manager_->getBusinessTemplates();
        if (templates_result["status"] != "success") {
            sendErrorResponse(res, "Failed to get business templates");
            return;
        }

        // 查找匹配的业务模板
        for (const auto& template_info : templates_result["templates"]) {
            if (template_info["template_name"] == json["name"]) {
                // 构建响应
                nlohmann::json response = {
                    {"name", template_info["template_name"]},
                    {"task_groups", nlohmann::json::array()}
                };

                // 构建任务组
                nlohmann::json task_group = {
                    {"tasks", nlohmann::json::array()}
                };

                // 获取组件详情
                for (const auto& comp : template_info["components"]) {
                    if (comp.contains("template_details")) {
                        const auto& details = comp["template_details"];
                        std::string command = details["config"]["binary_path"].get<std::string>();
                        if (details["config"].contains("affinity") && details["config"]["affinity"].contains("ip_address")) {
                            std::string affinity = details["config"]["affinity"]["ip_address"].get<std::string>();
                            command = affinity + ":" + command;
                        }
                        nlohmann::json task = {
                            {"name", details["template_name"]},
                            {"config", {
                                {"command", command}
                            }}
                        };
                        task_group["tasks"].push_back(task);
                    }
                }

                response["task_groups"].push_back(task_group);
                res.set_content(response.dump(), "application/json");
                return;
            }
        }

        // 如果没有找到匹配的业务模板
        sendErrorResponse(res, "Business template not found");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleTaskGroupDeploy(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求体
        auto json = nlohmann::json::parse(req.body);
        
        // 验证必要字段
        if (!json.contains("name")) {
            sendErrorResponse(res, "Missing business name");
            return;
        }

        // 获取所有业务模板
        auto templates_result = db_manager_->getBusinessTemplates();
        if (templates_result["status"] != "success") {
            sendErrorResponse(res, "Failed to get business templates");
            return;
        }

        // 查找匹配的业务模板
        std::string template_id;
        for (const auto& template_info : templates_result["templates"]) {
            if (template_info["template_name"] == json["name"]) {
                template_id = template_info["business_template_id"];
                break;
            }
        }

        if (template_id.empty()) {
            sendErrorResponse(res, "Business template not found");
            return;
        }

        // 调用业务管理器进行部署
        auto deploy_result = business_manager_->deployBusinessByTemplateId(template_id);
        if (deploy_result["status"] != "success") {
            sendErrorResponse(res, "Failed to deploy business: " + deploy_result["message"].get<std::string>());
            return;
        }

        // 返回成功响应
        nlohmann::json response = {
            {"id", deploy_result["business_id"]}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleTaskGroupStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求体
        auto json = nlohmann::json::parse(req.body);
        
        // 验证必要字段
        if (!json.contains("id")) {
            sendErrorResponse(res, "Missing business ID");
            return;
        }

        // 获取业务详情
        bool is_running = true;
        auto business_result = db_manager_->getBusinessDetails(json["id"]);
        if (business_result["status"] != "running") {
            is_running = false;
        }

        // 构建响应
        nlohmann::json response = {
            {"id", json["id"]},
            {"status", is_running ? 0 : 1}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleTaskGroupStop(const httplib::Request& req, httplib::Response& res) {
    try {
        // 解析请求体
        auto json = nlohmann::json::parse(req.body);
        
        // 验证必要字段
        if (!json.contains("id")) {
            sendErrorResponse(res, "Missing business ID");
            return;
        }

        // 调用业务管理器删除业务
        auto delete_result = business_manager_->deleteBusiness(json["id"]);
        if (delete_result["status"] != "success") {
            sendErrorResponse(res, "Failed to delete business: " + delete_result["message"].get<std::string>());
            return;
        }

        // 返回成功响应
        nlohmann::json response = {
            {"id", json["id"]}
        };
        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

void HTTPServer::handleTaskNodeList(const httplib::Request& req, httplib::Response& res) {
    try {
        // 获取所有节点
        auto nodes_result = db_manager_->getNodes();

        // 构建响应
        nlohmann::json response = {
            {"nodes", nlohmann::json::array()}
        };

        // 处理每个节点
        for (const auto& node : nodes_result) {
            // 获取CPU和内存指标
            auto cpu_result = db_manager_->getCpuMetrics(node.at("node_id"));
            auto memory_result = db_manager_->getMemoryMetrics(node.at("node_id"));
            
            std::string latest_cpu = "0";
            std::string latest_memory = "0";

            if (!cpu_result.empty()) {
                latest_cpu = std::to_string(static_cast<int>(cpu_result[0].at("usage_percent")));
            }
            if (!memory_result.empty()) {
                latest_memory = std::to_string(static_cast<int>(memory_result[0].at("usage_percent")));
            }

            // 构建资源字符串
            std::string resources_str = "cpu_usage_percent:" + 
                latest_cpu +
                ",memory_usage_percent:" + 
                latest_memory;

            // 添加节点信息
            response["nodes"].push_back({
                {"ip", node.at("ip_address")},
                {"status", node.at("status") == "online" ? 0 : 1},
                {"resources", resources_str}
            });
        }

        res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
        sendExceptionResponse(res, e);
    }
}

