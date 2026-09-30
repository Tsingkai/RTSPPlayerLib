cmake_minimum_required(VERSION 3.20)
project(RtspPlayerDll LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 关闭编译器扩展，保证标准C++

# 编译为Windows DLL
add_library(RtspPlayerDll SHARED
    capi.cpp
    registry.cpp
    frame_queue.cpp
    latency_control.cpp
    media_source.cpp
    decoder.cpp
    player.cpp
    renderer.cpp
)

# MSVC编译选项
if(MSVC)
    target_compile_options(RtspPlayerDll PRIVATE "/utf-8")
    target_compile_definitions(RtspPlayerDll PRIVATE RTSPPLAYER_EXPORTS)

    # 推荐：开启多线程编译
    target_compile_options(RtspPlayerDll PRIVATE "/MP")
endif()

# DLL导出符号定义
target_compile_definitions(RtspPlayerDll PRIVATE RTSP_PLAYER_DLL_EXPORT)

######################## FFmpeg 现代CMake查找配置 ########################
# 配置时通过 -DFFMPEG_ROOT 指定 FFmpeg 解压根目录。
set(FFMPEG_ROOT "" CACHE PATH "Root directory of the FFmpeg development package")
if(NOT FFMPEG_ROOT AND DEFINED ENV{FFMPEG_ROOT})
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()
if(NOT FFMPEG_ROOT AND EXISTS "E:/ffmpeg/include/libavformat/avformat.h")
    set(FFMPEG_ROOT "E:/ffmpeg")
endif()
if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR
        "FFmpeg development files were not found. Set FFMPEG_ROOT to the package directory "
        "using -DFFMPEG_ROOT=<path> or the FFMPEG_ROOT environment variable.")
endif()

# 查找FFmpeg头文件
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h
    PATHS "${FFMPEG_ROOT}/include"
    NO_DEFAULT_PATH
)
find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h
    PATHS "${FFMPEG_ROOT}/include"
    NO_DEFAULT_PATH
)
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h
    PATHS "${FFMPEG_ROOT}/include"
    NO_DEFAULT_PATH
)
find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h
    PATHS "${FFMPEG_ROOT}/include"
    NO_DEFAULT_PATH
)

# 查找FFmpeg导入库
find_library(AVFORMAT_LIBRARY avformat
    PATHS "${FFMPEG_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(AVCODEC_LIBRARY avcodec
    PATHS "${FFMPEG_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(AVUTIL_LIBRARY avutil
    PATHS "${FFMPEG_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(SWSCALE_LIBRARY swscale
    PATHS "${FFMPEG_ROOT}/lib"
    NO_DEFAULT_PATH
)

# 校验FFmpeg是否全部找到，缺失直接报错
if(NOT AVFORMAT_INCLUDE_DIR OR NOT AVCODEC_INCLUDE_DIR OR NOT AVUTIL_INCLUDE_DIR OR NOT SWSCALE_INCLUDE_DIR)
    message(FATAL_ERROR "FFmpeg include headers not found! Check FFMPEG_ROOT path: ${FFMPEG_ROOT}")
endif()
if(NOT AVFORMAT_LIBRARY OR NOT AVCODEC_LIBRARY OR NOT AVUTIL_LIBRARY OR NOT SWSCALE_LIBRARY)
    message(FATAL_ERROR "FFmpeg lib files not found! Check FFMPEG_ROOT path: ${FFMPEG_ROOT}")
endif()

# 绑定头文件路径到目标（仅RtspPlayerDll可见）
target_include_directories(RtspPlayerDll PRIVATE
    ${AVFORMAT_INCLUDE_DIR}
    ${AVCODEC_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${SWSCALE_INCLUDE_DIR}
)

# 绑定FFmpeg库链接到目标
target_link_libraries(RtspPlayerDll PRIVATE
    ${AVFORMAT_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
)
