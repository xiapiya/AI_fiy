"""日志配置模块"""

import json
import logging
from datetime import datetime
from typing import Any, Dict


class JSONFormatter(logging.Formatter):
    """JSON格式化器"""

    def format(self, record: logging.LogRecord) -> str:
        log_data: Dict[str, Any] = {
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "level": record.levelname,
            "message": record.getMessage(),
            "module": record.module,
            "function": record.funcName,
        }

        # 添加额外上下文
        if hasattr(record, "context"):
            log_data["context"] = record.context

        return json.dumps(log_data, ensure_ascii=False)


def setup_logger(name: str = "backend", level: int = logging.INFO) -> logging.Logger:
    """初始化日志配置"""
    logger = logging.getLogger(name)
    logger.setLevel(level)

    # 避免重复添加handler
    if not logger.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(JSONFormatter())
        logger.addHandler(handler)

    return logger


# 全局logger实例
logger = setup_logger()
