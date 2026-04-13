# Cloud Deployment Guide (V4.0)

> 云端部署指南 - 1Panel + Docker容器化管理
>
> **适用场景**: Linux云服务器 + 公网部署

---

## Overview

**部署架构**: 使用**1Panel**面板统一管理所有容器化服务

**核心组件**:
1. **EMQX** - MQTTS Broker (TLS加密)
2. **NewAPI** - 大模型网关
3. **FastAPI** - 业务逻辑API
4. **MySQL** - 数据持久化
5. **Nginx** - SSL反向代理 + Web监控端托管

**优势**:
- 可视化管理,降低运维心智负担
- 统一日志查看和容器监控
- 一键备份和恢复
- 自动重启和健康检查

---

## 前置准备

### 1. 服务器要求

| 配置项 | 最低要求 | 推荐配置 |
|-------|---------|---------|
| CPU | 2核 | 4核 |
| 内存 | 4GB | 8GB |
| 磁盘 | 40GB | 100GB SSD |
| 带宽 | 5Mbps | 10Mbps+ |
| 操作系统 | Ubuntu 20.04+ / CentOS 7+ | Ubuntu 22.04 LTS |

### 2. 域名与DNS

- 注册域名 (如 `your-project.com`)
- 配置A记录指向服务器IP
- 准备SSL证书 (Let's Encrypt或购买)

### 3. 防火墙端口

**必须开放的端口**:

| 端口 | 用途 | 协议 |
|-----|------|-----|
| 80 | HTTP (自动重定向HTTPS) | TCP |
| 443 | HTTPS (FastAPI + Web监控) | TCP |
| 8883 | MQTTS (加密MQTT) | TCP |
| 18083 | EMQX Dashboard (可选) | TCP |

**内网端口** (不对外开放):
- 1883 - MQTT内网端口
- 3000 - NewAPI内网端口
- 3306 - MySQL内网端口

---

## 1Panel 安装

### 快速安装

```bash
# 官方安装脚本
curl -sSL https://resource.fit2cloud.com/1panel/package/quick_start.sh -o quick_start.sh && bash quick_start.sh

# 安装完成后访问
https://your-server-ip:your-port
```

**初始化**:
1. 设置管理员密码
2. 配置安全入口 (防爆破)
3. 绑定域名 (可选)

---

## 容器部署流程

### Step 1: 部署 MySQL

**1Panel操作**:
1. 进入"容器" → "创建容器"
2. 选择镜像: `mysql:8.0`
3. 配置:
   ```yaml
   容器名: mysql
   端口映射: 3306:3306 (仅内网)
   环境变量:
     MYSQL_ROOT_PASSWORD: your_strong_password
     MYSQL_DATABASE: newapi
   数据卷:
     /var/lib/mysql → /opt/1panel/apps/mysql/data
   重启策略: always
   ```

**验证**:
```bash
# 1Panel终端进入容器
docker exec -it mysql mysql -uroot -p
```

---

### Step 2: 部署 NewAPI (模型网关)

**拉取镜像**:
```bash
docker pull calciumion/new-api:latest
```

**1Panel操作**:
1. 创建容器
2. 配置:
   ```yaml
   容器名: newapi
   镜像: calciumion/new-api:latest
   端口映射: 3000:3000 (仅内网)
   环境变量:
     SESSION_SECRET: your_random_secret
     SQL_DSN: root:your_password@tcp(mysql:3306)/newapi
   网络: bridge
   重启策略: always
   ```

**初始化NewAPI**:
1. 访问 `http://localhost:3000` (通过SSH隧道或内网)
2. 登录默认账号: `root` / `123456`
3. 修改密码
4. 添加渠道:
   - 类型: 阿里云 DashScope
   - API Key: 你的DashScope Key
   - 模型: `qwen-vl-max`, `qwen-omni`
5. 创建令牌 (给FastAPI使用)

**测试NewAPI**:
```bash
curl http://localhost:3000/v1/chat/completions \
  -H "Authorization: Bearer YOUR_NEWAPI_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "qwen-omni",
    "messages": [{"role": "user", "content": "你好"}]
  }'
```

---

### Step 3: 部署 EMQX (MQTTS Broker)

**拉取镜像**:
```bash
docker pull emqx/emqx:5.5.0
```

**准备SSL证书**:
```bash
mkdir -p /opt/1panel/apps/emqx/certs
# 上传或生成证书
cp server.crt /opt/1panel/apps/emqx/certs/
cp server.key /opt/1panel/apps/emqx/certs/
cp ca.pem /opt/1panel/apps/emqx/certs/
```

**1Panel操作**:
```yaml
容器名: emqx
镜像: emqx/emqx:5.5.0
端口映射:
  1883:1883 (MQTT内网)
  8883:8883 (MQTTS公网)
  18083:18083 (Dashboard)
数据卷:
  /opt/emqx/data → /opt/1panel/apps/emqx/data
  /opt/emqx/etc/certs → /opt/1panel/apps/emqx/certs
环境变量:
  EMQX_LOADED_PLUGINS: "emqx_dashboard,emqx_auth_mnesia"
重启策略: always
```

**配置MQTTS** (进入EMQX Dashboard `http://your-ip:18083`):
1. 登录: `admin` / `public`
2. 进入"配置" → "监听器"
3. 添加MQTTS监听器:
   - 端口: 8883
   - 启用SSL/TLS
   - 证书路径:
     - CA: `/opt/emqx/etc/certs/ca.pem`
     - Cert: `/opt/emqx/etc/certs/server.crt`
     - Key: `/opt/emqx/etc/certs/server.key`
4. 配置认证 (用户名密码或Token)

**测试MQTTS**:
```bash
# 使用mosquitto客户端测试
mosquitto_pub -h your-domain.com -p 8883 \
  --cafile ca.pem \
  -u your-username -P your-password \
  -t "test/topic" -m "Hello MQTTS"
```

---

### Step 4: 部署 FastAPI

**构建镜像**:

`Dockerfile`:
```dockerfile
FROM python:3.11-slim

WORKDIR /app

# 安装依赖
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple

# 复制代码
COPY app/ ./app/

# 暴露端口
EXPOSE 8000

# 启动命令
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

**构建并推送**:
```bash
docker build -t your-registry/fastapi-backend:latest .
docker push your-registry/fastapi-backend:latest
```

**1Panel操作**:
```yaml
容器名: fastapi
镜像: your-registry/fastapi-backend:latest
端口映射: 8000:8000 (内网)
环境变量 (从.env文件):
  NEWAPI_BASE_URL: http://newapi:3000/v1
  NEWAPI_API_KEY: YOUR_NEWAPI_TOKEN
  MQTT_BROKER: emqx
  MQTT_PORT: 1883
  MQTT_USERNAME: your-mqtt-user
  MQTT_PASSWORD: your-mqtt-pass
网络: bridge
重启策略: always
```

**验证FastAPI**:
```bash
curl http://localhost:8000/health
# 应返回: {"status":"ok"}
```

---

### Step 5: 配置 Nginx (SSL反向代理)

**安装Nginx** (1Panel内置或手动安装):

**配置文件** (`/etc/nginx/sites-available/fastapi`):
```nginx
# HTTPS反向代理到FastAPI
server {
    listen 443 ssl http2;
    server_name api.your-domain.com;

    # SSL证书
    ssl_certificate /etc/nginx/certs/fullchain.pem;
    ssl_certificate_key /etc/nginx/certs/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    # 反向代理到FastAPI容器
    location /api/ {
        proxy_pass http://localhost:8000/api/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # 超时设置
        proxy_read_timeout 60s;
        proxy_connect_timeout 10s;
    }

    # 健康检查
    location /health {
        proxy_pass http://localhost:8000/health;
    }
}

# Web监控端静态托管
server {
    listen 443 ssl http2;
    server_name monitor.your-domain.com;

    ssl_certificate /etc/nginx/certs/fullchain.pem;
    ssl_certificate_key /etc/nginx/certs/privkey.pem;

    root /var/www/web-monitor;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }

    # WebSocket代理到EMQX
    location /mqtt {
        proxy_pass http://localhost:8083/mqtt;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host $host;
    }
}

