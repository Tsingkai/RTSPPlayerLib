/**
 * @brief 有界帧队列，用于解耦解码生产线程与渲染消费线程。
 */
#pragma once
#include "latency_control.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

struct VideoFrame
{
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int linesize = 0;
    int64_t pts = 0;
    int duration_ms = 0;
};

/**
 * @brief 有界帧队列，连接解码生产者与渲染消费者。
 */
class FrameQueue
{
public:
    /** @brief 创建具有时长上限并指定超限处置策略的帧队列。 */
    explicit FrameQueue(int32_t maxMs, bool dropOldFrames);

    /**
     * @brief 按配置执行有界入队；必要时丢弃旧帧或跳过当前帧。
     * @return true 表示帧已入队，false 表示当前帧因队列上限被跳过。
     */
    bool Enqueue(std::shared_ptr<VideoFrame> frame);

    /**
     * @brief 等待并取出下一帧。
     */
    std::shared_ptr<VideoFrame> Dequeue(int timeoutMs);

    /**
     * @brief 获取队列当前时长与上限。
     */
    QueueMetrics GetMetrics() const;

    /**
     * @brief 清空队列并释放其中帧数据。
     */
    void Clear();

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::shared_ptr<VideoFrame>> m_queue;
    int32_t m_maxQueueMs;
    int32_t m_totalDurationMs;
    bool m_dropOldFrames;
};
