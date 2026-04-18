"""FastAPI应用入口"""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api.v1 import api_router
from app.config import settings
from app.core.logging import logger

# 创建FastAPI应用
app = FastAPI(
    title=settings.APP_NAME,
    version=settings.APP_VERSION,
    description="ESP32视觉识别接口 - MVP版本",
    docs_url="/docs",
    redoc_url="/redoc",
)

# 配置CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # 生产环境应限制具体域名
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 注册路由
app.include_router(api_router, prefix="/api/v1")


@app.on_event("startup")
async def startup_event():
    """启动事件"""
    logger.info(
        "FastAPI应用启动",
        extra={
            "context": {
                "app_name": settings.APP_NAME,
                "version": settings.APP_VERSION,
                "newapi_url": settings.NEWAPI_BASE_URL,
            }
        },
    )


@app.on_event("shutdown")
async def shutdown_event():
    """关闭事件"""
    logger.info("FastAPI应用关闭")


@app.get("/", summary="根路径")
async def root():
    """根路径"""
    return {
        "message": "FastAPI Vision API",
        "version": settings.APP_VERSION,
        "docs": "/docs",
    }


@app.get("/health", summary="健康检查")
async def health():
    """健康检查"""
    return {"status": "ok"}


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "app.main:app",
        host="0.0.0.0",
        port=8000,
        reload=settings.DEBUG,
    )
