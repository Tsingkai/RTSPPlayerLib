#include "logger.h"
#include <Windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
extern "C"
{
#include <libavutil/error.h>
#include <libavutil/log.h>
}

namespace
{
std::mutex g_mutex;
std::atomic<RtspLogLevel> g_level{RTSP_LOG_INFO};
RtspLogCallback g_callback = nullptr;
FILE* g_file = nullptr;

const char* level_tag(RtspLogLevel level)
{
    switch (level)
    {
        case RTSP_LOG_DEBUG: return "DEBUG";
        case RTSP_LOG_INFO:  return "INFO ";
        case RTSP_LOG_WARN:  return "WARN ";
        case RTSP_LOG_ERROR: return "ERROR";
        default:             return "LOG   ";
    }
}

RtspLogLevel map_ffmpeg_level(int level)
{
    if (level <= AV_LOG_ERROR)
        return RTSP_LOG_ERROR;
    if (level <= AV_LOG_WARNING)
        return RTSP_LOG_WARN;
    if (level <= AV_LOG_INFO)
        return RTSP_LOG_INFO;
    return RTSP_LOG_DEBUG;
}

void ffmpeg_log_redirect(void* avcl, int level, const char* fmt, va_list vl)
{
    (void)avcl;
    char message[2048];
    va_list copy;
    va_copy(copy, vl);
    vsnprintf(message, sizeof(message), fmt, copy);
    va_end(copy);
    message[sizeof(message) - 1] = '\0';

    size_t len = strlen(message);
    while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r'))
        message[--len] = '\0';

    rtsp_log_write(map_ffmpeg_level(level), "[ffmpeg] %s", message);
}
}

void rtsp_log_write(RtspLogLevel level, const char* fmt, ...)
{
    if (level < g_level)
        return;

    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    message[sizeof(message) - 1] = '\0';

    char timestamp[40];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    char line[2304];
    snprintf(line, sizeof(line), "[%s] [%s] [%u] %s\n",
        timestamp, level_tag(level), GetCurrentThreadId(), message);

    std::lock_guard<std::mutex> lock(g_mutex);
    wchar_t wline[2304];
    if (MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 2304) > 0)
        OutputDebugStringW(wline);

    if (g_file)
    {
        fputs(line, g_file);
        fflush(g_file);
    }

    if (g_callback)
        g_callback(level, message);
}

void rtsp_log_hook_ffmpeg()
{
    av_log_set_callback(ffmpeg_log_redirect);
}

const char* ff_err_str(int errnum)
{
    thread_local char buf[128];
    av_strerror(errnum, buf, sizeof(buf));
    return buf;
}

RTSP_API void rtsp_log_set_level(RtspLogLevel level)
{
    if (level < RTSP_LOG_DEBUG)
        level = RTSP_LOG_DEBUG;
    if (level > RTSP_LOG_ERROR)
        level = RTSP_LOG_ERROR;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_level = level;
    }
    rtsp_log_write(RTSP_LOG_INFO, "logger: 日志级别已设置为 %s", level_tag(level));
}

RTSP_API void rtsp_log_set_file(const char* path)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file)
    {
        fclose(g_file);
        g_file = nullptr;
    }
    if (!path || path[0] == '\0')
        return;

    FILE* file = nullptr;
    if (fopen_s(&file, path, "ab") != 0 || !file)
    {
        g_file = nullptr;
        return;
    }
    g_file = file;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_file, "---- %04u-%02u-%02u %02u:%02u:%02u.%03u 日志文件已打开 ----\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    fflush(g_file);
}

RTSP_API void rtsp_log_set_callback(RtspLogCallback cb)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_callback = cb;
    }
    rtsp_log_write(RTSP_LOG_INFO, "logger: 日志回调 %s", cb ? "已启用" : "已关闭");
}
