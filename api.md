1 创建新组件
POST /api/templates/components
请求体：
{
    "template_name": "nginx-docker-template",
    "description": "Nginx docker组件模板",
    "type": "docker",
    "config": {
      "image_url": "nginx:latest",
      "image_name": "nginx:latest",
      "environment_variables": {"PORT": "80"},
      "affinity": {"require_gpu": false}
    }
}
响应体：
{
	"component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890",
	"message": "Component template created successfully",
	"status": "success"
}

2 编辑/更新组件
PUT /api/templates/components/{component_template_id}
请求体：
{
    "config": {
        "affinity": {},
        "binary_path": "/opt/bin/my-binary", 
        "binary_url": "http://example.com/bin/my-binary",
        "environment_variables": {
            "PORT": "8080"
        }
    },
    "description": "Nginx binary组件模板",
    "template_name": "nginx-binary-template",
    "type": "binary"
}
响应体：
{
	"component_template_id": "ct-9f9186b5-98ff-40e0-8f2f-a5659a510d93",
	"message": "Component template created successfully",
	"status": "success"
}

3 获取组件列表
GET /api/templates/components
响应体：
{
	"status": "success",
	"templates": [
		{
			"component_template_id": "ct-9f9186b5-98ff-40e0-8f2f-a5659a510d93",
			"config": {
				"affinity": {},
				"binary_path": "/opt/bin/my-binary",
				"binary_url": "http://example.com/bin/my-binary",
				"environment_variables": {
					"PORT": "8080"
				}
			},
			"created_at": "2025-06-04 14:08:19",
			"description": "Nginx binary组件模板",
			"template_name": "nginx-binary-template",
			"type": "binary",
			"updated_at": "2025-06-04 14:08:19"
		},
		{
			"component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890",
			"config": {
				"affinity": {
					"require_gpu": false
				},
				"environment_variables": {
					"PORT": "80"
				},
				"image_name": "nginx:latest",
				"image_url": "nginx:latest"
			},
			"created_at": "2025-06-04 14:04:34",
			"description": "Nginx docker组件模板",
			"template_name": "nginx-docker-template",
			"type": "docker",
			"updated_at": "2025-06-04 14:04:34"
		}
	]
}

4 获取组件详情
GET /api/templates/components/{component_template_id}
响应体：
{
	"status": "success",
	"template": {
		"component_template_id": "ct-9f9186b5-98ff-40e0-8f2f-a5659a510d93",
		"config": {
			"affinity": {},
			"binary_path": "/opt/bin/my-binary",
			"binary_url": "http://example.com/bin/my-binary",
			"config_files": [],
			"environment_variables": {
				"PORT": "8080"
			},
			"resource_requirements": {
				"cpu_cores": 1,
				"memory_mb": 128
			}
		},
		"created_at": "2025-06-04 14:08:19",
		"description": "Nginx binary组件模板",
		"template_name": "nginx-binary-template",
		"type": "binary",
		"updated_at": "2025-06-04 14:08:19"
	}
}
如果component_template_id未找到，则返回
{
	"message": "Component template not found",
	"status": "error"
}

5 删除组件
DELETE /api/templates/components/{component_template_id}
响应体：
{
	"message": "Component template deleted successfully",
	"status": "success"
}
或者
{
	"message": "Component template not found",
	"status": "error"
}




1 创建新业务模板
POST /api/templates/businesses
请求体：
{
  "template_name": "web-app-template",
  "description": "Web应用业务模板",
  "components": [
    {
      "component_template_id": "ct-9f9186b5-98ff-40e0-8f2f-a5659a510d93"
    },
    {
      "component_template_id": "ct-63b7b8f8-c161-45ab-b5be-395667cbad6d"
    }
  ]
}
响应体：
{
	"business_template_id": "bt-0baeca03-d015-4cee-af25-7676c094e51e",
	"message": "Business template created successfully",
	"status": "success"
}

