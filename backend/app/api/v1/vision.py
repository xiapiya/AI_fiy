"""视觉识别API路由"""

import time
from typing import Optional, Union

from fastapi import APIRouter, File, Form, UploadFile

from app.config import settings
from app.core.logging import logger
from app.exceptions.errors import (
    InvalidImageError,
    NewAPIError,
    TimeoutError,
)
from app.models.vision import VisionErrorResponse, VisionResponse
from app.services.fallback import FallbackService
from app.services.newapi_client import NewAPIClient

router = APIRouter()


@router.post(
    "/upload",
    response_model=Union[VisionResponse, VisionErrorResponse],
    summary="上传图片进行视觉识别",
    description="接收ESP32上传的JPEG图片，调用Qwen-VL进行视觉识别，返回情感化描述",
)
async def upload_vision(
    image: UploadFile = File(..., description="JPEG图片文件"),
    device_id: Optional[str] = Form(None, description="ESP32设备ID"),
) -> Union[VisionResponse, VisionErrorResponse]:
    """
    视觉识别接口

    接收JPEG图片，返回包含emotion和text的响应

    MVP范围：
    - 接收图片上传
    - 调用NewAPI/Qwen-VL
    - 10秒超时+兜底机制
    - 返回emotion和text（暂不合成音频）
    """
    start_time = time.time()

    logger.info(
        "接收视觉识别请求",
        extra={
            "context": {
                "device_id": device_id,
                "filename": image.filename,
                "content_type": image.content_type,
            }
        },
    )

    try:
        # 1. 验证图片类型
        if image.content_type not in settings.ALLOWED_IMAGE_TYPES:
            raise InvalidImageError(
                f"不支持的图片格式: {image.content_type}，仅支持JPEG"
            )

        # 2. 读取图片数据
        image_data = await image.read()

        # 3. 验证图片大小
        if len(image_data) > settings.MAX_IMAGE_SIZE:
            raise InvalidImageError(
                f"图片过大: {len(image_data)} bytes，最大限制 {settings.MAX_IMAGE_SIZE} bytes"
            )

        logger.info(
            "图片验证通过",
            extra={
                "context": {
                    "device_id": device_id,
                    "size": len(image_data),
                }
            },
        )

        # 4. 调用NewAPI
        newapi_client = NewAPIClient()
        result = await newapi_client.call_qwen_vl(image_data, device_id)

        # 5. 计算处理时间
        processing_time = time.time() - start_time

        logger.info(
            "视觉识别成功",
            extra={
                "context": {
                    "device_id": device_id,
                    "emotion": result["emotion"],
                    "processing_time": processing_time,
                }
            },
        )

        # 6. 返回成功响应
        return VisionResponse(
            success=True,
            emotion=result["emotion"],
            text=result["text"],
            processing_time=round(processing_time, 2),
        )

    except InvalidImageError as e:
        # 图片验证失败
        processing_time = time.time() - start_time
        fallback = FallbackService.get_fallback_response("invalid_image")

        logger.warning(
            "图片验证失败",
            extra={
                "context": {
                    "device_id": device_id,
                    "error": str(e),
                }
            },
        )

        return VisionErrorResponse(
            success=False,
            emotion=fallback["emotion"],
            text=fallback["text"],
            error="invalid_image",
            processing_time=round(processing_time, 2),
        )

    except TimeoutError:
        # NewAPI超时
        processing_time = time.time() - start_time
        fallback = FallbackService.get_fallback_response("timeout")

        logger.error(
            "NewAPI调用超时",
            extra={
                "context": {
                    "device_id": device_id,
                    "timeout": settings.NEWAPI_TIMEOUT,
                }
            },
        )

        return VisionErrorResponse(
            success=False,
            emotion=fallback["emotion"],
            text=fallback["text"],
            error="timeout",
            processing_time=round(processing_time, 2),
        )

    except NewAPIError as e:
        # NewAPI调用失败
        processing_time = time.time() - start_time
        fallback = FallbackService.get_fallback_response("newapi_error")

        logger.error(
            "NewAPI调用失败",
            extra={
                "context": {
                    "device_id": device_id,
                    "error": str(e),
                }
            },
        )

        return VisionErrorResponse(
            success=False,
            emotion=fallback["emotion"],
            text=fallback["text"],
            error="newapi_error",
            processing_time=round(processing_time, 2),
        )

    except Exception as e:
        # 其他未预期错误
        processing_time = time.time() - start_time
        fallback = FallbackService.get_fallback_response("error")

        logger.error(
            "视觉识别失败",
            extra={
                "context": {
                    "device_id": device_id,
                    "error": str(e),
                }
            },
        )

        return VisionErrorResponse(
            success=False,
            emotion=fallback["emotion"],
            text=fallback["text"],
            error="internal_error",
            processing_time=round(processing_time, 2),
        )


@router.get("/health", summary="健康检查")
async def health_check():
    """健康检查接口"""
    return {"status": "ok", "service": "vision-api"}
