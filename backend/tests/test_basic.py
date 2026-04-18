"""基础测试"""



def test_imports():
    """测试模块导入"""
    from app.config import settings
    from app.main import app
    from app.models.vision import VisionErrorResponse, VisionResponse
    from app.services.fallback import FallbackService
    from app.services.newapi_client import NewAPIClient

    assert app is not None
    assert settings is not None
    assert NewAPIClient is not None
    assert FallbackService is not None
    assert VisionResponse is not None
    assert VisionErrorResponse is not None


def test_fallback_service():
    """测试兜底服务"""
    from app.services.fallback import FallbackService

    # 测试超时兜底
    fallback = FallbackService.get_fallback_response("timeout")
    assert fallback["emotion"] == "neutral"
    assert fallback["text"] == "网络有点慢，稍等一下"

    # 测试无效图片兜底
    fallback = FallbackService.get_fallback_response("invalid_image")
    assert fallback["emotion"] == "neutral"
    assert fallback["text"] == "图片好像有点问题"

    # 测试默认兜底
    fallback = FallbackService.get_fallback_response("unknown")
    assert fallback["emotion"] == "neutral"
    assert "看不太清楚" in fallback["text"]


def test_vision_response_model():
    """测试响应模型"""
    from app.models.vision import VisionErrorResponse, VisionResponse

    # 测试成功响应
    response = VisionResponse(
        success=True, emotion="happy", text="测试文本", processing_time=1.5
    )
    assert response.success is True
    assert response.emotion == "happy"
    assert response.text == "测试文本"
    assert response.processing_time == 1.5

    # 测试失败响应
    error_response = VisionErrorResponse(
        success=False,
        emotion="neutral",
        text="兜底文本",
        error="timeout",
        processing_time=10.0,
    )
    assert error_response.success is False
    assert error_response.emotion == "neutral"
    assert error_response.error == "timeout"
