#include "registry.h"

PlayerRegistry& PlayerRegistry::Instance()
{
    static PlayerRegistry ins;
    return ins;
}

RtspErrorCode PlayerRegistry::Register(RtspPlayerHandle handle, HWND hwnd,
    const std::string& url, std::shared_ptr<Player> player)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_map.contains(handle))
    {
        return RTSP_ERR_HANDLE_CONFLICT;
    }
    // 检测同一个流地址是否已经被占用
    for (auto& pair : m_map)
    {
        if (pair.second.url == url)
        {
            return RTSP_ERR_ADDRESS_CONFLICT;
        }
    }
    PlayerEntry entry{};
    entry.hwnd = hwnd;
    entry.url = url;
    entry.player = std::move(player);
    m_map.emplace(handle, std::move(entry));
    return RTSP_OK;
}

RtspErrorCode PlayerRegistry::Unregister(RtspPlayerHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_map.find(handle);
    if (it == m_map.end())
        return RTSP_ERR_NOT_FOUND;
    m_map.erase(it);
    return RTSP_OK;
}

bool PlayerRegistry::Exists(RtspPlayerHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_map.contains(handle);
}

std::shared_ptr<Player> PlayerRegistry::GetPlayer(RtspPlayerHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_map.find(handle);
    if (it == m_map.end())
        return {};
    return it->second.player;
}
