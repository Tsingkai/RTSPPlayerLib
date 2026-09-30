#include "player.h"
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
}

RtspErrorCode Player::Start()
{
    if (m_running)
        return RTSP_ERR_THREAD;

    const auto openResult = OpenSource();
    if (openResult != RTSP_OK)
        return openResult;

    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    try
    {
        m_demuxThread = std::thread(&Player::DemuxThread, this);
        m_renderThread = std::thread(&Player::RenderThread, this);
    }
    catch (const std::system_error&)
    {
        m_running = false;
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

    ret = decoder_init(&m_decoder, &m_mediaSource);
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
        m_running = false;
        return;
    }

    while (m_running)
    {
        if (m_needReconnect.exchange(false))
        {
            CloseSource();
            auto remaining = m_cfg.reconnect_interval;
            while (m_running && remaining > 0)
            {
                const auto delay = remaining < 50 ? remaining : 50;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                remaining -= delay;
            }
            if (m_running && OpenSource() != RTSP_OK)
                m_needReconnect = true;
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
            m_needReconnect = true;
            continue;
        }

        if (pkt->stream_index == m_mediaSource.videoStreamIdx)
        {
            std::vector<std::shared_ptr<VideoFrame>> frames;
            ret = decoder_decode_packet(&m_decoder, pkt, frames);
            if (ret != RTSP_OK)
                m_needReconnect = true;
            else
            {
                for (auto& frame : frames)
                    m_frameQueue.Enqueue(std::move(frame));
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
}

void Player::RenderThread()
{
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
            m_frameCount++;
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
