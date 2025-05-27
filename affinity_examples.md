# Affinity 字段使用示例

## 修改后的 affinity 字段支持以下几种匹配方式：

### 1. 向后兼容的 GPU 要求
```json
{
  "affinity": {
    "require_gpu": true
  }
}
```
或者
```json
{
  "affinity": {
    "gpu": true
  }
}
```

### 2. 按 IP 地址指定节点
```json
{
  "affinity": {
    "ip_address": "192.168.1.100"
  }
}
```
或者
```json
{
  "affinity": {
    "ip": "192.168.1.100"
  }
}
```

### 3. 按主机名指定节点
```json
{
  "affinity": {
    "hostname": "server01"
  }
}
```

### 4. 按节点ID指定节点
```json
{
  "affinity": {
    "agent_id": "agent-123456"
  }
}
```
或者
```json
{
  "affinity": {
    "node_id": "agent-123456"
  }
}
```

### 5. 按操作系统指定节点
```json
{
  "affinity": {
    "os_info": "Ubuntu"
  }
}
```
或者
```json
{
  "affinity": {
    "os": "Ubuntu 20.04"
  }
}
```

### 6. 按节点标签指定（如果支持标签系统）
```json
{
  "affinity": {
    "labels": {
      "zone": "beijing",
      "type": "gpu-node"
    }
  }
}
```

### 7. 组合多个条件（必须全部满足）
```json
{
  "affinity": {
    "require_gpu": true,
    "os_info": "Ubuntu",
    "ip_address": "192.168.1.100"
  }
}
```

### 8. 通用字段匹配
对于任何节点信息中存在的字段，都可以直接在 affinity 中指定：
```json
{
  "affinity": {
    "status": "online",
    "hostname": "gpu-server-01"
  }
}
```

## 实现特点：

1. **向后兼容**：现有的 `require_gpu` 和 `gpu` 字段继续支持
2. **灵活匹配**：支持节点信息中的任何字段进行精确匹配
3. **部分匹配**：对于 `os_info` 字段支持包含关系匹配
4. **错误提示**：不满足条件时会详细记录原因
5. **多条件AND**：所有 affinity 条件必须同时满足

## 使用场景：

- **特定节点部署**：将组件部署到指定IP或主机名的节点
- **GPU需求**：继续支持原有的GPU要求
- **操作系统兼容性**：根据OS版本选择合适的节点
- **地理位置要求**：通过IP或标签指定区域
- **高可用部署**：结合反亲和性实现分布式部署 