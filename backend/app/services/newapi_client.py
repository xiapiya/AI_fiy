"""NewAPI客户端服务"""

import asyncio
import base64
from typing import Dict, Optional

from openai import AsyncOpenAI

from app.config import settings
from app.core.logging import logger
from app.exceptions.errors import NewAPIError
from app.exceptions.errors import TimeoutError as CustomTimeoutError


class NewAPIClient:
    """NewAPI网关客户端"""

    def __init__(self):
        self.client = AsyncOpenAI(
            base_url=settings.NEWAPI_BASE_URL, api_key=settings.NEWAPI_API_KEY
        )
        self.timeout = settings.NEWAPI_TIMEOUT

    async def call_qwen_vl(
        self, image_data: bytes, device_id: Optional[str] = None
    ) -> Dict[str, str]:
        """
        调用Qwen-VL-Max进行视觉识别

        Args:
            image_data: JPEG图片二进制数据
            device_id: ESP32设备ID（可选）

        Returns:
            包含emotion和text的字典

        Raises:
            CustomTimeoutError: 调用超时
            NewAPIError: NewAPI调用失败
        """
        # 将图片转换为base64编码的data URL
        image_base64 = base64.b64encode(image_data).decode("utf-8")
        image_url = f"data:image/jpeg;base64,{image_base64}"

        prompt = "用10字以内描述图片+情绪标签(happy/sad/comfort/thinking/neutral)"

        logger.info(
            "开始调用NewAPI/Qwen-VL",
            extra={
                "context": {
                    "model": "qwen-vl-max-latest",
                    "endpoint": settings.NEWAPI_BASE_URL,
                    "device_id": device_id,
                    "image_size": len(image_data),
                }
            },
        )

        try:
            # 设置超时调用
            response = await asyncio.wait_for(
                self.client.chat.completions.create(
                    model="qwen-vl-max-latest",
                    messages=[
                        {
                            "role": "user",
                            "content": [
                                {"type": "image_url", "image_url": {"url": image_url}},
                                {"type": "text", "text": prompt},
                            ],
                        }
                    ],
                    max_tokens=50,
                ),
                timeout=self.timeout,
            )

            # 解析响应
            result_text = response.choices[0].message.content or ""
            logger.info(
                "NewAPI调用成功",
                extra={
                    "context": {
                        "result": result_text,
                        "device_id": device_id,
                    }
                },
            )

            # 提取情绪和文本
            emotion, text = self._parse_response(result_text)
            return {"emotion": emotion, "text": text}

        except asyncio.TimeoutError as e:
            logger.error(
                "NewAPI调用超时",
                extra={
                    "context": {
                        "timeout": self.timeout,
                        "device_id": device_id,
                    }
                },
            )
            raise CustomTimeoutError("NewAPI调用超时（10秒）") from e

        except Exception as e:
            logger.error(
                "NewAPI调用失败",
                extra={
                    "context": {
                        "error": str(e),
                        "device_id": device_id,
                    }
                },
            )
            raise NewAPIError(f"调用失败: {str(e)}") from e

    def _parse_response(self, text: str) -> tuple[str, str]:
        """
        解析AI响应，提取情绪和文本

        Args:
            text: AI返回的文本

        Returns:
            (emotion, text) 元组
        """
        # 简单的情绪提取逻辑
        text_lower = text.lower()
        emotion = "neutral"

        # 检查是否包含情绪关键词
        if any(
            keyword in text_lower
            for keyword in ["happy", "开心", "高兴", "快乐", "哈哈"]
        ):
            emotion = "happy"
        elif any(keyword in text_lower for keyword in ["sad", "难过", "伤心", "哭"]):
            emotion = "sad"
        elif any(
            keyword in text_lower for keyword in ["comfort", "安慰", "理解", "陪伴"]
        ):
            emotion = "comfort"
        elif any(keyword in text_lower for keyword in ["thinking", "思考", "想想"]):
            emotion = "thinking"

        # 移除可能的情绪标签标记
        clean_text = text
        for tag in ["(happy)", "(sad)", "(comfort)", "(thinking)", "(neutral)"]:
            clean_text = clean_text.replace(tag, "").strip()

        # 限制文本长度为20字
        if len(clean_text) > 20:
            clean_text = clean_text[:20]

        return emotion, clean_text
