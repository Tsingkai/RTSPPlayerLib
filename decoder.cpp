#include "decoder.h"
#include "logger.h"
#include <algorithm>
#include <cmath>
#include <new>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace
{
// get_format 回调：从解码器支持的像素格式中选择硬件帧格式
enum AVPixelFormat hw_get_format(AVCodecContext* ctx, const enum AVPixelFormat* fmts)
{
    auto* dec = static_cast<VideoDecoder*>(ctx->opaque);
    if (!dec)
        return AV_PIX_FMT_NONE;
    for (const enum AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; ++p)
    {
        if (*p == dec->hwPixFmt)
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

// 查找指定硬件类型下解码器可用的 hw config，输出硬件帧像素格式
const AVCodecHWConfig* find_hw_config(const AVCodec* codec, AVHWDeviceType type, AVPixelFormat& outPixFmt)
{
    for (int i = 0; ; ++i)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (!config)
            return nullptr;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type)
        {
            outPixFmt = config->pix_fmt;
            return config;
        }
    }
}

bool decoder_try_enable_hw(VideoDecoder* dec, const AVCodec* codec)
{
    const AVHWDeviceType types[] = {
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_QSV,
        AV_HWDEVICE_TYPE_DXVA2,
    };

    for (AVHWDeviceType type : types)
    {
        AVBufferRef* device = nullptr;
        const int createRet = av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0);
        if (createRet < 0)
        {
            rtsp_log_write(RTSP_LOG_DEBUG, "decoder: 硬件设备 %s 创建失败: %s",
                av_hwdevice_get_type_name(type), ff_err_str(createRet));
            continue;
        }

        AVPixelFormat pixFmt = AV_PIX_FMT_NONE;
        if (!find_hw_config(codec, type, pixFmt))
        {
            rtsp_log_write(RTSP_LOG_DEBUG, "decoder: 解码器 %s 不支持 %s",
                codec->name, av_hwdevice_get_type_name(type));
            av_buffer_unref(&device);
            continue;
        }

        dec->hwPixFmt = pixFmt;
        dec->codecCtx->get_format = hw_get_format;
        AVBufferRef* codecRef = av_buffer_ref(device);
        if (!codecRef)
        {
            dec->hwPixFmt = AV_PIX_FMT_NONE;
            av_buffer_unref(&device);
            continue;
        }
        dec->hwDeviceCtx = device;
        dec->codecCtx->hw_device_ctx = codecRef;
        rtsp_log_write(RTSP_LOG_INFO, "decoder: 启用硬件解码 type=%s pix_fmt=%s codec=%s",
            av_hwdevice_get_type_name(type), av_get_pix_fmt_name(pixFmt), codec->name);
        return true;
    }

    rtsp_log_write(RTSP_LOG_WARN, "decoder: 无可用硬件解码器，回退软件解码 codec=%s", codec->name);
    return false;
}
}

RtspErrorCode decoder_init(VideoDecoder* dec, MediaSource* src, bool enableHwAccel)
{
    if (!dec || !src || !src->opened || !src->fmtCtx ||
        src->videoStreamIdx < 0 || src->videoStreamIdx >= static_cast<int>(src->fmtCtx->nb_streams))
        return RTSP_ERR_INVALID_ARG;

    decoder_destroy(dec);
    AVStream* st = src->fmtCtx->streams[src->videoStreamIdx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "decoder: 未找到 codec_id=%d 对应的解码器",
            st->codecpar->codec_id);
        return RTSP_ERR_DECODE;
    }

    // 分配上下文并从流参数填充；硬件路径打开失败后重建上下文重试软件解码
    const auto resetContext = [dec, st, codec]() -> bool
    {
        decoder_destroy(dec);
        dec->codecCtx = avcodec_alloc_context3(codec);
        if (!dec->codecCtx)
            return false;
        dec->codecCtx->opaque = dec;
        if (avcodec_parameters_to_context(dec->codecCtx, st->codecpar) < 0)
        {
            decoder_destroy(dec);
            return false;
        }
        return true;
    };

    if (!resetContext())
        return RTSP_ERR_ALLOC_FAILED;

    bool hwReady = false;
    if (enableHwAccel && decoder_try_enable_hw(dec, codec))
    {
        if (avcodec_open2(dec->codecCtx, codec, nullptr) >= 0)
        {
            hwReady = true;
        }
        else
        {
            rtsp_log_write(RTSP_LOG_WARN, "decoder: 硬件解码器打开失败，回退软件解码 codec=%s", codec->name);
            if (!resetContext())
                return RTSP_ERR_ALLOC_FAILED;
        }
    }

    if (!hwReady && avcodec_open2(dec->codecCtx, codec, nullptr) < 0)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "decoder: 解码器打开失败 codec=%s", codec->name);
        decoder_destroy(dec);
        return RTSP_ERR_DECODE;
    }

    dec->frameRate = av_guess_frame_rate(src->fmtCtx, st, nullptr);
    rtsp_log_write(RTSP_LOG_INFO, "decoder: 解码器就绪 codec=%s mode=%s",
        codec->name, hwReady ? "hardware" : "software");
    return RTSP_OK;
}

