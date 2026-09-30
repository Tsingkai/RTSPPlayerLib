#include "frame_queue.h"
#include <chrono>
#include <new>
#include <utility>

FrameQueue::FrameQueue(int32_t maxMs, bool dropOldFrames)
    : m_maxQueueMs(maxMs), m_totalDurationMs(0), m_dropOldFrames(dropOldFrames)
{}

bool FrameQueue::Enqueue(std::shared_ptr<VideoFrame> frame)
{
    if (!frame || frame->duration_ms <= 0)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    QueueMetrics metrics{ m_totalDurationMs, m_maxQueueMs };
    const auto decision = decide_frame_action(metrics, frame->duration_ms, m_dropOldFrames);
    if (decision == FRAME_SKIP_CURRENT)
        return false;

    const int32_t frameDurationMs = frame->duration_ms;
    try
    {
        m_queue.push(std::move(frame));
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    m_totalDurationMs += frameDurationMs;
    if (decision == FRAME_DROP_OLD)
    {
        while (m_queue.size() > 1 && m_totalDurationMs > m_maxQueueMs)
        {
            m_totalDurationMs -= m_queue.front()->duration_ms;
            m_queue.pop();
        }
    }
    m_cv.notify_one();
    return true;
}

std::shared_ptr<VideoFrame> FrameQueue::Dequeue(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this]() { return !m_queue.empty(); }))
    {
        return nullptr;
    }
    auto frame = m_queue.front();
    m_queue.pop();
    m_totalDurationMs -= frame->duration_ms;
    return frame;
}

QueueMetrics FrameQueue::GetMetrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_totalDurationMs, m_maxQueueMs};
}

void FrameQueue::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty())
        m_queue.pop();
    m_totalDurationMs = 0;
    m_cv.notify_all();
}
