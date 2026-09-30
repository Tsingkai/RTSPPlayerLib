#include "latency_control.h"

FrameDisposeDecision decide_frame_action(const QueueMetrics& metrics, int32_t incomingDurationMs, bool dropOldFrames)
{
    // 允许至少保留一帧，避免帧本身时长大于队列上限时永远无法输出。
    const int64_t projectedDurationMs =
        static_cast<int64_t>(metrics.queue_duration_ms) + incomingDurationMs;
    if (metrics.queue_duration_ms > 0 && projectedDurationMs > metrics.max_queue_ms)
    {
        return dropOldFrames ? FRAME_DROP_OLD : FRAME_SKIP_CURRENT;
    }
    return FRAME_KEEP;
}
