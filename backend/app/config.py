"""配置管理模块"""

from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    """应用配置"""

    # NewAPI配置
    NEWAPI_BASE_URL: str = "http://localhost:3000/v1"
    NEWAPI_API_KEY: str = "test-key"  # 默认测试密钥

    # 应用配置
    APP_NAME: str = "FastAPI Vision API"
    APP_VERSION: str = "0.1.0"
    DEBUG: bool = False

    # 超时配置
    NEWAPI_TIMEOUT: float = 10.0  # NewAPI调用超时时间（秒）

    # 文件上传配置
    MAX_IMAGE_SIZE: int = 300 * 1024  # 最大图片大小 300KB
    ALLOWED_IMAGE_TYPES: list = ["image/jpeg", "image/jpg"]

    class Config:
        env_file = ".env"
        case_sensitive = True


settings = Settings()
