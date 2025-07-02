#include "database_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <uuid/uuid.h>
#include <SQLiteCpp/SQLiteCpp.h>

// 生成UUID
std::string generate_template_uuid(const std::string& prefix) {
    uuid_t uuid = {0};
    char uuid_str[37] = {0};
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return prefix + "-" + std::string(uuid_str);
}

// 获取当前时间戳
std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream sstream;
    sstream << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return sstream.str();
}

// 创建组件模板表
bool DatabaseManager::createComponentTemplateTable() {
    try {
        db_->exec(
            "CREATE TABLE IF NOT EXISTS component_templates ("
            "component_template_id TEXT PRIMARY KEY,"
            "template_name TEXT NOT NULL,"
            "description TEXT,"
            "type TEXT NOT NULL,"
            "config TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ");"
        );
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        return false;
    }
}

// 创建业务模板表
bool DatabaseManager::createBusinessTemplateTable() {
    try {
        db_->exec(
            "CREATE TABLE IF NOT EXISTS business_templates ("
            "business_template_id TEXT PRIMARY KEY,"
            "template_name TEXT NOT NULL,"
            "description TEXT,"
            "components TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL"
            ");"
        );
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        return false;
    }
}

// 保存组件模板
nlohmann::json DatabaseManager::saveComponentTemplate(const nlohmann::json& template_info) {
    try {
        std::string template_id = template_info.contains("component_template_id") ? template_info["component_template_id"].get<std::string>() : generate_template_uuid("ct");
        std::string timestamp = get_current_timestamp();
        bool is_update = false;
        std::string created_at = timestamp;

        // 检查是否是更新操作
        if (template_info.contains("component_template_id")) {
            SQLite::Statement check(*db_, "SELECT created_at FROM component_templates WHERE component_template_id = ?");
            check.bind(1, template_id);
            if (check.executeStep()) {
                created_at = check.getColumn(0).getString();
                is_update = true;
            }
        }

        SQLite::Statement insert(*db_,
            "INSERT OR REPLACE INTO component_templates "
            "(component_template_id, template_name, description, type, config, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.bind(1, template_id);
        insert.bind(2, template_info["template_name"].get<std::string>());
        insert.bind(3, template_info.contains("description") ? template_info["description"].get<std::string>() : "");
        insert.bind(4, template_info["type"].get<std::string>());
        insert.bind(5, template_info["config"].dump());
        insert.bind(6, created_at);
        insert.bind(7, timestamp);
        insert.exec();
        return {{"status", "success"}, {"component_template_id", template_id}, {"message", is_update ? "Component template updated successfully" : "Component template created successfully"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 获取组件模板列表
nlohmann::json DatabaseManager::getComponentTemplates() {
    try {
        SQLite::Statement query(*db_, "SELECT component_template_id, template_name, description, type, config, created_at, updated_at FROM component_templates ORDER BY created_at DESC");
        nlohmann::json templates = nlohmann::json::array();
        while (query.executeStep()) {
            nlohmann::json template_info = nlohmann::json::object();
            template_info["component_template_id"] = query.getColumn(0).getString();
            template_info["template_name"] = query.getColumn(1).getString();
            template_info["description"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            template_info["type"] = query.getColumn(3).getString();
            template_info["config"] = nlohmann::json::parse(query.getColumn(4).getString());
            template_info["created_at"] = query.getColumn(5).getString();
            template_info["updated_at"] = query.getColumn(6).getString();
            templates.push_back(template_info);
        }
        return {{"status", "success"}, {"templates", templates}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 获取组件模板详情
nlohmann::json DatabaseManager::getComponentTemplate(const std::string& template_id) {
    try {
        SQLite::Statement query(*db_, "SELECT component_template_id, template_name, description, type, config, created_at, updated_at FROM component_templates WHERE component_template_id = ?");
        query.bind(1, template_id);
        if (query.executeStep()) {
            nlohmann::json template_info = nlohmann::json::object();
            template_info["component_template_id"] = query.getColumn(0).getString();
            template_info["template_name"] = query.getColumn(1).getString();
            template_info["description"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            template_info["type"] = query.getColumn(3).getString();
            template_info["config"] = nlohmann::json::parse(query.getColumn(4).getString());
            template_info["created_at"] = query.getColumn(5).getString();
            template_info["updated_at"] = query.getColumn(6).getString();
            return {{"status", "success"}, {"template", template_info}};
        }
        return {{"status", "error"}, {"message", "Component template not found"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 删除组件模板
nlohmann::json DatabaseManager::deleteComponentTemplate(const std::string& template_id) {
    try {
        // 检查是否有业务模板引用了该组件模板
        SQLite::Statement check(*db_, "SELECT business_template_id FROM business_templates WHERE components LIKE ?");
        std::string search_pattern = "%" + template_id + "%";
        check.bind(1, search_pattern);
        if (check.executeStep()) {
            std::string business_template_id = check.getColumn(0).getString();
            return {{"status", "error"}, {"message", "Cannot delete component template: it is referenced by business template " + business_template_id}};
        }
        SQLite::Statement del(*db_, "DELETE FROM component_templates WHERE component_template_id = ?");
        del.bind(1, template_id);
        del.exec();
        if (db_->getChanges() == 0) {
            return {{"status", "error"}, {"message", "Component template not found"}};
        }
        return {{"status", "success"}, {"message", "Component template deleted successfully"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 保存业务模板
nlohmann::json DatabaseManager::saveBusinessTemplate(const nlohmann::json& template_info) {
    try {
        std::string template_id = template_info.contains("business_template_id") ? template_info["business_template_id"].get<std::string>() : generate_template_uuid("bt");
        std::string timestamp = get_current_timestamp();
        bool is_update = false;
        std::string created_at = timestamp;
        // 验证组件模板是否存在
        if (template_info.contains("components") && template_info["components"].is_array()) {
            for (const auto& component : template_info["components"]) {
                if (component.is_null()) {
                    continue;
                }
                if (!component.contains("component_template_id")) {
                    return {{"status", "error"}, {"message", "Missing component_template_id in component"}};
                }
                std::string component_template_id = component["component_template_id"];
                auto result = getComponentTemplate(component_template_id);
                if (result["status"] != "success") {
                    return {{"status", "error"}, {"message", "Component template not found: " + component_template_id}};
                }
            }
        } else {
            return {{"status", "error"}, {"message", "Missing or invalid components array"}};
        }
        // 检查是否是更新操作
        if (template_info.contains("business_template_id")) {
            SQLite::Statement check(*db_, "SELECT created_at FROM business_templates WHERE business_template_id = ?");
            check.bind(1, template_id);
            if (check.executeStep()) {
                created_at = check.getColumn(0).getString();
                is_update = true;
            }
        }
        SQLite::Statement insert(*db_,
            "INSERT OR REPLACE INTO business_templates "
            "(business_template_id, template_name, description, components, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        insert.bind(1, template_id);
        insert.bind(2, template_info["template_name"].get<std::string>());
        insert.bind(3, template_info.contains("description") ? template_info["description"].get<std::string>() : "");
        insert.bind(4, template_info["components"].dump());
        insert.bind(5, created_at);
        insert.bind(6, timestamp);
        insert.exec();
        return {{"status", "success"}, {"business_template_id", template_id}, {"message", is_update ? "Business template updated successfully" : "Business template created successfully"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 获取业务模板列表
nlohmann::json DatabaseManager::getBusinessTemplates() {
    try {
        SQLite::Statement query(*db_, "SELECT business_template_id, template_name, description, components, created_at, updated_at FROM business_templates ORDER BY created_at DESC");
        nlohmann::json templates = nlohmann::json::array();
        while (query.executeStep()) {
            nlohmann::json template_info = nlohmann::json::object();
            template_info["business_template_id"] = query.getColumn(0).getString();
            template_info["template_name"] = query.getColumn(1).getString();
            template_info["description"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            auto components = nlohmann::json::parse(query.getColumn(3).getString());
            nlohmann::json components_with_details = nlohmann::json::array();
            for (const auto& component : components) {
                if (component.is_null()) {
                    continue;
                }
                nlohmann::json component_with_details = component;
                if (component.contains("component_template_id")) {
                    std::string component_template_id = component["component_template_id"];
                    auto template_result = getComponentTemplate(component_template_id);
                    if (template_result["status"] == "success") {
                        component_with_details["template_details"] = template_result["template"];
                    }
                }
                components_with_details.push_back(component_with_details);
            }
            template_info["components"] = components_with_details;
            template_info["created_at"] = query.getColumn(4).getString();
            template_info["updated_at"] = query.getColumn(5).getString();
            templates.push_back(template_info);
        }
        return {{"status", "success"}, {"templates", templates}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 获取业务模板详情
nlohmann::json DatabaseManager::getBusinessTemplate(const std::string& template_id) {
    try {
        SQLite::Statement query(*db_, "SELECT business_template_id, template_name, description, components, created_at, updated_at FROM business_templates WHERE business_template_id = ?");
        query.bind(1, template_id);
        if (query.executeStep()) {
            nlohmann::json template_info = nlohmann::json::object();
            template_info["business_template_id"] = query.getColumn(0).getString();
            template_info["template_name"] = query.getColumn(1).getString();
            template_info["description"] = query.getColumn(2).isNull() ? "" : query.getColumn(2).getString();
            auto components = nlohmann::json::parse(query.getColumn(3).getString());
            nlohmann::json components_with_details = nlohmann::json::array();
            for (const auto& component : components) {
                if (component.is_null()) {
                    continue;
                }
                nlohmann::json component_with_details = component;
                if (component.contains("component_template_id")) {
                    std::string component_template_id = component["component_template_id"];
                    auto template_result = getComponentTemplate(component_template_id);
                    if (template_result["status"] == "success") {
                        component_with_details["template_details"] = template_result["template"];
                    }
                }
                components_with_details.push_back(component_with_details);
            }
            template_info["components"] = components_with_details;
            template_info["created_at"] = query.getColumn(4).getString();
            template_info["updated_at"] = query.getColumn(5).getString();
            return {{"status", "success"}, {"template", template_info}};
        }
        return {{"status", "error"}, {"message", "Business template not found"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}

// 删除业务模板
nlohmann::json DatabaseManager::deleteBusinessTemplate(const std::string& template_id) {
    try {
        SQLite::Statement del(*db_, "DELETE FROM business_templates WHERE business_template_id = ?");
        del.bind(1, template_id);
        del.exec();
        if (db_->getChanges() == 0) {
            return {{"status", "error"}, {"message", "Business template not found"}};
        }
        return {{"status", "success"}, {"message", "Business template deleted successfully"}};
    } catch (const std::exception& e) {
        return {{"status", "error"}, {"message", e.what()}};
    }
}
