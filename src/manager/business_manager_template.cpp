#include "business_manager.h"
#include "database_manager.h"
#include "scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <uuid/uuid.h>

// 初始化数据库表
bool BusinessManager::initializeTables() {
    // 初始化业务和组件表
    bool success = true;
    
    // 初始化模板表
    success = success && db_manager_->createComponentTemplateTable();
    success = success && db_manager_->createBusinessTemplateTable();
    
    return success;
}
