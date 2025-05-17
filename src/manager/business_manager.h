#ifndef BUSINESS_MANAGER_H
#define BUSINESS_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include "scheduler.h"

// 前向声明
class DatabaseManager;

/**
 * BusinessManager类 - 业务管理器
 * 
 * 负责管理业务的部署、停止、更新和状态
 */
class BusinessManager {
public:
    /**
     * 构造函数
     * 
     * @param db_manager 数据库管理器
     */
    explicit BusinessManager(std::shared_ptr<DatabaseManager> db_manager);
    
    /**
     * 析构函数
     */
    ~BusinessManager() = default;
    
    /**
     * 初始化业务管理器
     * 
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * 部署业务
     * 
     * @param business_info 业务信息
     * @return 响应JSON
     */
    nlohmann::json deployBusiness(const nlohmann::json& business_info);
    
    /**
     * 停止业务
     * 
     * @param business_id 业务ID
     * @return 响应JSON
     */
    nlohmann::json stopBusiness(const std::string& business_id);
    
    /**
     * 重启业务
     * 
     * @param business_id 业务ID
     * @return 响应JSON
     */
    nlohmann::json restartBusiness(const std::string& business_id);
    
    /**
     * 更新业务
     * 
     * @param business_id 业务ID
     * @param business_info 更新后的业务信息
     * @return 响应JSON
     */
    nlohmann::json updateBusiness(const std::string& business_id, const nlohmann::json& business_info);
    
    /**
     * 获取业务列表
     * 
     * @return 业务列表JSON
     */
    nlohmann::json getBusinesses();
    
    /**
     * 获取业务详情
     * 
     * @param business_id 业务ID
     * @return 业务详情JSON
     */
    nlohmann::json getBusinessDetails(const std::string& business_id);
    
    /**
     * 获取业务组件状态
     * 
     * @param business_id 业务ID
     * @return 业务组件状态JSON
     */
    nlohmann::json getBusinessComponents(const std::string& business_id);
    
    /**
     * 更新组件状态
     * 
     * @param component_status 组件状态信息
     * @return 是否成功更新
     */
    bool updateComponentStatus(const nlohmann::json& component_status);

private:
    /**
     * 验证业务信息
     * 
     * @param business_info 业务信息
     * @return 是否有效
     */
    bool validateBusinessInfo(const nlohmann::json& business_info);
    
    /**
     * 调度业务组件
     * 
     * @param business_info 业务信息
     * @return 调度结果，包含每个组件的目标节点
     */
    nlohmann::json scheduleBusinessComponents(const nlohmann::json& business_info);
    
    /**
     * 部署组件到指定节点
     * 
     * @param component_info 组件信息
     * @param node_id 节点ID
     * @return 部署结果
     */
    nlohmann::json deployComponentToNode(const nlohmann::json& component_info, const std::string& node_id);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器
    std::unique_ptr<Scheduler> scheduler_;         // 资源调度器
    std::mutex mutex_;                             // 互斥锁
};

#endif // BUSINESS_MANAGER_H
