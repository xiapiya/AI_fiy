# Web Monitor Guidelines (V4.0)

> Web监控端开发规范 - 实时可视化云端处理链路
>
> **技术栈**: HTML/JS + WebSocket Secure (WSS) + Chart.js
> **部署**: Nginx静态文件托管
> **权限**: 只读权限,不下发控制指令

---

## Overview

**核心功能**:
1. **实时日志流**: WSS连接EMQX,订阅`emqx/system/logs`,实时展示云端处理日志
2. **抓拍画面展示**: 显示OV2640摄像头抓拍的历史图片
3. **全链路耗时分析**: 可视化图表展示从ESP32上传到MQTT下发的全链路延迟
4. **设备状态监控**: 实时显示ESP32设备在线状态、电池电量、WiFi信号等

**设计原则**:
- **只读权限**: Web端不下发任何控制指令,避免误操作
- **轻量化**: 纯静态HTML/JS,无需Node.js后端
- **实时性**: WebSocket长连接,低延迟更新
- **安全性**: 使用WSS (WebSocket Secure) 加密传输

---

## 项目结构

```
web-monitor/
├── index.html           # 主页面
├── css/
│   ├── main.css         # 主样式
│   └── chart.css        # 图表样式
├── js/
│   ├── mqtt-client.js   # WSS MQTT客户端
│   ├── log-viewer.js    # 日志实时展示
│   ├── image-gallery.js # 抓拍图片展示
│   └── metrics.js       # 耗时统计图表
├── lib/                 # 第三方库
│   ├── mqtt.min.js      # MQTT.js (支持WSS)
│   ├── chart.min.js     # Chart.js
│   └── dayjs.min.js     # 日期格式化
└── config.js            # 配置文件
```

---

## WebSocket 连接配置

### config.js

```javascript
// config.js
const CONFIG = {
    // WSS连接配置
    mqtt: {
        host: 'wss://your-domain.com/mqtt',  // Nginx代理的WSS端点
        port: 443,
        clientId: `web_monitor_${Math.random().toString(16).slice(2, 8)}`,
        username: 'web_monitor_user',  // EMQX认证
        password: 'your_password',
        keepalive: 60,
        reconnectPeriod: 5000,  // 5秒自动重连
    },

    // 订阅主题
    topics: {
        logs: 'emqx/system/logs',         // 云端日志流
        deviceStatus: 'emqx/esp32/+/status',  // 设备状态 (+为通配符)
        images: 'emqx/esp32/+/image_url',      // 抓拍图片URL
    },

    // 日志保留配置
    logRetention: {
        maxLines: 500,  // 最多保留500条日志
        autoScroll: true,
    },

    // 图表配置
    chart: {
        updateInterval: 5000,  // 5秒更新一次
        maxDataPoints: 50,     // 最多显示50个数据点
    },
};
```

---

## MQTT客户端 (WSS)

### mqtt-client.js

