/**
 * @brief 播放器核心类：拉流/解码生产线程、渲染消费线程。
 */
#pragma once
#include "rtsp_player_api.h"
#include "media_source.h"
#include "decoder.h"
#include "frame_queue.h"
#include <chrono>
#include <thread>
#include <atomic>

class Player
{
public:
    /** @brief 创建一个播放器实例；配置须已由 C API 校验。 */
    Player(RtspPlayerHandle handle, HWND hwnd, const char* url, const RtspPlayerConfig& cfg);

    /** @brief 停止工作线程并释放媒体、解码及帧队列资源。 */
    ~Player();

    /** @brief 打开媒体并启动拉流/解码与渲染线程。 */
    RtspErrorCode Start();

    /** @brief 暂停消费输入与渲染。 */
    void Pause();

    /** @brief 恢复播放。 */
    void Resume();

    /** @brief 请求工作线程重建媒体源和解码器。 */
    void Reconnect();

    /** @brief 读取累计渲染帧数、平均 FPS 与当前队列时长。 */
    void GetStats(uint64_t& frameCount, double& fps, int32_t& queueMs);

private:
    /** @brief 读取视频包并同步解码，将输出帧送入有界帧队列。 */
    void DemuxThread();

    /** @brief 从帧队列取帧并绘制到目标窗口。 */
    void RenderThread();

    /** @brief 打开媒体源并初始化其视频解码器。 */
    RtspErrorCode OpenSource();

    /** @brief 释放媒体源与解码器资源。 */
    void CloseSource();

    RtspPlayerHandle m_handle;
    HWND m_hwnd;
    std::string m_url;
    RtspPlayerConfig m_cfg;

    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_needReconnect;

    MediaSource m_mediaSource;
    VideoDecoder m_decoder;
    FrameQueue m_frameQueue;

    std::thread m_demuxThread;
    std::thread m_renderThread;

    std::atomic<uint64_t> m_frameCount;
    std::chrono::steady_clock::time_point m_startTime;
};
