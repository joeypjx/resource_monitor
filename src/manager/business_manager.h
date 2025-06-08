#ifndef BUSINESS_MANAGER_H
#define BUSINESS_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

// 前向声明
class DatabaseManager;
class Scheduler;

/**
 * BusinessManager类 - 业务管理器
 * 
 * 负责业务的部署、停止、更新和状态管理
 */
class BusinessManager {
public:
    /**
     * 构造函数
     * 
     * @param db_manager 数据库管理器
     * @param scheduler 调度器
     */
    BusinessManager(std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<Scheduler> scheduler);
    
    /**
     * 析构函数
     */
    ~BusinessManager();
    
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
     * @return 部署结果
     */
    nlohmann::json deployBusiness(const nlohmann::json& business_info);
    
    /**
     * 停止业务
     * 
     * @param business_id 业务ID
     * @return 停止结果
     */
    nlohmann::json stopBusiness(const std::string& business_id);
    
    /**
     * 重启业务
     * 
     * @param business_id 业务ID
     * @return 重启结果
     */
    nlohmann::json restartBusiness(const std::string& business_id);
    
    /**
     * 更新业务
     * 
     * @param business_id 业务ID
     * @param business_info 业务信息
     * @return 更新结果
     */
    nlohmann::json updateBusiness(const std::string& business_id, const nlohmann::json& business_info);
    
    /**
     * 获取业务列表
     * 
     * @return 业务列表
     */
    nlohmann::json getBusinesses();
    
    /**
     * 获取业务详情
     * 
     * @param business_id 业务ID
     * @return 业务详情
     */
    nlohmann::json getBusinessDetails(const std::string& business_id);
    
    /**
     * 获取业务组件
     * 
     * @param business_id 业务ID
     * @return 业务组件
     */
    nlohmann::json getBusinessComponents(const std::string& business_id);

    /**
     * 根据业务模板ID部署业务
     * 
     * @param business_template_id 业务模板ID
     * @return 部署结果
     */
    nlohmann::json deployBusinessByTemplateId(const std::string& business_template_id);

    /**
     * 从模板中扩展组件
     * 
     * @param components 组件信息
     * @return 扩展后的组件信息
     */
    nlohmann::json expandComponentsFromTemplate(const nlohmann::json& components);

    /**
     * 删除业务
     * 
     * @param business_id 业务ID
     * @return 删除结果
     */
    nlohmann::json deleteBusiness(const std::string& business_id);

    /**
     * 部署业务组件
     * 
     * @param business_id 业务ID
     * @param component_id 组件ID
     * @return 部署结果
     */
    nlohmann::json deployComponent(const std::string& business_id, const std::string& component_id);

    /**
     * 停止业务组件
     * 
     * @param business_id 业务ID
     * @param component_id 组件ID
     * @return 停止结果
     */
    nlohmann::json stopComponent(const std::string& business_id, const std::string& component_id);

private:
    /**
     * 验证业务信息
     * 
     * @param business_info 业务信息
     * @return 是否有效
     */
    bool validateBusinessInfo(const nlohmann::json& business_info);
    
    /**
     * 验证组件信息
     * 
     * @param component_info 组件信息
     * @return 是否有效
     */
    bool validateComponentInfo(const nlohmann::json& component_info);
    
    /**
     * 部署业务组件
     * 
     * @param business_id 业务ID
     * @param component_info 组件信息
     * @param node_id 节点ID
     * @return 部署结果
     */
    nlohmann::json deployComponent(const std::string& business_id, 
                                 const nlohmann::json& component_info, 
                                 const std::string& node_id);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器
    std::shared_ptr<Scheduler> scheduler_;         // 调度器
    
    std::map<std::string, nlohmann::json> businesses_;  // 业务信息，key为业务ID
    std::mutex businesses_mutex_;                       // 业务信息互斥锁
};

#endif // BUSINESS_MANAGER_H