```javascript
// mqtt-client.js
class MQTTClient {
    constructor(config) {
        this.config = config;
        this.client = null;
        this.callbacks = {};
    }

    connect() {
        console.log('正在连接到EMQX (WSS)...');

        this.client = mqtt.connect(this.config.mqtt.host, {
            port: this.config.mqtt.port,
            clientId: this.config.mqtt.clientId,
            username: this.config.mqtt.username,
            password: this.config.mqtt.password,
            keepalive: this.config.mqtt.keepalive,
            reconnectPeriod: this.config.mqtt.reconnectPeriod,
            protocol: 'wss',  // 强制使用WSS
        });

        this.client.on('connect', () => {
            console.log('✅ 已连接到EMQX');
            this.updateConnectionStatus(true);

            // 订阅主题
            Object.values(this.config.topics).forEach(topic => {
                this.client.subscribe(topic, { qos: 1 }, (err) => {
                    if (!err) {
                        console.log(`✅ 已订阅主题: ${topic}`);
                    } else {
                        console.error(`❌ 订阅失败: ${topic}`, err);
                    }
                });
            });
        });

        this.client.on('message', (topic, message) => {
            this.handleMessage(topic, message);
        });

        this.client.on('error', (err) => {
            console.error('MQTT错误:', err);
            this.updateConnectionStatus(false);
        });

        this.client.on('offline', () => {
            console.warn('MQTT离线,正在重连...');
            this.updateConnectionStatus(false);
        });

        this.client.on('reconnect', () => {
            console.log('正在重新连接...');
        });
    }

    handleMessage(topic, message) {
        const payload = message.toString();

        // 根据主题分发消息
        if (topic === this.config.topics.logs) {
            this.trigger('log', JSON.parse(payload));
        } else if (topic.includes('/status')) {
            this.trigger('deviceStatus', JSON.parse(payload));
        } else if (topic.includes('/image_url')) {
            this.trigger('image', JSON.parse(payload));
        }
    }

    // 事件订阅
    on(event, callback) {
        if (!this.callbacks[event]) {
            this.callbacks[event] = [];
        }
        this.callbacks[event].push(callback);
    }

    // 触发事件
    trigger(event, data) {
        if (this.callbacks[event]) {
            this.callbacks[event].forEach(cb => cb(data));
        }
    }

    updateConnectionStatus(connected) {
        const statusEl = document.getElementById('connection-status');
        if (connected) {
            statusEl.textContent = '● 已连接';
            statusEl.className = 'status connected';
        } else {
            statusEl.textContent = '● 断开连接';
            statusEl.className = 'status disconnected';
        }
    }
}

// 全局实例
const mqttClient = new MQTTClient(CONFIG);
```

---

## 日志实时展示

### log-viewer.js

```javascript
// log-viewer.js
class LogViewer {
    constructor(mqttClient, config) {
        this.mqttClient = mqttClient;
        this.config = config;
        this.logs = [];
        this.logContainer = document.getElementById('log-container');
        this.autoScroll = config.logRetention.autoScroll;

        // 监听日志消息
        this.mqttClient.on('log', (logData) => {
            this.addLog(logData);
        });
    }

    addLog(logData) {
        // 添加日志到数组
        this.logs.push(logData);

        // 超过最大保留数,删除最早的日志
        if (this.logs.length > this.config.logRetention.maxLines) {
            this.logs.shift();
        }

        // 渲染日志行
        const logElement = this.createLogElement(logData);
        this.logContainer.appendChild(logElement);

        // 限制DOM元素数量
        if (this.logContainer.children.length > this.config.logRetention.maxLines) {
            this.logContainer.removeChild(this.logContainer.firstChild);
        }

        // 自动滚动到底部
        if (this.autoScroll) {
            this.logContainer.scrollTop = this.logContainer.scrollHeight;
        }
    }

    createLogElement(logData) {
        const div = document.createElement('div');
        div.className = `log-line log-${logData.level.toLowerCase()}`;

        const timestamp = dayjs(logData.timestamp).format('HH:mm:ss.SSS');
        const level = logData.level.padEnd(8);
        const module = (logData.module || '').padEnd(15);
        const message = logData.message;

        div.innerHTML = `
            <span class="log-timestamp">${timestamp}</span>
            <span class="log-level">${level}</span>
            <span class="log-module">${module}</span>
            <span class="log-message">${message}</span>
        `;

        // 点击展开详细上下文
        if (logData.context) {
            div.addEventListener('click', () => {
                alert(JSON.stringify(logData.context, null, 2));
            });
        }

        return div;
    }

    toggleAutoScroll() {
        this.autoScroll = !this.autoScroll;
        document.getElementById('auto-scroll-toggle').textContent =
            this.autoScroll ? '自动滚动: 开' : '自动滚动: 关';
    }

    clearLogs() {
        this.logs = [];
        this.logContainer.innerHTML = '';
    }
}
```

---

## 抓拍图片展示

### image-gallery.js

