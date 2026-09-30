#include "rtsp_player_api.h"
#include "registry.h"
#include "player.h"
#include <new>
#include <utility>

RTSP_API RtspErrorCode rtsp_player_create(RtspPlayerHandle handle, HWND hwnd, const char* url, const RtspPlayerConfig* cfg)
{
    if (!url || url[0] == '\0' || !cfg || !hwnd ||
        cfg->max_queue_ms <= 0 || cfg->reconnect_interval < 0)
        return RTSP_ERR_INVALID_ARG;

    std::shared_ptr<Player> player;
    try
    {
        player = std::make_shared<Player>(handle, hwnd, url, *cfg);
    }
    catch (const std::bad_alloc&)
    {
        return RTSP_ERR_ALLOC_FAILED;
    }

    const auto startResult = player->Start();
    if (startResult != RTSP_OK)
        return startResult;

    try
    {
        return PlayerRegistry::Instance().Register(handle, hwnd, url, std::move(player));
    }
    catch (const std::bad_alloc&)
    {
        return RTSP_ERR_ALLOC_FAILED;
    }
}

RTSP_API RtspErrorCode rtsp_player_destroy(RtspPlayerHandle handle)
{
    auto player = PlayerRegistry::Instance().GetPlayer(handle);
    if (!player) return RTSP_ERR_NOT_FOUND;
    return PlayerRegistry::Instance().Unregister(handle);
}

RTSP_API RtspErrorCode rtsp_player_pause(RtspPlayerHandle handle)
{
    auto player = PlayerRegistry::Instance().GetPlayer(handle);
    if (!player) return RTSP_ERR_NOT_FOUND;
    player->Pause();
    return RTSP_OK;
}

RTSP_API RtspErrorCode rtsp_player_resume(RtspPlayerHandle handle)
{
    auto player = PlayerRegistry::Instance().GetPlayer(handle);
    if (!player) return RTSP_ERR_NOT_FOUND;
    player->Resume();
    return RTSP_OK;
}

RTSP_API RtspErrorCode rtsp_player_reconnect(RtspPlayerHandle handle)
{
    auto player = PlayerRegistry::Instance().GetPlayer(handle);
    if (!player) return RTSP_ERR_NOT_FOUND;
    player->Reconnect();
    return RTSP_OK;
}

RTSP_API RtspErrorCode rtsp_player_get_stats(RtspPlayerHandle handle, uint64_t* frameCount, double* fps, int32_t* queueMs)
{
    if (!frameCount || !fps || !queueMs) return RTSP_ERR_INVALID_ARG;
    auto player = PlayerRegistry::Instance().GetPlayer(handle);
    if (!player) return RTSP_ERR_NOT_FOUND;
    player->GetStats(*frameCount, *fps, *queueMs);
    return RTSP_OK;
}

const char* rtsp_get_error_string(RtspErrorCode code)
{
    switch(code)
    {
        case RTSP_OK: return "OK";
        case RTSP_ERR_INVALID_ARG: return "参数非法";
        case RTSP_ERR_ALLOC_FAILED: return "内存分配失败";
        case RTSP_ERR_HANDLE_CONFLICT: return "Handle冲突，该句柄已被占用";
        case RTSP_ERR_ADDRESS_CONFLICT: return "流地址冲突，该地址正在播放";
        case RTSP_ERR_NOT_FOUND: return "Handle不存在";
        case RTSP_ERR_NETWORK: return "网络错误";
        case RTSP_ERR_DECODE: return "解码失败";
        case RTSP_ERR_QUEUE_FULL: return "队列满";
        case RTSP_ERR_RENDER: return "渲染失败";
        case RTSP_ERR_THREAD: return "线程创建失败";
        default: return "未知错误";
    }
}
