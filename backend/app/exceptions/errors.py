"""自定义异常类"""

from typing import Optional


class VisionAPIError(Exception):
    """视觉API基础异常"""

    def __init__(self, message: str, should_fallback: bool = True):
        self.message = message
        self.should_fallback = should_fallback
        super().__init__(self.message)


class NewAPIError(VisionAPIError):
    """NewAPI调用错误"""

    def __init__(
        self, message: str, status_code: Optional[int] = None, retry: bool = False
    ):
        self.status_code = status_code
        self.retry = retry
        super().__init__(message, should_fallback=True)


class ImageProcessingError(VisionAPIError):
    """图片处理错误"""

    def __init__(self, message: str):
        super().__init__(message, should_fallback=False)


class TimeoutError(NewAPIError):
    """超时错误"""

    def __init__(self, message: str = "NewAPI调用超时"):
        super().__init__(message, status_code=504, retry=False)


class InvalidImageError(ImageProcessingError):
    """无效图片错误"""

    def __init__(self, message: str = "图片格式或大小不符合要求"):
        super().__init__(message)
