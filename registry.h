/**
 * @brief Player 注册表：管理句柄并检测句柄、流地址冲突。
 */
#pragma once
#include "errors.h"
#include "rtsp_player_api.h"
#include <mutex>
#include <memory>
#include <unordered_map>
#include <string>

class Player;

struct PlayerEntry
{
    HWND hwnd = nullptr;
    std::string url;
    std::shared_ptr<Player> player;
};

/**
 * @brief 线程安全的播放器注册表，按句柄管理实例并防止流地址重复。
 */
class PlayerRegistry
{
public:
    /** @brief 获取进程内注册表单例。 */
    static PlayerRegistry& Instance();

    /**
     * @brief 注册播放器并检测句柄与流地址冲突。
     * @param handle 调用方提供的唯一句柄
     * @param hwnd 渲染窗口句柄
     * @param url 流地址
     * @param player 播放器共享引用
     * @return RTSP_OK / RTSP_ERR_HANDLE_CONFLICT / RTSP_ERR_ADDRESS_CONFLICT
     */
    RtspErrorCode Register(RtspPlayerHandle handle, HWND hwnd, const std::string& url,
        std::shared_ptr<Player> player);

    /**
     * @brief 注销句柄；已取得共享引用的调用可安全完成。
     * @param handle 待注销句柄
     * @return RTSP_OK 或 RTSP_ERR_NOT_FOUND
     */
    RtspErrorCode Unregister(RtspPlayerHandle handle);

    /**
     * @brief 查询句柄是否已注册。
     * @param handle 播放器句柄
     * @return 句柄存在时为 true
     */
    bool Exists(RtspPlayerHandle handle);

    /**
     * @brief 获取播放器的共享引用，保证并发销毁时实例仍然存活。
     * @param handle 播放器句柄
     * @return 播放器不存在时返回空引用
     */
    std::shared_ptr<Player> GetPlayer(RtspPlayerHandle handle);

private:
    std::mutex m_mutex;
    std::unordered_map<RtspPlayerHandle, PlayerEntry> m_map;
    PlayerRegistry() = default;
};
