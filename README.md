# RTSPPlayerLib

RTSPPlayerLib 是一个面向 Windows 的 C++20 动态库，通过 C ABI 为 C# 等调用方提供多实例 RTSP/RTMP 视频播放接口。它使用 FFmpeg 拉流、解码并转换为 BGR24，再通过 GDI 绘制到指定的 `HWND`。当前实现仅处理视频，不包含音频播放。

## 项目结构

| 文件 | 职责 |
| --- | --- |
| `rtsp_player_api.h` | DLL 导出的 C API、播放器句柄和配置结构 |
| `capi.cpp` | 参数校验、API 到播放器实例的分发及错误码文本 |
| `registry.h/.cpp` | 线程安全的播放器注册表；检查句柄和流地址冲突 |
| `player.h/.cpp` | 播放器生命周期、拉流/解码线程、渲染线程及统计信息 |
| `media_source.h/.cpp` | FFmpeg 网络初始化、打开媒体源和读取数据包 |
| `decoder.h/.cpp` | 视频解码、动态像素格式转换以及 BGR24 帧生成 |
| `frame_queue.h/.cpp` | 解码帧与渲染之间的有界生产者—消费者队列 |
| `latency_control.h/.cpp` | 无全局状态的纯函数队列丢帧策略 |
| `renderer.h/.cpp` | 使用 GDI 将 BGR24 帧绘制到窗口 |
| `errors.h` | C API 错误码定义 |
| `CMakeLists.txt` | Windows DLL、C++20 和 FFmpeg 依赖配置 |

## 调用流程

1. 调用 `rtsp_player_create`，校验参数后同步打开流并创建解码器，成功后启动工作线程。
2. 拉流线程读取 FFmpeg 数据包，只将视频包送入解码器；解码出的帧转换为 BGR24 后进入有界帧队列。
3. 渲染线程从队列取帧并通过 GDI 绘制到指定窗口。队列达到时长上限时，可按配置丢弃旧帧以追赶实时画面，或跳过当前帧以保留队列中的旧帧。
4. 调用方可通过 `pause`、`resume`、`reconnect` 和 `get_stats` 控制或查询实例；`destroy` 注销实例并在其最后一个并发引用释放时停止线程、释放资源。

网络读取和解码在同一个生产线程执行，以避免额外的压缩包队列和线程切换；渲染使用独立线程，避免 GDI 阻塞拉流/解码。

## 构建

准备与目标架构一致的 FFmpeg **开发包**（头文件和 MSVC `.lib` 导入库），然后使用 Visual Studio CMake 生成器构建：

```powershell
cmake -S . -B build -A x64 -DFFMPEG_ROOT="C:\deps\ffmpeg"
cmake --build build --config Release
```

也可将 `FFMPEG_ROOT` 设置为环境变量；如果未配置且 `E:\ffmpeg` 中存在 FFmpeg 头文件，CMake 会自动使用该目录。CLion 可在 CMake Profile 的 CMake options 中设置 `-DFFMPEG_ROOT=C:/deps/ffmpeg`。生成的 `RtspPlayerDll.dll` 位于构建目录。部署时还需将匹配版本/架构的 FFmpeg 运行时 DLL 放在应用可搜索到的位置。当前工程面向 Windows；FFmpeg 的头文件、库和运行时必须来自兼容的构建。

## C# P/Invoke 使用

确保进程位数与 DLL/FFmpeg 一致，并在窗口控件创建后传入其 `Handle`。以下声明展示了原生配置结构的字段布局和 UTF-8 流地址传递方式：

```csharp
using System;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)]
struct RtspPlayerConfig
{
    public int max_queue_ms;
    public int reconnect_interval;
    [MarshalAs(UnmanagedType.I1)]
    public bool enable_drop_old_frame;
}

static class RtspPlayer
{
    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_create(
        ulong handle, IntPtr hwnd,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string url,
        ref RtspPlayerConfig config);

    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_pause(ulong handle);

    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_resume(ulong handle);

    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_reconnect(ulong handle);

    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_get_stats(
        ulong handle, out ulong frameCount, out double fps, out int queueMs);

    [DllImport("RtspPlayerDll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int rtsp_player_destroy(ulong handle);
}
```

最小调用顺序如下；在窗口销毁前释放播放器：

```csharp
var config = new RtspPlayerConfig
{
    max_queue_ms = 300,
    reconnect_interval = 2000,
    enable_drop_old_frame = true
};

ulong handle = 1; // 每个活动播放器使用唯一句柄
int result = RtspPlayer.rtsp_player_create(handle, videoControl.Handle, url, ref config);
if (result != 0)
    throw new InvalidOperationException($"播放器创建失败，错误码：{result}");

// 播放期间可调用 pause/resume/reconnect/get_stats。
// 窗口即将销毁时调用：
RtspPlayer.rtsp_player_destroy(handle);
```

C API 返回 `RtspErrorCode`，成功为 `RTSP_OK`（0）；错误码定义在 `errors.h`，`rtsp_get_error_string` 的导出声明位于 `rtsp_player_api.h`。`rtsp_player_get_stats` 返回累计成功渲染帧数、从启动起计算的平均 FPS，以及当前帧队列的估算时长。

## 配置与注意事项

- `handle` 必须在所有活动播放器中唯一；同一个流地址也不能同时创建多个实例。
- `hwnd` 必须是有效的 Windows 窗口句柄，并在播放器运行期间保持有效。渲染线程直接对该窗口执行 GDI 绘制。
- `max_queue_ms` 必须大于 0；`reconnect_interval` 不能小于 0，单位均为毫秒。帧时长根据视频帧率估算，未知帧率时按约 33 ms 估算。
- `enable_drop_old_frame = true` 时队列超限会移除较旧帧来降低延迟；为 `false` 时会跳过新帧，不主动丢弃已排队帧。队列至少允许容纳一帧，因此单帧时长大于上限时队列可能短暂超过配置值。
- `rtsp_player_create` 会同步打开网络流，耗时取决于连接和服务器响应。网络读取配置了约 10 秒超时，因此暂停或销毁遇到阻塞读取时可能需要等待读取超时。
- 创建成功后的异步网络、解码或渲染错误目前没有事件/回调接口；网络读取失败会按配置间隔自动重连。统计接口不提供连接状态。
- 当前输出为 CPU 解码后的 BGR24，并通过 GDI 绘制；尚未实现硬件解码、音频播放、窗口尺寸自适应或用户可配置的像素格式。
