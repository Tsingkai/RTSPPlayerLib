#include "media_source.h"
#include "logger.h"
#include <mutex>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

RtspErrorCode media_source_open(MediaSource* src, const char* url)
{
    if (!src || !url || url[0] == '\0')
        return RTSP_ERR_INVALID_ARG;

    media_source_close(src);
    static std::once_flag networkInitFlag;
    std::call_once(networkInitFlag, []()
    {
        rtsp_log_hook_ffmpeg(); // 先挂接日志再初始化，捕捉 FFmpeg 早期输出
        avformat_network_init();
    });

    AVDictionary* opts = nullptr;
    // Keep transport buffering low while bounding time spent in a stalled read.
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);
    av_dict_set(&opts, "fflags", "nobuffer", 0);
    av_dict_set(&opts, "max_delay", "100000", 0);
    av_dict_set(&opts, "rw_timeout", "10000000", 0);

    int ret = avformat_open_input(&src->fmtCtx, url, nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "media_source: 打开流失败 url=%s: %s", url, ff_err_str(ret));
        media_source_close(src);
        return RTSP_ERR_NETWORK;
    }

    ret = avformat_find_stream_info(src->fmtCtx, nullptr);
    if (ret < 0)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "media_source: 探测流信息失败 url=%s: %s", url, ff_err_str(ret));
        media_source_close(src);
        return RTSP_ERR_NETWORK;
    }

    src->videoStreamIdx = av_find_best_stream(src->fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (src->videoStreamIdx < 0)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "media_source: 流中未找到视频轨 url=%s", url);
        media_source_close(src);
        return RTSP_ERR_DECODE;
    }

    src->opened = true;
    rtsp_log_write(RTSP_LOG_INFO, "media_source: 流已打开 url=%s videoStream=%d",
        url, src->videoStreamIdx);
    return RTSP_OK;
}

void media_source_close(MediaSource* src)
{
    if (!src)
        return;

    if (src->fmtCtx)
        avformat_close_input(&src->fmtCtx);

    src->videoStreamIdx = -1;
    src->opened = false;
}

RtspErrorCode media_source_read_packet(MediaSource* src, AVPacket* pkt)
{
    if (!src || !pkt || !src->opened || !src->fmtCtx)
        return RTSP_ERR_NETWORK;

    const int ret = av_read_frame(src->fmtCtx, pkt);
    if (ret < 0)
    {
        rtsp_log_write(RTSP_LOG_WARN, "media_source: 读取数据包失败: %s", ff_err_str(ret));
        return RTSP_ERR_NETWORK;
    }

    return RTSP_OK;
}
