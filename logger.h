/**
 * @brief 内部日志模块：线程安全分级日志，输出到调试器、可选文件与可选回调。
 */
#pragma once
#include "rtsp_player_api.h"

/**
 * @brief 写入一条日志（printf 风格格式化），线程安全，UTF-8 文本。
 * @note 供库内部使用；外部调用方请使用 rtsp_player_api.h 中的配置接口。
 */
void rtsp_log_write(RtspLogLevel level, const char* fmt, ...);

/**
 * @brief 将 FFmpeg av_log 输出重定向到本模块；随网络初始化调用一次。
 */
void rtsp_log_hook_ffmpeg();

/**
 * @brief 将 FFmpeg 错误码转为可读文本（线程本地缓冲）。
 * @param errnum avformat/avcodec 等返回的负数错误码
 */
const char* ff_err_str(int errnum);
