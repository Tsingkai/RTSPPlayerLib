#include "renderer.h"
#include "logger.h"

RtspErrorCode render_frame(HWND hwnd, const std::shared_ptr<VideoFrame>& frame)
{
    if (!hwnd || !frame || frame->data.empty() || frame->width <= 0 ||
        frame->height <= 0 || frame->linesize < frame->width * 3)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "renderer: 帧参数非法 width=%d height=%d linesize=%d",
            frame ? frame->width : 0, frame ? frame->height : 0, frame ? frame->linesize : 0);
        return RTSP_ERR_INVALID_ARG;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "renderer: GetDC 失败 hwnd=%p", hwnd);
        return RTSP_ERR_RENDER;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = frame->width;
    bmi.bmiHeader.biHeight = -frame->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    const int result = StretchDIBits(hdc, 0, 0, frame->width, frame->height,
        0,0, frame->width, frame->height,
        frame->data.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, hdc);
    if (result == 0 || result == GDI_ERROR)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "renderer: StretchDIBits 失败 result=%d size=%dx%d",
            result, frame->width, frame->height);
        return RTSP_ERR_RENDER;
    }
    return RTSP_OK;
}