void decoder_destroy(VideoDecoder* dec)
{
    if (!dec)
        return;

    if (dec->codecCtx)
        avcodec_free_context(&dec->codecCtx); // 内部会释放其 hw_device_ctx 引用
    if (dec->hwDeviceCtx)
        av_buffer_unref(&dec->hwDeviceCtx);
    if (dec->swsCtx)
    {
        sws_freeContext(dec->swsCtx);
        dec->swsCtx = nullptr;
    }
    dec->hwPixFmt = AV_PIX_FMT_NONE;
    dec->frameRate = AVRational{0, 1};
}

RtspErrorCode decoder_decode_packet(VideoDecoder* dec, AVPacket* pkt,
    std::vector<std::shared_ptr<VideoFrame>>& outFrames)
{
    if (!dec || !dec->codecCtx || !pkt)
        return RTSP_ERR_INVALID_ARG;

    outFrames.clear();
    const int sendResult = avcodec_send_packet(dec->codecCtx, pkt);
    if (sendResult < 0 && sendResult != AVERROR(EAGAIN))
    {
        rtsp_log_write(RTSP_LOG_ERROR, "decoder: 送入数据包失败: %s", ff_err_str(sendResult));
        return RTSP_ERR_DECODE;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return RTSP_ERR_ALLOC_FAILED;

    int ret = 0;
    try
    {
        while ((ret = avcodec_receive_frame(dec->codecCtx, frame)) >= 0)
        {
            if (frame->width <= 0 || frame->height <= 0)
            {
                av_frame_unref(frame);
                continue;
            }

            // 硬件帧下载到系统内存后再做像素转换
            AVFrame* cpuFrame = frame;
            AVFrame* swFrame = nullptr;
            if (dec->hwPixFmt != AV_PIX_FMT_NONE && frame->format == dec->hwPixFmt)
            {
                swFrame = av_frame_alloc();
                if (!swFrame)
                {
                    av_frame_unref(frame);
                    av_frame_free(&frame);
                    return RTSP_ERR_ALLOC_FAILED;
                }
                const int transferRet = av_hwframe_transfer_data(swFrame, frame, 0);
                if (transferRet < 0)
                {
                    rtsp_log_write(RTSP_LOG_ERROR, "decoder: 硬件帧下载失败: %s", ff_err_str(transferRet));
                    av_frame_free(&swFrame);
                    av_frame_unref(frame);
                    av_frame_free(&frame);
                    return RTSP_ERR_DECODE;
                }
                swFrame->pts = frame->pts;
                cpuFrame = swFrame;
            }

            const int bytesPerLine = (cpuFrame->width * 3 + 3) & ~3;
            const size_t bufferSize = static_cast<size_t>(bytesPerLine) * cpuFrame->height;
            auto output = std::make_shared<VideoFrame>();
            output->width = cpuFrame->width;
            output->height = cpuFrame->height;
            output->linesize = bytesPerLine;
            output->pts = cpuFrame->pts;
            const double fps = av_q2d(dec->frameRate);
            output->duration_ms = fps > 0.0
                ? std::max(1, static_cast<int>(std::lround(1000.0 / fps)))
                : 33;
            output->data.resize(bufferSize);

            dec->swsCtx = sws_getCachedContext(dec->swsCtx,
                cpuFrame->width, cpuFrame->height, static_cast<AVPixelFormat>(cpuFrame->format),
                cpuFrame->width, cpuFrame->height, AV_PIX_FMT_BGR24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!dec->swsCtx)
            {
                av_frame_free(&swFrame);
                av_frame_unref(frame);
                av_frame_free(&frame);
                return RTSP_ERR_DECODE;
            }

            uint8_t* dst[] = { output->data.data() };
            int dstStride[] = { bytesPerLine };
            if (sws_scale(dec->swsCtx, cpuFrame->data, cpuFrame->linesize, 0,
                cpuFrame->height, dst, dstStride) <= 0)
            {
                av_frame_free(&swFrame);
                av_frame_unref(frame);
                av_frame_free(&frame);
                return RTSP_ERR_DECODE;
            }

            outFrames.push_back(std::move(output));
            av_frame_free(&swFrame);
            av_frame_unref(frame);
        }
    }
    catch (const std::bad_alloc&)
    {
        av_frame_free(&frame);
        return RTSP_ERR_ALLOC_FAILED;
    }

    av_frame_free(&frame);
    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "decoder: 接收解码帧失败: %s", ff_err_str(ret));
        return RTSP_ERR_DECODE;
    }

    return RTSP_OK;
}
