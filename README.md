# Source 2013 Webm Video Services

A work in progress drop in replacement for Source 2013's video services with webm support, originally created for Hunt Down the Freeman. Supports VP8 and VP9 video, and Vorbis and Opus audio. Based on [libsimplewebm](https://github.com/zaps166/libsimplewebm) and [AVI Materials for Source](https://developer.valvesoftware.com/wiki/AVI_Materials) with a few glances at [Godot's Webm playback](https://github.com/godotengine/godot/blob/b1f5cee7d9a1f509ef8990f3b8405c74e83a20cc/modules/webm/video_stream_webm.cpp) (as that used libsimplewebm).

This is intended for standalone Source engine mods released on Steam, and not those requiring the seperate installation of the Source SDK base. It is possible to get video working after the client is loaded for sourcemods, but you won't have sound playback.

This currently only works on Windows. I'll figure out Linux when I feel like it.

# Encoding compatible webms
- make sure the pixel format is YUV420
- *TODO*
