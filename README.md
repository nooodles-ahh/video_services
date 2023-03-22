# Webm Video Services

A work in progress drop-in replacement for Source SDK 2013's video services. Supports VP8 and VP9 video, and Vorbis and Opus audio. Based on [libsimplewebm](https://github.com/zaps166/libsimplewebm), [AVI Materials for Source](https://developer.valvesoftware.com/wiki/AVI_Materials) and [Godot's Webm playback](https://github.com/godotengine/godot/blob/b1f5cee7d9a1f509ef8990f3b8405c74e83a20cc/modules/webm/video_stream_webm.cpp)

This is intended for standalone Source engine mods released on Steam but can be used in a more limited capcity with sourcemods. Sound playback on for sourcemods on Windows is doable but not without some minor modifications.

Sound playback is currently only available on Windows. I'll figure it out for Linux when I feel like it.

# Encoding compatible webms
Peferably you want to be using VP9 and opus for the best results. Also ensure the pixel format is YUV420.
The easiest way to encode a compatiable webm is probably to use [WebmConverter](https://argorar.github.io/WebMConverter/). If you're using ffmpeg make sure you include `-pix_fmt yuv420p` if you're not sure the source video is in the correct format.

# Building
***TODO***
