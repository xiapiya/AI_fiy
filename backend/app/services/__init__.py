"""服务层模块"""

from .fallback import FallbackService
from .newapi_client import NewAPIClient

__all__ = ["NewAPIClient", "FallbackService"]
