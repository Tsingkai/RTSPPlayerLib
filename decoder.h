/**
 * @brief FFmpeg视频解码器（支持硬件加速，不可用时自动回退软件解码）
 */
#pragma once
#include "media_source.h"
#include "frame_queue.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

struct VideoDecoder
{
    AVCodecContext* codecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    AVRational frameRate{0, 1};
    AVBufferRef* hwDeviceCtx = nullptr;       // 硬件设备上下文；软件解码时为空
    AVPixelFormat hwPixFmt = AV_PIX_FMT_NONE; // 硬件帧像素格式；软件解码时为 AV_PIX_FMT_NONE
};

/**
 * @brief 根据媒体源的视频流初始化解码器。
 * @param dec 输出解码器状态
 * @param src 已打开且包含视频流的媒体源
 * @param enableHwAccel 是否优先启用硬件解码；失败时自动回退软件解码
 * @return RTSP_OK 或具体错误码
 */
RtspErrorCode decoder_init(VideoDecoder* dec, MediaSource* src, bool enableHwAccel);

/**
 * @brief 释放解码器、硬件设备上下文及像素格式转换器。
 * @param dec 待释放的解码器
 */
void decoder_destroy(VideoDecoder* dec);

/**
 * @brief 解码一个压缩包，将输出转换为带自动内存管理的 BGR24 帧。
 * @note 硬件解码时输出帧会自动下载到系统内存后再转换。
 * @param dec 已初始化的解码器
 * @param pkt 压缩视频包
 * @param outFrames 输出帧集合；每次调用前会清空
 * @return RTSP_OK 或具体错误码
 */
RtspErrorCode decoder_decode_packet(VideoDecoder* dec, AVPacket* pkt,
    std::vector<std::shared_ptr<VideoFrame>>& outFrames);
