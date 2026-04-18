"""视觉识别相关数据模型"""

from pydantic import BaseModel, Field


class VisionResponse(BaseModel):
    """视觉识别成功响应"""

    success: bool = True
    emotion: str = Field(
        ..., description="情绪标签 (happy/sad/comfort/thinking/neutral)"
    )
    text: str = Field(..., description="描述文本 (≤20字)")
    processing_time: float = Field(..., description="处理耗时（秒）")


class VisionErrorResponse(BaseModel):
    """视觉识别失败响应（兜底）"""

    success: bool = False
    emotion: str = Field(default="neutral", description="兜底情绪标签")
    text: str = Field(..., description="兜底文本")
    error: str = Field(..., description="错误类型")
    processing_time: float = Field(..., description="处理耗时（秒）")
