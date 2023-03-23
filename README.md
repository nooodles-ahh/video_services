# Webm Video Services

A work in progress drop-in replacement for Source SDK 2013's video services. Supports VP8 and VP9 video, and Vorbis and Opus audio. Based on [libsimplewebm](https://github.com/zaps166/libsimplewebm), [AVI Materials for Source](https://developer.valvesoftware.com/wiki/AVI_Materials) and [Godot's Webm playback](https://github.com/godotengine/godot/blob/b1f5cee7d9a1f509ef8990f3b8405c74e83a20cc/modules/webm/video_stream_webm.cpp)

This is intended for standalone Source engine mods released on Steam but can be used in a more limited capcity with sourcemods. Sound playback on for sourcemods on Windows is doable but not without some minor modifications.

Sound playback is currently only available on Windows. I'll figure it out for Linux when I feel like it.

# Rational
The version of Source provided to modders only has support for Bink which isn't well suited for HD video, and Quicktime video if you're using Windows and install the long since abandoned Quicktime player. 

At the time of writting I'm a developer attempting to improve the infamous mod Hunt Down the Freeman. The mod makes heavy use of cutscenes and as Bink is the only real option, that's what was used. This resulted in about 6.6GB of cutscenes of acceptable quality. The same videos encoded using VP9 and Opus only take up a little over 900MB. So about 13%-16% of the original size for equal or, in most cases, better quality. 

# Encoding compatible webms
Peferably you want to be using VP9 and opus for the best results. Also ensure the pixel format is YUV420.
The easiest way to encode a compatiable webm is probably to use [WebmConverter](https://argorar.github.io/WebMConverter/). If you're using ffmpeg make sure you include `-pix_fmt yuv420p` if you're not sure the source video is in the correct format.

# Building
***TODO***
- Add `$Include "video_services\vpc_scripts\projects.vgc"` to `vpc_scripts\default.vgc` in your mod.
- Include `video_services` in your project group
- Regenerate and build
- Copy vpx.dll and the resulting video_services.dll into your root bin folder. i.e. `Half-Life 2\bin` **not** `Half-life 2\hl2\bin`

# Problems
- Feature parity isn't 1:1
- The sound system is shutdown before video services is, so you might get a crash on shutdown depending how you're using it
- I don't know how to write thread safe code
