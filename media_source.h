/**
 * @brief FFmpeg媒体源模块，RTSP/RTMP拉流，低延迟参数配置
 */
#pragma once
#include "errors.h"
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

struct MediaSource
{
    AVFormatContext* fmtCtx = nullptr;
    int videoStreamIdx = -1;
    bool opened = false;
};

/**
 * @brief 打开 RTSP/RTMP 媒体源并选择视频流。
 * @param src 媒体源状态
 * @param url UTF-8 流地址
 * @return RTSP_OK 或具体错误码
 */
RtspErrorCode media_source_open(MediaSource* src, const char* url);

/**
 * @brief 关闭媒体源并重置其状态。
 * @param src 待关闭的媒体源
 */
void media_source_close(MediaSource* src);

/**
 * @brief 从媒体源读取一个压缩数据包。
 * @param src 已打开的媒体源
 * @param pkt 输出数据包
 * @return RTSP_OK 或 RTSP_ERR_NETWORK
 */
RtspErrorCode media_source_read_packet(MediaSource* src, AVPacket* pkt);
