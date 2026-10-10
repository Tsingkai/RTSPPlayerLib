#include "player.h"
#include "logger.h"
#include "renderer.h"
#include <chrono>
#include <system_error>

Player::Player(RtspPlayerHandle handle, HWND hwnd, const char* url, const RtspPlayerConfig& cfg)
    : m_handle(handle), m_hwnd(hwnd), m_url(url), m_cfg(cfg),
    m_frameQueue(cfg.max_queue_ms, cfg.enable_drop_old_frame),
    m_running(false), m_paused(false), m_needReconnect(false), m_frameCount(0)
{}

Player::~Player()
{
    m_running = false;
    if (m_demuxThread.joinable()) m_demuxThread.join();
    if (m_renderThread.joinable()) m_renderThread.join();
    CloseSource();
    m_frameQueue.Clear();
    rtsp_log_write(RTSP_LOG_INFO, "player[%llu]: 已销毁并停止全部线程",
        static_cast<unsigned long long>(m_handle));
}

RtspErrorCode Player::Start()
{
    if (m_running)
        return RTSP_ERR_THREAD;

    const auto openResult = OpenSource();
    if (openResult != RTSP_OK)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "player[%llu]: 启动失败，打开流错误码=%d",
            static_cast<unsigned long long>(m_handle), openResult);
        return openResult;
    }

    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    rtsp_log_write(RTSP_LOG_INFO,
        "player[%llu]: 启动 url=%s hw_accel=%d max_queue_ms=%d reconnect_interval=%d drop_old=%d",
        static_cast<unsigned long long>(m_handle), m_url.c_str(),
        m_cfg.enable_hw_accel ? 1 : 0, m_cfg.max_queue_ms,
        m_cfg.reconnect_interval, m_cfg.enable_drop_old_frame ? 1 : 0);
    try
    {
        m_demuxThread = std::thread(&Player::DemuxThread, this);
        m_renderThread = std::thread(&Player::RenderThread, this);
    }
    catch (const std::system_error&)
    {
        m_running = false;
        rtsp_log_write(RTSP_LOG_ERROR, "player[%llu]: 工作线程创建失败",
            static_cast<unsigned long long>(m_handle));
        if (m_demuxThread.joinable()) m_demuxThread.join();
        if (m_renderThread.joinable()) m_renderThread.join();
        CloseSource();
        return RTSP_ERR_THREAD;
    }

    return RTSP_OK;
}

void Player::Pause()
{
    m_paused = true;
}

void Player::Resume()
{
    m_paused = false;
}

void Player::Reconnect()
{
    m_needReconnect = true;
}

RtspErrorCode Player::OpenSource()
{
    auto ret = media_source_open(&m_mediaSource, m_url.c_str());
    if (ret != RTSP_OK)
        return ret;

    ret = decoder_init(&m_decoder, &m_mediaSource, m_cfg.enable_hw_accel);
    if (ret != RTSP_OK)
    {
        media_source_close(&m_mediaSource);
        return ret;
    }

    return RTSP_OK;
}

void Player::CloseSource()
{
    decoder_destroy(&m_decoder);
    media_source_close(&m_mediaSource);
}

void Player::DemuxThread()
{
    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        rtsp_log_write(RTSP_LOG_ERROR, "player[%llu]: 数据包对象分配失败，线程退出",
            static_cast<unsigned long long>(m_handle));
        m_running = false;
        return;
    }

    while (m_running)
    {
        if (m_needReconnect.exchange(false))
        {
            CloseSource();
            rtsp_log_write(RTSP_LOG_INFO, "player[%llu]: 连接已断开，%dms 后重连",
                static_cast<unsigned long long>(m_handle), m_cfg.reconnect_interval);
            auto remaining = m_cfg.reconnect_interval;
            while (m_running && remaining > 0)
            {
                const auto delay = remaining < 50 ? remaining : 50;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                remaining -= delay;
            }
            if (m_running)
            {
                if (OpenSource() == RTSP_OK)
                {
                    rtsp_log_write(RTSP_LOG_INFO, "player[%llu]: 重连成功",
                        static_cast<unsigned long long>(m_handle));
                }
                else
                {
                    rtsp_log_write(RTSP_LOG_WARN, "player[%llu]: 重连失败，%dms 后重试",
                        static_cast<unsigned long long>(m_handle), m_cfg.reconnect_interval);
                    m_needReconnect = true;
                }
            }
            continue;
        }

        if (m_paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (!m_mediaSource.opened)
        {
            m_needReconnect = true;
            continue;
        }

        av_packet_unref(pkt);
        auto ret = media_source_read_packet(&m_mediaSource, pkt);
        if (ret != RTSP_OK)
        {
            rtsp_log_write(RTSP_LOG_DEBUG, "player[%llu]: 读取失败，触发重连",
                static_cast<unsigned long long>(m_handle));
            m_needReconnect = true;
            continue;
        }

        if (pkt->stream_index == m_mediaSource.videoStreamIdx)
        {
            std::vector<std::shared_ptr<VideoFrame>> frames;
            ret = decoder_decode_packet(&m_decoder, pkt, frames);
            if (ret != RTSP_OK)
            {
                rtsp_log_write(RTSP_LOG_WARN, "player[%llu]: 解码失败 code=%d，触发重连",
                    static_cast<unsigned long long>(m_handle), ret);
                m_needReconnect = true;
            }
            else
            {
                for (auto& frame : frames)
                {
                    if (!m_frameQueue.Enqueue(std::move(frame)))
                        rtsp_log_write(RTSP_LOG_DEBUG, "player[%llu]: 队列已满，跳过当前帧",
                            static_cast<unsigned long long>(m_handle));
                }
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
}

void Player::RenderThread()
{
    int consecutiveFailures = 0;
    while (m_running)
    {
        if (m_paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        auto frame = m_frameQueue.Dequeue(50);
        if (!frame) continue;
        if (render_frame(m_hwnd, frame) == RTSP_OK)
        {
            consecutiveFailures = 0;
            m_frameCount++;
        }
        else
        {
            ++consecutiveFailures;
            if (consecutiveFailures == 1 || consecutiveFailures % 100 == 0)
                rtsp_log_write(RTSP_LOG_ERROR, "player[%llu]: 渲染失败(连续%d次)，请检查窗口句柄是否有效",
                    static_cast<unsigned long long>(m_handle), consecutiveFailures);
        }
    }
}

void Player::GetStats(uint64_t& frameCount, double& fps, int32_t& queueMs)
{
    frameCount = m_frameCount.load();
    const double sec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_startTime).count();
    fps = sec > 0 ? frameCount / sec : 0.0;
    queueMs = m_frameQueue.GetMetrics().queue_duration_ms;
}
