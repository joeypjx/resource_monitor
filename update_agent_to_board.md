# Agent 到 Board 重构修改清单

## 已完成的修改：

### 1. database_manager.cpp
- [x] 表名：agents -> board
- [x] 字段：agent_id -> board_id  
- [x] 方法：saveAgent -> saveBoard
- [x] 方法：updateAgentLastSeen -> updateBoardLastSeen
- [x] 方法：updateAgentStatus -> updateBoardStatus
- [x] 方法：getAgents -> getBoards
- [x] getNode方法中的查询表和字段

### 2. database_manager.h
- [x] 方法声明更新

### 3. database_manager_metric.cpp
- [x] 表的外键引用：agents(agent_id) -> board(board_id)
- [x] 索引名称：agent_id -> board_id
- [x] saveCpuMetrics方法签名

## 待修改的内容：

### 4. database_manager_metric.cpp (继续)
- [ ] saveCpuMetrics方法实现中的字段名和绑定
- [ ] saveMemoryMetrics方法签名和实现  
- [ ] saveDiskMetrics方法签名和实现
- [ ] saveNetworkMetrics方法签名和实现
- [ ] saveDockerMetrics方法签名和实现
- [ ] 所有get*Metrics方法的签名和实现
- [ ] saveResourceUsage方法中的字段检查
- [ ] getClusterMetrics和getClusterMetricsHistory方法

### 5. scheduler.cpp
- [ ] getAvailableNodes中的getAgents -> getBoards
- [ ] selectBestNodeForComponent中的agent_id -> board_id
- [ ] checkNodeAffinity中的agent_id匹配

### 6. http_server.cpp  
- [ ] API路径：/api/agents -> /api/boards
- [ ] 方法调用：getAgents -> getBoards
- [ ] agent_id生成和处理 -> board_id

### 7. business_manager.cpp
- [ ] getAgents调用 -> getBoards

### 8. agent_control_manager.cpp
- [ ] 方法参数：agent_id -> board_id

### 9. database_manager_business.cpp
- [ ] 外键引用：agents(agent_id) -> board(board_id)

### 10. 其他文件
- [ ] 测试文件和文档更新

## 注意事项：
- agent端的代码可能需要相应调整，将agent_id改为board_id
- API文档需要更新
- 数据库迁移脚本可能需要考虑现有数据 