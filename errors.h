/**
 * @brief 错误码定义
 */
#pragma once
#include <cstdint>

enum RtspErrorCode : int32_t
{
    RTSP_OK = 0,
    RTSP_ERR_INVALID_ARG = -1,
    RTSP_ERR_ALLOC_FAILED = -2,
    RTSP_ERR_HANDLE_CONFLICT = -3,     // handle重复冲突
    RTSP_ERR_ADDRESS_CONFLICT = -4,    // 播放地址已占用
    RTSP_ERR_NOT_FOUND = -5,
    RTSP_ERR_NETWORK = -6,
    RTSP_ERR_DECODE = -7,
    RTSP_ERR_QUEUE_FULL = -8,
    RTSP_ERR_RENDER = -9,
    RTSP_ERR_THREAD = -10
};
