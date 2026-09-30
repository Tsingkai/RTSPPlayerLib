/**
 * @brief 延迟控制策略的纯函数模块；输入指标和输出决策分离，无全局状态。
 */
#pragma once
#include <cstdint>

struct QueueMetrics
{
    int32_t queue_duration_ms;    // 当前队列内帧总时长ms
    int32_t max_queue_ms;         // 队列上限
};

enum FrameDisposeDecision
{
    FRAME_KEEP,
    FRAME_DROP_OLD,
    FRAME_SKIP_CURRENT
};

/**
 * @brief 根据预计入队后的时长和配置决策帧处理策略（纯函数）。
 * @param metrics 队列指标
 * @param incomingDurationMs 即将入队帧的估算时长
 * @param dropOldFrames 队列超限时是否允许丢弃旧帧
 * @return 帧处置策略
 */
FrameDisposeDecision decide_frame_action(const QueueMetrics& metrics, int32_t incomingDurationMs, bool dropOldFrames);
