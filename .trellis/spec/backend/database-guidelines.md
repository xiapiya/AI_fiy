# Database Guidelines

> 数据库规范

---

## Overview

**重要**: 基于PRD,**后端服务不使用数据库**

所有数据存储:
- **Android端**: 使用Room数据库进行本地持久化(见 frontend/android-guidelines.md)
- **后端**: 无状态服务,仅负责API转发和大模型调用

---

## 为什么后端不用数据库?

根据PRD设计:

1. **轻量级服务**: 后端仅作为MQTT/HTTP网关,转发请求给大模型
2. **实时性优先**: 所有处理都是实时的,无需持久化
3. **历史记录存储**: Android App使用Room数据库本地存储聊天记录

---

## 如果未来需要数据库

如果后续需求变更(如多设备消息同步、用户管理),推荐方案:

### 选型建议

| 场景 | 推荐数据库 | 理由 |
|------|-----------|------|
| 结构化数据(用户、设备) | PostgreSQL | 支持JSON、全文搜索、可靠 |
| 简单KV缓存 | Redis | 快速、支持TTL |
| 消息队列 | EMQX(已有) + Redis | 复用现有MQTT |

### FastAPI + SQLAlchemy 示例

```python
# app/db.py (仅供参考,当前不使用)
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession
from sqlalchemy.orm import sessionmaker

engine = create_async_engine("postgresql+asyncpg://user:pass@localhost/db")
async_session = sessionmaker(engine, class_=AsyncSession, expire_on_commit=False)

# models/user.py
from sqlalchemy import Column, Integer, String
from sqlalchemy.orm import declarative_base

Base = declarative_base()

class User(Base):
    __tablename__ = "users"
    id = Column(Integer, primary_key=True)
    device_id = Column(String, unique=True)
    created_at = Column(DateTime)
```

---

## 核心原则

- **当前**: 后端无数据库,保持无状态
- **未来**: 如需持久化,优先PostgreSQL
- **缓存**: 如需缓存,使用Redis

---

**总结**: 后端无数据库 + Android本地Room + 保持无状态
