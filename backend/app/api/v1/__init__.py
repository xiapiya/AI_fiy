"""API v1路由"""

from fastapi import APIRouter

from .vision import router as vision_router

api_router = APIRouter()
api_router.include_router(vision_router, prefix="/vision", tags=["vision"])

__all__ = ["api_router"]