# HTTP重定向到HTTPS
server {
    listen 80;
    server_name api.your-domain.com monitor.your-domain.com;
    return 301 https://$server_name$request_uri;
}
```

**启用配置**:
```bash
ln -s /etc/nginx/sites-available/fastapi /etc/nginx/sites-enabled/
nginx -t
systemctl reload nginx
```

---

## SSL证书管理

### Let's Encrypt (免费证书)

```bash
# 安装certbot
apt install certbot python3-certbot-nginx

# 自动获取证书
certbot --nginx -d api.your-domain.com -d monitor.your-domain.com

# 自动续期 (crontab)
0 3 * * * certbot renew --quiet
```

### 手动证书上传

1. 将证书文件上传到 `/etc/nginx/certs/`
2. 在Nginx配置中引用
3. 同样的证书复制到EMQX容器内 (用于MQTTS)

---

## 监控与运维

### 1Panel 容器监控

- **实时日志**: 1Panel → 容器 → 查看日志
- **资源监控**: CPU/内存使用率
- **自动重启**: 容器异常自动重启

### 日志收集

**FastAPI日志**:
```bash
docker logs -f fastapi --tail 100
```

**EMQX日志**:
```bash
docker logs -f emqx --tail 100
```

### 备份策略

**1Panel备份功能**:
1. 进入"备份" → "创建备份任务"
2. 备份内容:
   - MySQL数据卷
   - EMQX配置
   - Nginx配置
3. 备份周期: 每日3:00AM
4. 保留策略: 最近7天

---

## 安全加固

### 防火墙 (UFW)

```bash
# 仅开放必要端口
ufw allow 22/tcp   # SSH
ufw allow 80/tcp   # HTTP
ufw allow 443/tcp  # HTTPS
ufw allow 8883/tcp # MQTTS
ufw enable
```

### API密钥管理

- ❌ **禁止**: 在代码或日志中硬编码API Key
- ✅ **推荐**: 使用环境变量或1Panel secrets管理
- ✅ **推荐**: NewAPI统一管理,定期轮换

### MQTT认证

- 启用EMQX用户名密码认证
- 或使用JWT Token认证
- 定期审计连接日志

---

## 故障排查

### 常见问题

| 问题 | 排查步骤 | 解决方案 |
|-----|---------|---------|
| ESP32无法连接MQTTS | 1. 检查证书是否匹配<br>2. 检查端口8883是否开放 | 重新烧录证书,检查防火墙 |
| NewAPI调用失败 | 查看NewAPI日志 | 检查API Key配置,检查渠道状态 |
| FastAPI响应慢 | 查看日志中的耗时统计 | 优化NewAPI超时设置,增加并发 |
| 容器启动失败 | `docker logs <容器名>` | 检查环境变量,检查网络连接 |

### 紧急恢复

**从备份恢复**:
1. 1Panel → 备份 → 选择备份点
2. 一键恢复数据卷
3. 重启相关容器

---

## 核心原则

### ✅ DO

- 使用1Panel统一管理,避免手动docker命令
- 所有敏感配置通过环境变量注入
- 定期备份MySQL和配置文件
- 监控容器资源使用,及时扩容

### ❌ DON'T

- ❌ 不要在公网暴露MySQL端口3306
- ❌ 不要在日志中记录完整API Key
- ❌ 不要忘记配置SSL证书自动续期
- ❌ 不要跳过防火墙配置

---

**总结**: 1Panel容器化 + SSL加密 + NewAPI网关 = 安全可靠的云端架构
