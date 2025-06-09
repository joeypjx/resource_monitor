#include "business_manager.h"
#include "database_manager.h"
#include "scheduler.h"
#include <iostream>
#include <chrono>
#include <uuid/uuid.h>
#include <httplib.h>
#include <random>

// 生成UUID
std::string generate_uuid()
{
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

BusinessManager::BusinessManager(std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<Scheduler> scheduler)
    : db_manager_(db_manager), scheduler_(scheduler)
{
}

BusinessManager::~BusinessManager()
{
}

bool BusinessManager::initialize()
{
    std::cout << "Initializing BusinessManager..." << std::endl;
    return true;
}

nlohmann::json BusinessManager::deployBusinessByTemplateId(const std::string &business_template_id)
{
    // 1. 获取业务模板
    auto template_result = db_manager_->getBusinessTemplate(business_template_id);
    if (!template_result.contains("status") || template_result["status"] != "success")
    {
        return {
            {"status", "error"},
            {"message", "Business template not found or error: " + (template_result.contains("message") ? template_result["message"].get<std::string>() : "")}};
    }
    const auto &template_info = template_result["template"];
    // 2. 组装业务部署信息
    nlohmann::json business_info;
    business_info["business_name"] = template_info.contains("template_name") ? template_info["template_name"].get<std::string>() : "业务实例";
    business_info["components"] = nlohmann::json::array();
    if (template_info.contains("components") && template_info["components"].is_array())
    {
        for (const auto &comp : template_info["components"])
        {
            nlohmann::json comp_info;
            if (comp.contains("component_template_id"))
            {
                comp_info["component_template_id"] = comp["component_template_id"];
                business_info["components"].push_back(comp_info);
            }
        }
    }
    // 用模板详细信息扩展components
    business_info["components"] = expandComponentsFromTemplate(business_info["components"]);
    // 3. 调用现有deployBusiness方法
    return deployBusiness(business_info);
}

// 部署业务
nlohmann::json BusinessManager::deployBusiness(const nlohmann::json &business_info)
{
    std::cout << "Deploying business: " << business_info.dump(4) << std::endl;

    // 验证业务信息
    if (!validateBusinessInfo(business_info))
    {
        return {
            {"status", "error"},
            {"message", "Invalid business information"}};
    }

    // 生成业务ID（如果没有提供）
    std::string business_id;
    if (business_info.contains("business_id"))
    {
        business_id = business_info["business_id"];
    }
    else
    {
        business_id = generate_uuid();
    }

    // 保存业务信息（此时组件可以为空或只包含基本信息）
    // 新建一个业务对象
    nlohmann::json business = business_info;
    business["business_id"] = business_id;
    business["business_name"] = business_info["business_name"];
    business["status"] = "running";
    business["created_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    business["updated_at"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    db_manager_->saveBusiness(business);

    // 获取组件列表
    if (!business_info.contains("components") || !business_info["components"].is_array())
    {
        return {
            {"status", "error"},
            {"message", "Missing components"}};
    }

    auto components = business_info["components"];
    // 修改组件的id
    for (auto &component : components)
    {
        component["component_id"] = generate_uuid();
    }

    // 调度组件
    auto schedule_result = scheduler_->scheduleComponents(business_id, components);
    if (schedule_result["status"] != "success")
    {
        return {
            {"status", "error"},
            {"message", "Failed to schedule components"}};
    }

    // 部署组件
    bool has_error = false;
    for (const auto &schedule : schedule_result["component_schedules"])
    {
        std::string component_id = schedule["component_id"];
        std::string node_id = schedule["node_id"];

        nlohmann::json component_info;
        for (const auto &component : components)
        {
            if (component["component_id"] == component_id)
            {
                component_info = component;
                break;
            }
        }
        auto deploy_result = deployComponent(business_id, component_info, node_id);
        if (deploy_result["status"] != "success")
        {
            has_error = true;
        }

        // 保存组件信息
        nlohmann::json component = component_info;
        component["node_id"] = node_id;
        component["business_id"] = business_id;
        component["status"] = deploy_result["status"];
        db_manager_->saveBusinessComponent(component);
    }

    // 只更新业务状态
    db_manager_->updateBusinessStatus(business_id, has_error ? "error" : "running");

    if (has_error)
    {
        return {
            {"status", "error"},
            {"message", "One or more components failed to deploy"}};
    }

    return {
        {"status", "success"},
        {"message", "Business deployed successfully"},
        {"business_id", business_id}};
}

// 停止业务
nlohmann::json BusinessManager::stopBusiness(const std::string &business_id)
{
    std::cout << "Stopping business: " << business_id << std::endl;

    // 获取业务信息
    auto business = db_manager_->getBusinessDetails(business_id);

    // 停止所有组件
    for (const auto &component_item : business["components"])
    {
        std::string component_id = component_item["component_id"];
        std::string node_id = component_item["node_id"];

        // 停止组件
        auto stop_result = stopComponent(business_id, component_id);

        if (stop_result["status"] != "success")
        {
            std::cerr << "Failed to stop component: " << component_id << std::endl;
        }
    }

    // 更新业务状态
    db_manager_->updateBusinessStatus(business_id, "stopped");

    return {
        {"status", "success"},
        {"message", "Business stopped successfully"}};
}

// 重启业务
nlohmann::json BusinessManager::restartBusiness(const std::string &business_id)
{
    std::cout << "Restarting business: " << business_id << std::endl;
    // 先停止业务
    stopBusiness(business_id);

    // 等待业务停止
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 获取业务详情
    auto business = db_manager_->getBusinessDetails(business_id);
    if (!business.contains("components"))
    {
        return {{"status", "error"}, {"message", "No components found"}};
    }

    bool has_error = false;
    // 遍历组件，重新部署
    for (const auto &component : business["components"])
    {
        auto result = deployComponent(business_id, component, component["node_id"]);
        if (result["status"] != "success")
        {
            has_error = true;
        }

        // 更新组件信息
        nlohmann::json component_info = component;
        component_info["node_id"] = component["node_id"];
        component_info["business_id"] = business_id;
        component_info["status"] = result["status"];
        db_manager_->updateComponentStatus(component_info);
    }

    // 更新业务状态
    db_manager_->updateBusinessStatus(business_id, has_error ? "error" : "running");

    return {
        {"status", has_error ? "error" : "success"},
        {"message", has_error ? "One or more components failed to restart" : "Business restarted"},
    };
}

nlohmann::json BusinessManager::deleteBusiness(const std::string &business_id)
{
    stopBusiness(business_id);

    bool db_result = db_manager_->deleteBusiness(business_id);
    if (db_result)
    {
        return {
            {"status", "success"},
            {"message", "Business deleted successfully"},
            {"business_id", business_id}};
    }
    else
    {
        return {
            {"status", "error"},
            {"message", "Failed to delete business"},
            {"business_id", business_id}};
    }
}

// 获取所有业务
nlohmann::json BusinessManager::getBusinesses()
{
    // 从数据库获取所有业务
    auto businesses = db_manager_->getBusinesses();

    return {
        {"status", "success"},
        {"businesses", businesses}};
}

// 获取业务详情
nlohmann::json BusinessManager::getBusinessDetails(const std::string &business_id)
{
    auto business = db_manager_->getBusinessDetails(business_id);

    return {
        {"status", "success"},
        {"business", business}};
}

// 部署组件
nlohmann::json BusinessManager::deployComponent(const std::string &business_id, const std::string &component_id)
{
    // 获取组件信息
    nlohmann::json component = db_manager_->getComponentById(component_id);
    if (component.empty())
    {
        return {{"status", "error"}, {"message", "Component not found"}};
    }
    // 检查组件归属和节点信息
    std::string err_msg;
    if (!component.contains("business_id") || component["business_id"] != business_id)
    {
        err_msg = "Component does not belong to this business";
    }
    else if (!component.contains("node_id") || component["node_id"].get<std::string>().empty())
    {
        err_msg = "Component node_id is empty";
    }
    if (!err_msg.empty())
    {
        return {{"status", "error"}, {"message", err_msg}};
    }
    // 调用已有的部署方法
    return deployComponent(business_id, component, component["node_id"].get<std::string>());
}

// 部署组件
nlohmann::json BusinessManager::deployComponent(const std::string &business_id,
                                                const nlohmann::json &component_info,
                                                const std::string &node_id)
{
    // 获取节点信息
    nlohmann::json node_info = db_manager_->getNode(node_id);
    if (node_info.empty() || !node_info.contains("ip_address"))
    {
        return {{"status", "error"}, {"message", "Node not found or missing IP"}};
    }
    std::string node_url = "http://" + node_info["ip_address"].get<std::string>() + ":8081";
    std::string host = node_info["ip_address"].get<std::string>();
    int port = 8081;
    std::string path = "/api/deploy";

    // 构造部署请求
    nlohmann::json deploy_request = component_info;
    deploy_request["business_id"] = business_id;

    nlohmann::json response;
    try
    {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(5);

        httplib::Headers header_map = {{"Content-Type", "application/json"}};
        std::string json_data = deploy_request.dump();

        auto res = cli.Post(path, header_map, json_data, "application/json");
        if (res && res->status == 200)
        {
            try
            {
                response = nlohmann::json::parse(res->body);
            }
            catch (const std::exception &e)
            {
                response = {{"status", "error"}, {"message", "Invalid JSON response"}};
            }
        }
        else
        {
            std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
            response = {{"status", "error"}, {"message", error_msg}};
        }
    }
    catch (const std::exception &e)
    {
        response = {{"status", "error"}, {"message", std::string("Exception: ") + e.what()}};
    }

    return response;
}

nlohmann::json BusinessManager::stopComponent(const std::string &business_id, const std::string &component_id)
{
    // 获取组件信息
    nlohmann::json component = db_manager_->getComponentById(component_id);
    if (component.empty() || !component.contains("node_id"))
    {
        return {{"status", "error"}, {"message", "Component or node_id not found"}};
    }
    std::string node_id = component["node_id"].get<std::string>();
    nlohmann::json node_info = db_manager_->getNode(node_id);
    if (node_info.empty() || !node_info.contains("ip_address"))
    {
        return {{"status", "error"}, {"message", "Node not found or missing IP"}};
    }
    std::string host = node_info["ip_address"].get<std::string>();
    int port = 8081;
    std::string path = "/api/stop";

    // 构造停止请求
    nlohmann::json stop_request = {
        {"component_id", component_id},
        {"business_id", business_id}};
    if (component["type"] == "docker" && component.contains("container_id"))
    {
        stop_request["container_id"] = component["container_id"];
        stop_request["component_type"] = "docker";
    }
    else if (component["type"] == "binary" && component.contains("process_id"))
    {
        stop_request["process_id"] = component["process_id"];
        stop_request["component_type"] = "binary";
    }

    nlohmann::json response;
    try
    {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(5);

        httplib::Headers header_map = {{"Content-Type", "application/json"}};
        std::string json_data = stop_request.dump();

        auto res = cli.Post(path, header_map, json_data, "application/json");
        if (res && res->status == 200)
        {
            try
            {
                response = nlohmann::json::parse(res->body);
            }
            catch (const std::exception &e)
            {
                response = {{"status", "error"}, {"message", "Invalid JSON response"}};
            }
        }
        else
        {
            std::string error_msg = res ? "HTTP error: " + std::to_string(res->status) : "Connection error";
            response = {{"status", "error"}, {"message", error_msg}};
        }
    }
    catch (const std::exception &e)
    {
        response = {{"status", "error"}, {"message", std::string("Exception: ") + e.what()}};
    }

    return response;
}

nlohmann::json BusinessManager::expandComponentsFromTemplate(const nlohmann::json &components)
{
    nlohmann::json expanded = nlohmann::json::array();
    for (const auto &comp : components)
    {
        if (!comp.contains("component_template_id"))
            continue;
        std::string template_id = comp["component_template_id"];
        auto tpl_result = db_manager_->getComponentTemplate(template_id);
        if (!tpl_result.contains("status") || tpl_result["status"] != "success")
        {
            continue;
        }

        auto tpl = tpl_result["template"];
        nlohmann::json new_comp;
        new_comp["component_id"] = generate_uuid();
        new_comp["component_name"] = tpl["template_name"];
        new_comp["type"] = tpl["type"];

        if (tpl["config"].contains("image_name"))
        {
            new_comp["image_name"] = tpl["config"]["image_name"];
        }
        if (tpl["config"].contains("image_url"))
        {
            new_comp["image_url"] = tpl["config"]["image_url"];
        }
        if (tpl["config"].contains("environment_variables"))
        {
            new_comp["environment_variables"] = tpl["config"]["environment_variables"];
        }
        if (tpl["config"].contains("affinity"))
        {
            new_comp["affinity"] = tpl["config"]["affinity"];
        }
        if (tpl["config"].contains("binary_path"))
        {
            new_comp["binary_path"] = tpl["config"]["binary_path"];
        }
        if (tpl["config"].contains("binary_url"))
        {
            new_comp["binary_url"] = tpl["config"]["binary_url"];
        }
        // 你可以根据需要添加更多字段
        expanded.push_back(new_comp);
    }
    return expanded;
}

// 验证业务信息
bool BusinessManager::validateBusinessInfo(const nlohmann::json &business_info)
{
    // 检查必要字段
    if (!business_info.contains("business_name"))
    {
        return false;
    }

    // 检查组件
    if (!business_info.contains("components") || !business_info["components"].is_array())
    {
        return false;
    }

    // 验证每个组件
    for (const auto &component : business_info["components"])
    {
        if (!validateComponentInfo(component))
        {
            return false;
        }
    }

    return true;
}

bool BusinessManager::validateComponentInfo(const nlohmann::json &component_info)
{
    // 检查必要字段
    if (!component_info.contains("component_id") ||
        !component_info.contains("component_name") ||
        !component_info.contains("type"))
    {
        return false;
    }

    std::string type = component_info["type"];

    // 根据类型检查特定字段
    if (type == "docker")
    {
        // 检查Docker特定字段
        if ((!component_info.contains("image_url") || component_info["image_url"].get<std::string>().empty()) &&
            (!component_info.contains("image_name") || component_info["image_name"].get<std::string>().empty()))
        {
            return false;
        }
    }
    else if (type == "binary")
    {
        // 检查二进制特定字段
        if (!component_info.contains("binary_path") && !component_info.contains("binary_url"))
        {
            return false;
        }
    }
    else
    {
        // 不支持的类型
        return false;
    }

    return true;
}