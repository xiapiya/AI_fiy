"""自定义异常模块"""

from .errors import (
    ImageProcessingError,
    InvalidImageError,
    NewAPIError,
    TimeoutError,
    VisionAPIError,
)

__all__ = [
    "VisionAPIError",
    "NewAPIError",
    "ImageProcessingError",
    "TimeoutError",
    "InvalidImageError",
]