```javascript
// image-gallery.js
class ImageGallery {
    constructor(mqttClient) {
        this.mqttClient = mqttClient;
        this.images = [];
        this.galleryContainer = document.getElementById('image-gallery');

        this.mqttClient.on('image', (imageData) => {
            this.addImage(imageData);
        });
    }

    addImage(imageData) {
        this.images.unshift(imageData);  // 最新的在前

        // 只保留最近20张图片
        if (this.images.length > 20) {
            this.images.pop();
        }

        this.renderGallery();
    }

    renderGallery() {
        this.galleryContainer.innerHTML = '';

        this.images.forEach((img, index) => {
            const card = document.createElement('div');
            card.className = 'image-card';

            const timestamp = dayjs(img.timestamp).format('YYYY-MM-DD HH:mm:ss');

            card.innerHTML = `
                <img src="${img.url}" alt="抓拍 ${index + 1}" loading="lazy">
                <div class="image-meta">
                    <span>设备: ${img.device_id}</span>
                    <span>${timestamp}</span>
                </div>
            `;

            // 点击放大
            card.querySelector('img').addEventListener('click', () => {
                this.showFullImage(img.url);
            });

            this.galleryContainer.appendChild(card);
        });
    }

    showFullImage(url) {
        const modal = document.getElementById('image-modal');
        const modalImg = document.getElementById('modal-image');
        modalImg.src = url;
        modal.style.display = 'flex';
    }
}
```

---

## 全链路耗时图表

### metrics.js

```javascript
// metrics.js
class MetricsChart {
    constructor(mqttClient, config) {
        this.mqttClient = mqttClient;
        this.config = config;
        this.dataPoints = [];

        // 监听包含耗时数据的日志
        this.mqttClient.on('log', (logData) => {
            if (logData.context && logData.context.latency_ms) {
                this.addDataPoint({
                    timestamp: new Date(logData.timestamp),
                    latency: logData.context.latency_ms,
                    module: logData.module,
                });
            }
        });

        this.initChart();
        this.startAutoUpdate();
    }

    initChart() {
        const ctx = document.getElementById('latency-chart').getContext('2d');
        this.chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: '全链路延迟 (ms)',
                    data: [],
                    borderColor: 'rgb(75, 192, 192)',
                    tension: 0.1,
                }],
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: '延迟 (ms)',
                        },
                    },
                },
            },
        });
    }

    addDataPoint(point) {
        this.dataPoints.push(point);

        // 限制数据点数量
        if (this.dataPoints.length > this.config.chart.maxDataPoints) {
            this.dataPoints.shift();
        }
    }

    updateChart() {
        const labels = this.dataPoints.map(p => dayjs(p.timestamp).format('HH:mm:ss'));
        const data = this.dataPoints.map(p => p.latency);

        this.chart.data.labels = labels;
        this.chart.data.datasets[0].data = data;
        this.chart.update();
    }

    startAutoUpdate() {
        setInterval(() => {
            this.updateChart();
        }, this.config.chart.updateInterval);
    }
}
```

---

## 主页面

### index.html

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>情感陪伴智能体 - 云端监控</title>
    <link rel="stylesheet" href="css/main.css">
    <link rel="stylesheet" href="css/chart.css">
</head>
<body>
    <header>
        <h1>🤖 情感陪伴智能体 - 云端监控</h1>
        <div id="connection-status" class="status disconnected">● 断开连接</div>
    </header>

    <main>
        <!-- 实时日志 -->
        <section class="panel">
            <div class="panel-header">
                <h2>📋 实时日志流</h2>
                <div class="panel-controls">
                    <button id="auto-scroll-toggle">自动滚动: 开</button>
                    <button id="clear-logs">清空日志</button>
                </div>
            </div>
            <div id="log-container" class="log-container"></div>
        </section>

        <!-- 抓拍图片 -->
        <section class="panel">
            <div class="panel-header">
                <h2>📷 抓拍画面</h2>
            </div>
            <div id="image-gallery" class="image-gallery"></div>
        </section>

        <!-- 全链路延迟 -->
        <section class="panel">
            <div class="panel-header">
                <h2>📊 全链路延迟分析</h2>
            </div>
            <canvas id="latency-chart"></canvas>
        </section>
    </main>

    <!-- 图片放大模态框 -->
    <div id="image-modal" class="modal" onclick="this.style.display='none'">
        <img id="modal-image" src="" alt="放大查看">
    </div>

    <!-- 第三方库 -->
    <script src="lib/mqtt.min.js"></script>
    <script src="lib/chart.min.js"></script>
    <script src="lib/dayjs.min.js"></script>

    <!-- 项目脚本 -->
    <script src="config.js"></script>
    <script src="js/mqtt-client.js"></script>
    <script src="js/log-viewer.js"></script>
    <script src="js/image-gallery.js"></script>
    <script src="js/metrics.js"></script>

    <script>
        // 初始化
        window.onload = function() {
            mqttClient.connect();

            const logViewer = new LogViewer(mqttClient, CONFIG);
            const imageGallery = new ImageGallery(mqttClient);
            const metricsChart = new MetricsChart(mqttClient, CONFIG);

            // 绑定按钮事件
            document.getElementById('auto-scroll-toggle').onclick = () => {
                logViewer.toggleAutoScroll();
            };

            document.getElementById('clear-logs').onclick = () => {
                logViewer.clearLogs();
            };
        };
    </script>
