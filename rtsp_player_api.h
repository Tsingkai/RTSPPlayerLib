/**
 * @brief RTSP播放器DLL对外C API头文件
 * @note 导出 C ABI，供 C# 等外部调用方通过 P/Invoke 使用。
 */
#pragma once
#include "errors.h"
#include <cstdint>
#include <Windows.h>
#ifdef _WIN32
#   ifdef RTSP_PLAYER_DLL_EXPORT
#       define RTSP_API extern "C" __declspec(dllexport)
#   else
#       define RTSP_API extern "C" __declspec(dllimport)
#   endif
#else
#   define RTSP_API extern "C"
#endif

// 播放器句柄类型，调用方传入，用于多路播放
typedef uint64_t RtspPlayerHandle;
// Windows窗口句柄
//typedef void* HWND;

/**
 * @brief 播放器配置参数
 */
typedef struct
{
    int32_t max_queue_ms;       // 最大缓冲队列时长(ms)，用于控制延迟
    int32_t reconnect_interval; // 网络断开重连间隔(ms)
    bool enable_drop_old_frame; // 队列满时丢弃旧帧追赶实时流
} RtspPlayerConfig;

/**
 * @brief 创建播放器实例
 * @param handle 调用方传入自定义唯一handle，重复则返回 RTSP_ERR_HANDLE_CONFLICT
 * @param hwnd 渲染目标窗口句柄（WinForm组件Handle）
 * @param url RTSP/RTMP流地址
 * @param cfg 播放器配置；max_queue_ms 必须大于 0，reconnect_interval 不得小于 0
 * @return RTSP_OK 成功；错误码失败
 */
RTSP_API RtspErrorCode rtsp_player_create(RtspPlayerHandle handle, HWND hwnd, const char* url, const RtspPlayerConfig* cfg);

/**
 * @brief 销毁播放器实例，释放资源
 * @param handle 播放器句柄
 * @return RTSP_OK 成功；RTSP_ERR_NOT_FOUND handle不存在
 */
RTSP_API RtspErrorCode rtsp_player_destroy(RtspPlayerHandle handle);

/**
 * @brief 暂停拉流和渲染；正在执行的网络读取会在 FFmpeg 超时后结束。
 * @param handle 播放器句柄
 */
RTSP_API RtspErrorCode rtsp_player_pause(RtspPlayerHandle handle);

/**
 * @brief 恢复拉流和渲染。
 * @param handle 播放器句柄
 */
RTSP_API RtspErrorCode rtsp_player_resume(RtspPlayerHandle handle);

/**
 * @brief 请求工作线程重新打开流地址和解码器。
 * @param handle 播放器句柄
 */
RTSP_API RtspErrorCode rtsp_player_reconnect(RtspPlayerHandle handle);

/**
 * @brief 获取累计成功渲染帧数、从启动起计算的平均 FPS 和队列时长。
 * @param handle 播放器句柄
 * @param frameCount 输出：累计渲染帧数
 * @param fps 输出：平均渲染帧率
 * @param queueMs 输出：当前排队帧的估算时长
 */
RTSP_API RtspErrorCode rtsp_player_get_stats(RtspPlayerHandle handle, uint64_t* frameCount, double* fps, int32_t* queueMs);

/** @brief 获取错误码对应的静态描述字符串。 */
RTSP_API const char* rtsp_get_error_string(RtspErrorCode code);
