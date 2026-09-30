/**
 * @brief GDI 渲染模块，将 BGR24 帧绘制到 HWND 窗口。
 */
#pragma once
#include "frame_queue.h"
#include <Windows.h>
#include "errors.h"

/**
 * @brief 将 BGR24 VideoFrame 绘制到目标 HWND；调用线程必须允许执行 GDI 操作。
 * @param hwnd 目标窗口句柄
 * @param frame BGR24 像素帧
 * @return RTSP_OK 或渲染/参数错误
 */
RtspErrorCode render_frame(HWND hwnd, const std::shared_ptr<VideoFrame>& frame);