</body>
</html>
```

---

## 样式参考

### main.css

```css
/* main.css */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Monaco', 'Courier New', monospace;
    background: #1e1e1e;
    color: #d4d4d4;
}

header {
    background: #252526;
    padding: 1rem 2rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 1px solid #3e3e42;
}

.status {
    padding: 0.5rem 1rem;
    border-radius: 4px;
    font-weight: bold;
}

.status.connected {
    background: #28a745;
    color: white;
}

.status.disconnected {
    background: #dc3545;
    color: white;
}

main {
    padding: 2rem;
}

.panel {
    background: #252526;
    border: 1px solid #3e3e42;
    border-radius: 8px;
    margin-bottom: 2rem;
    overflow: hidden;
}

.panel-header {
    padding: 1rem;
    background: #2d2d30;
    border-bottom: 1px solid #3e3e42;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.log-container {
    height: 400px;
    overflow-y: auto;
    padding: 1rem;
    font-size: 0.9rem;
    line-height: 1.6;
}

.log-line {
    padding: 0.25rem 0;
    border-left: 3px solid transparent;
}

.log-line:hover {
    background: #2a2d2e;
    cursor: pointer;
}

.log-info { border-color: #007acc; }
.log-warning { border-color: #ff9800; }
.log-error { border-color: #f44336; }

.log-timestamp { color: #858585; }
.log-level { font-weight: bold; }
.log-module { color: #569cd6; }

button {
    background: #0e639c;
    color: white;
    border: none;
    padding: 0.5rem 1rem;
    border-radius: 4px;
    cursor: pointer;
}

button:hover {
    background: #1177bb;
}

.image-gallery {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
    gap: 1rem;
    padding: 1rem;
}

.image-card img {
    width: 100%;
    border-radius: 4px;
    cursor: pointer;
}

.image-card img:hover {
    opacity: 0.8;
}

.modal {
    display: none;
    position: fixed;
    z-index: 1000;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.9);
    justify-content: center;
    align-items: center;
}

.modal img {
    max-width: 90%;
    max-height: 90%;
}
```

---

## 部署到Nginx

### 上传文件

```bash
# 上传到服务器
scp -r web-monitor/ root@your-server:/var/www/web-monitor
```

### Nginx配置 (已在cloud-deployment.md中定义)

访问: `https://monitor.your-domain.com`

---

## 核心原则

### ✅ DO

- 使用WSS (WebSocket Secure) 而非WS
- 限制日志和图片保留数量,防止内存泄漏
- 实现自动重连机制
- 使用CDN加速第三方库加载

### ❌ DON'T

- ❌ 不要下发任何MQTT控制指令 (只读权限)
- ❌ 不要在前端硬编码MQTT密码 (使用环境变量或用户输入)
- ❌ 不要忘记处理WebSocket断线重连
- ❌ 不要无限制地缓存日志和图片

---

**总结**: WSS实时通信 + 轻量化静态页面 + 只读权限 = 安全可靠的Web监控端
