#ifndef BUSINESS_MANAGER_H
#define BUSINESS_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

// 前向声明 (Forward declarations)
class DatabaseManager;
class Scheduler;

/**
 * @class BusinessManager
 * @brief 业务逻辑管理器 (Business Logic Manager)
 *
 * 负责处理系统中与"业务"相关的所有核心逻辑。
 * "业务"可以理解为一个或多个"组件"的集合，这些组件协同工作以完成特定任务。
 * BusinessManager协调DatabaseManager和Scheduler来管理业务的整个生命周期。
 * - 使用DatabaseManager持久化业务、组件和模板的配置信息。
 * - 使用Scheduler来执行异步任务，如部署和停止操作。
 */
class BusinessManager {
public:
    /**
     * @brief 默认构造函数
     */
    BusinessManager() = default;
    
    /**
     * @brief 构造函数
     * 
     * @param db_manager 指向数据库管理器的共享指针
     * @param scheduler 指向调度器的共享指针
     * @param sftp_host SFTP服务器地址，用于组件的二进制包分发
     */
    BusinessManager(std::shared_ptr<DatabaseManager> db_manager, std::shared_ptr<Scheduler> scheduler, const std::string& sftp_host);
    
    /**
     * @brief 析构函数
     */
    ~BusinessManager();
    
    /**
     * @brief 初始化业务管理器
     * @return bool 如果成功初始化返回true
     */
    bool initialize();

    // --- Business Lifecycle Management ---

    /**
     * @brief 根据完整的业务定义来部署一个新业务
     * 
     * @param business_info 包含业务名称、组件列表等信息的JSON对象
     * @return nlohmann::json 包含部署结果（如business_id）的JSON对象
     */
    nlohmann::json deployBusiness(const nlohmann::json& business_info);
    
    /**
     * @brief 停止一个正在运行的业务
     * 这会停止该业务下的所有组件。
     * @param business_id 要停止的业务的唯一ID
     * @param permanently 如果为true，将从节点上彻底删除组件文件
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json stopBusiness(const std::string& business_id, bool permanently = false);
    
    /**
     * @brief 重启一个业务
     * 先停止业务，然后再启动。
     * @param business_id 要重启的业务的唯一ID
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json restartBusiness(const std::string& business_id);
    
    /**
     * @brief 删除一个业务
     * 从数据库中彻底删除业务及其所有关联记录。通常在调用stopBusiness之后执行。
     * @param business_id 要删除的业务的唯一ID
     * @return nlohmann::json 包含操作结果的JSON对象
     */
    nlohmann::json deleteBusiness(const std::string& business_id);

    /**
     * @brief 根据业务模板ID来部署业务
     * 
     * @param business_template_id 业务模板的唯一ID
     * @return nlohmann::json 包含部署结果的JSON对象
     */
    nlohmann::json deployBusinessByTemplateId(const std::string& business_template_id);

    // --- Business & Component Information ---

    /**
     * @brief 获取所有业务的列表
     * @return nlohmann::json 包含所有业务简要信息的JSON数组
     */
    nlohmann::json getBusinesses();
    
    /**
     * @brief 获取指定业务的详细信息
     * 包括其下的所有组件及其状态。
     * @param business_id 要查询的业务的唯一ID
     * @return nlohmann::json 包含业务详细信息的JSON对象
     */
    nlohmann::json getBusinessDetails(const std::string& business_id);

    // --- Component Management ---

    /**
     * @brief 部署业务中的单个组件
     * 
     * @param business_id 组件所属业务的ID
     * @param component_id 要部署的组件的ID
     * @return nlohmann::json 包含部署结果的JSON对象
     */
    nlohmann::json deployComponent(const std::string& business_id, const std::string& component_id);

    /**
     * @brief 停止业务中的单个组件
     * 
     * @param business_id 组件所属业务的ID
     * @param component_id 要停止的组件的ID
     * @param permanently 如果为true，将从节点上彻底删除组件文件
     * @return nlohmann::json 包含停止结果的JSON对象
     */
    nlohmann::json stopComponent(const std::string& business_id, const std::string& component_id, bool permanently = false);

    // --- Helper Methods ---

    /**
     * @brief 从模板中扩展组件信息
     * 当使用模板部署时，此函数会用模板中的通用信息填充组件的具体配置。
     * @param components 从业务定义中传入的组件信息
     * @return nlohmann::json 包含完整组件信息的JSON对象
     */
    nlohmann::json expandComponentsFromTemplate(const nlohmann::json& components);

private:
    /**
     * @brief 验证业务信息的完整性和有效性
     * @param business_info 待验证的业务信息JSON对象
     * @return bool 如果信息有效则返回true
     */
    bool validateBusinessInfo(const nlohmann::json& business_info);
    
    /**
     * @brief 验证组件信息的完整性和有效性
     * @param component_info 待验证的组件信息JSON对象
     * @return bool 如果信息有效则返回true
     */
    bool validateComponentInfo(const nlohmann::json& component_info);
    
    /**
     * @brief 部署单个组件到指定节点（内部实现）
     * 
     * @param business_id 业务ID
     * @param component_info 组件的详细信息
     * @param node_id 要部署到的目标节点的ID
     * @return nlohmann::json 包含部署结果的JSON对象
     */
    nlohmann::json deployComponent(const std::string& business_id, 
                                 const nlohmann::json& component_info, 
                                 const std::string& node_id);

private:
    std::shared_ptr<DatabaseManager> db_manager_;  // 数据库管理器，用于数据持久化
    std::shared_ptr<Scheduler> scheduler_;         // 调度器，用于执行异步任务
    std::string sftp_host_;                       // SFTP服务器地址，用于获取组件包
};

#endif // BUSINESS_MANAGER_H
