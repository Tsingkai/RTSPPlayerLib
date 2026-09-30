#include "decoder.h"
#include <algorithm>
#include <cmath>
#include <new>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

RtspErrorCode decoder_init(VideoDecoder* dec, MediaSource* src)
{
    if (!dec || !src || !src->opened || !src->fmtCtx ||
        src->videoStreamIdx < 0 || src->videoStreamIdx >= static_cast<int>(src->fmtCtx->nb_streams))
        return RTSP_ERR_INVALID_ARG;

    decoder_destroy(dec);
    AVStream* st = src->fmtCtx->streams[src->videoStreamIdx];
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        return RTSP_ERR_DECODE;

    dec->codecCtx = avcodec_alloc_context3(codec);
    if (!dec->codecCtx)
        return RTSP_ERR_ALLOC_FAILED;

    if (avcodec_parameters_to_context(dec->codecCtx, st->codecpar) < 0 ||
        avcodec_open2(dec->codecCtx, codec, nullptr) < 0)
    {
        decoder_destroy(dec);
        return RTSP_ERR_DECODE;
    }

    dec->frameRate = av_guess_frame_rate(src->fmtCtx, st, nullptr);
    return RTSP_OK;
}

void decoder_destroy(VideoDecoder* dec)
{
    if (!dec)
        return;

    if (dec->codecCtx)
        avcodec_free_context(&dec->codecCtx);
    if (dec->swsCtx)
    {
        sws_freeContext(dec->swsCtx);
        dec->swsCtx = nullptr;
    }
    dec->frameRate = AVRational{0, 1};
}

RtspErrorCode decoder_decode_packet(VideoDecoder* dec, AVPacket* pkt,
    std::vector<std::shared_ptr<VideoFrame>>& outFrames)
{
    if (!dec || !dec->codecCtx || !pkt)
        return RTSP_ERR_INVALID_ARG;

    outFrames.clear();
    const int sendResult = avcodec_send_packet(dec->codecCtx, pkt);
    if (sendResult < 0)
        return RTSP_ERR_DECODE;

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

            const int bytesPerLine = (frame->width * 3 + 3) & ~3;
            const size_t bufferSize = static_cast<size_t>(bytesPerLine) * frame->height;
            auto output = std::make_shared<VideoFrame>();
            output->width = frame->width;
            output->height = frame->height;
            output->linesize = bytesPerLine;
            output->pts = frame->pts;
            const double fps = av_q2d(dec->frameRate);
            output->duration_ms = fps > 0.0
                ? std::max(1, static_cast<int>(std::lround(1000.0 / fps)))
                : 33;
            output->data.resize(bufferSize);

            dec->swsCtx = sws_getCachedContext(dec->swsCtx,
                frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                frame->width, frame->height, AV_PIX_FMT_BGR24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!dec->swsCtx)
            {
                av_frame_free(&frame);
                return RTSP_ERR_DECODE;
            }

            uint8_t* dst[] = { output->data.data() };
            int dstStride[] = { bytesPerLine };
            if (sws_scale(dec->swsCtx, frame->data, frame->linesize, 0,
                frame->height, dst, dstStride) <= 0)
            {
                av_frame_free(&frame);
                return RTSP_ERR_DECODE;
            }

            outFrames.push_back(std::move(output));
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
        return RTSP_ERR_DECODE;

    return RTSP_OK;
}
