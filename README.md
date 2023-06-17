# Webm Video Services

A work in progress drop-in replacement for Source SDK 2013's video services. Supports VP8 and VP9 video, and Vorbis and Opus audio. Based on [libsimplewebm](https://github.com/zaps166/libsimplewebm), [AVI Materials for Source](https://developer.valvesoftware.com/wiki/AVI_Materials) and [Godot's Webm playback](https://github.com/godotengine/godot/blob/b1f5cee7d9a1f509ef8990f3b8405c74e83a20cc/modules/webm/video_stream_webm.cpp)

This is intended for standalone Source engine mods released on Steam. This can be used for standard sourcemods but sound playback will not work, it's doable on Windows by creating a DirectSound interface object. See the beginning of the [PlayVideoFileFullScreen method](https://github.com/nooodles-ahh/video_services/blob/master/video_services/video_services.cpp#L222-L228).

Sound playback is currently only available on Windows. I'll figure it out for Linux when I feel like it.

# Rationale
The version of Source provided to modders only has support for Bink which isn't well suited for HD video, and Quicktime video if you're using Windows and install the long since abandoned Quicktime player.

At the time of writting I'm developer for the infamous _game_ Hunt Down the Freeman. The mod makes heavy use of cutscenes and as Bink is the only real option, that's what was used. This 
resulted in about 6.6GB of cutscenes of acceptable quality. The same videos encoded using VP9 and Opus only take up a little over 900MB. So about 13%-16% of the original size for equal 
or, in most cases, better quality.

# Encoding compatible webms
Peferably you want to be using VP9 and opus for the best results and ensure the pixel format is YUV420.
The easiest way to encode a compatiable webm is probably to use [WebmConverter](https://argorar.github.io/WebMConverter/). If you're using ffmpeg make sure to include the argument `-pix_fmt yuv420p` if you're not unsure of the source video is in the correct format.

# Building
- Add `$Include "video_services\vpc_scripts\projects.vgc"` to `vpc_scripts\default.vgc` in your mod.
- Include `video_services` in your project group
- Regenerate and build
- Copy vpx.dll and the resulting video_services.dll into the relevant bin folder. i.e. If you're a mod on Steam it would go in `Half-Life 2\bin`. And if you're not on Steam it would go in `sourcemods\hl2\bin`.

# Sourcemod usage
If you don't need audio you can use this in sourcemods by placing this block of code in `CHLClient::Init`. Anywhere after all the interfaces are connected, specifically I'd put it after the `WORKSHOP_IMPORT_ENABLED` block.
```cpp
// disconnect the original video services
if ( g_pVideo )
{
	g_pVideo->Shutdown();
	g_pVideo->Disconnect();
	g_pVideo = nullptr;
}

// get video_services.dll from our game's bin folder
char video_service_path[MAX_PATH];
Q_snprintf( video_service_path, sizeof( video_service_path ), "%s\\bin\\video_services.dll", engine->GetGameDirectory() );

CSysModule *video_services_module = Sys_LoadModule( video_service_path );
if ( video_services_module != nullptr )
{
	CreateInterfaceFn VideoServicesFactory = Sys_GetFactory( video_services_module );
	if ( VideoServicesFactory )
	{
		g_pVideo = (IVideoServices *)VideoServicesFactory( VIDEO_SERVICES_INTERFACE_VERSION, NULL );
		if ( g_pVideo != nullptr )
		{
			g_pVideo->Connect( appSystemFactory );
		}
	}
}
```

If you needed the old video services still, you could make a new global specfically for this library, and just use that when applicable.

# Issues
- All video service features are not present such as video recording
- Not everything works identically to BINK videos such as when the video will pause and audio volume
- The sound system is shutdown before video services is, so you may crash on shutdown depending on what you're doing
- No audio playback on Linux (yet)
- I don't know how to write safe threaded code
- Depending on the state of your source dll vpc's it might not like compiling in debug