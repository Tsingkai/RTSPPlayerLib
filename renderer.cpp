#include "renderer.h"

RtspErrorCode render_frame(HWND hwnd, const std::shared_ptr<VideoFrame>& frame)
{
    if (!hwnd || !frame || frame->data.empty() || frame->width <= 0 ||
        frame->height <= 0 || frame->linesize < frame->width * 3)
        return RTSP_ERR_INVALID_ARG;

    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return RTSP_ERR_RENDER;

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
    return result == 0 || result == GDI_ERROR ? RTSP_ERR_RENDER : RTSP_OK;
}