2 编辑/更新业务模板
PUT /api/templates/businesses/{business_template_id}
请求体：
{
  "template_name": "web-app-template",
  "description": "Web应用业务模板",
  "components": [
    {
      "component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890"
    }
  ]
}
响应体：
{
	"business_template_id": "bt-0baeca03-d015-4cee-af25-7676c094e51e",
	"message": "Business template created successfully",
	"status": "success"
}

3 获取业务模板列表
GET /api/templates/businesses
响应体：
{
	"status": "success",
	"templates": [
		{
			"business_template_id": "bt-0baeca03-d015-4cee-af25-7676c094e51e",
			"components": [
				{
					"component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890"
				}
			],
			"created_at": "2025-06-04 14:56:28",
			"description": "Web应用业务模板",
			"template_name": "web-app-template",
			"updated_at": "2025-06-04 14:56:28"
		}
	]
}

4 获取业务模板详情
GET /api/templates/businesses/{business_template_id}
响应体：
{
	"status": "success",
	"template": {
		"business_template_id": "bt-0baeca03-d015-4cee-af25-7676c094e51e",
		"components": [
			{
				"component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890",
				"template_details": {
					"component_template_id": "ct-1f5e4862-fbbf-4c82-ae1c-b37fee1a1890",
					"config": {
						"affinity": {
							"require_gpu": false
						},
						"environment_variables": {
							"PORT": "80"
						},
						"image_name": "nginx:latest",
						"image_url": "nginx:latest"
					},
					"created_at": "2025-06-04 14:04:34",
					"description": "Nginx docker组件模板",
					"template_name": "nginx-docker-template",
					"type": "docker",
					"updated_at": "2025-06-04 14:04:34"
				}
			}
		],
		"created_at": "2025-06-04 14:56:28",
		"description": "Web应用业务模板",
		"template_name": "web-app-template",
		"updated_at": "2025-06-04 14:56:28"
	}
}
如果business_template_id未找到，则返回
{
	"message": "Business template not found",
	"status": "error"
}

5 删除业务模板
DELETE /api/templates/businesses/{business_template_id}
响应体：
{
	"message": "Business template deleted successfully",
	"status": "success"
}

1 获取节点列表
GET /api/nodes
响应体：
{
	"nodes": [
		{
			"cpu_model": "Intel Core i7",
			"created_at": 1749055176,
			"gpu_count": 0,
			"hostname": "d621f85f514f",
			"ip_address": "172.17.0.2",
			"node_id": "node-0a00ae17-45ec-4dd1-9155-dc76cce95ff6",
			"os_info": "Linux 6.10.14-linuxkit #1 SMP Tue Apr 15 16:00:54 UTC 2025 aarch64",
			"status": "online",
			"updated_at": 1749055311
		}
	],
	"status": "success"
}

2 获取节点详情
GET /api/nodes/{node_id}
响应体：
{
	"node": {
		"cpu_model": "Intel Core i7",
		"created_at": 1749055176,
		"gpu_count": 0,
		"hostname": "d621f85f514f",
		"ip_address": "172.17.0.2",
		"latest_cpu": {
			"core_count": 10,
			"load_avg_15m": 0.23,
			"load_avg_1m": 0.44,
			"load_avg_5m": 0.31,
			"timestamp": 1749055186,
			"usage_percent": 0.23923444976076125
		},
		"latest_memory": {
			"free": 6679232512,
			"timestamp": 1749055186,
			"total": 8218034176,
			"usage_percent": 9.721754264946002,
			"used": 798937088
		},
		"node_id": "node-0a00ae17-45ec-4dd1-9155-dc76cce95ff6",
		"os_info": "Linux 6.10.14-linuxkit #1 SMP Tue Apr 15 16:00:54 UTC 2025 aarch64",
		"status": "online",
		"updated_at": 1749055188
	},
	"status": "success"
}

1