"""兜底机制服务"""

from typing import Dict


class FallbackService:
    """兜底响应服务"""

    # 预定义的兜底响应
    FALLBACK_RESPONSES: Dict[str, Dict[str, str]] = {
        "timeout": {"emotion": "neutral", "text": "网络有点慢，稍等一下"},
        "error": {"emotion": "neutral", "text": "我暂时看不太清楚"},
        "invalid_image": {"emotion": "neutral", "text": "图片好像有点问题"},
        "newapi_error": {"emotion": "neutral", "text": "让我想想"},
    }

    @classmethod
    def get_fallback_response(cls, error_type: str) -> Dict[str, str]:
        """
        获取兜底响应

        Args:
            error_type: 错误类型 (timeout/error/invalid_image/newapi_error)

        Returns:
            包含emotion和text的字典
        """
        return cls.FALLBACK_RESPONSES.get(error_type, cls.FALLBACK_RESPONSES["error"])
