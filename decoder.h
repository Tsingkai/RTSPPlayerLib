/**
 * @brief FFmpeg视频解码器
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
};

/**
 * @brief 根据媒体源的视频流初始化解码器。
 * @param dec 输出解码器状态
 * @param src 已打开且包含视频流的媒体源
 * @return RTSP_OK 或具体错误码
 */
RtspErrorCode decoder_init(VideoDecoder* dec, MediaSource* src);

/**
 * @brief 释放解码器及像素格式转换器。
 * @param dec 待释放的解码器
 */
void decoder_destroy(VideoDecoder* dec);

/**
 * @brief 解码一个压缩包，将输出转换为带自动内存管理的 BGR24 帧。
 * @param dec 已初始化的解码器
 * @param pkt 压缩视频包
 * @param outFrames 输出帧集合；每次调用前会清空
 * @return RTSP_OK 或具体错误码
 */
RtspErrorCode decoder_decode_packet(VideoDecoder* dec, AVPacket* pkt,
    std::vector<std::shared_ptr<VideoFrame>>& outFrames);
