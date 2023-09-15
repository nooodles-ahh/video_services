# Webm Video Services
A functional work in progress drop-in replacement for Source SDK 2013's video services to play codecs commonly stored in webms. This is based on work from [libsimplewebm](https://github.com/zaps166/libsimplewebm), [AVI Materials for Source](https://developer.valvesoftware.com/wiki/AVI_Materials) and [Godot's Webm playback](https://github.com/godotengine/godot/blob/b1f5cee7d9a1f509ef8990f3b8405c74e83a20cc/modules/webm/video_stream_webm.cpp)

This is intended for standalone Source engine mods released on Steam. This can be used for standard sourcemods but sound playback will not work out of the box, you can create a new DirectSound interface object on Windows, see the [PlayVideoFileFullScreen method](https://github.com/nooodles-ahh/video_services/blob/master/video_services/video_services.cpp#L222-L228) on how you might do that.

# Rationale
The version of Source provided to modders on Windows and Linux only has support for Bink video playback, which isn't well suited for HD video. There is also the issue of licensing as Bink is a proprietary codec, so despite it being provided with Source you're not actually allowed to use it in mods on Steam unless you acquire a license.

The primary reason for having written this is that at the time of writing I'm a developer for the infamous _game_ Hunt Down the Freeman. The game makes heavy use of cutscenes and as Bink was the only option, that's what was used. This resulted in about 6.6GB of video of accept quality, for an hour's worth of content. The same content encoded using VP9 and Opus only takes up a little over 900MB. So about 13%-16% of the original size for equal or, in most cases, better quality. The only instance of lower quality I noticed was in dark scenes, but that's likely due to the absurd bitrate initially used for the Bink versions.

# Support
<table>
	<tr>
		<td></td>
		<td colspan="3"><b>Video</b></td>
		<td colspan="2"><b>Audio</b></td>
	</tr>
	<tr>
		<td><b>Codec</b></td>
		<td><b>VP8</b></td>
		<td><b>VP9</b></td>
		<td><b>AV1</b></td>
		<td><b>Vorbis</b></td>
		<td><b>Opus</b></td>
	</tr>
	<tr>
		<td><b>Supported?</b></td>
		<td>:white_check_mark:</td>
		<td>:white_check_mark:</td>
		<td>:x:</td>
		<td>:white_check_mark:</td>
		<td>:white_check_mark:</td>
	</tr>
</table>

I may support AV1 in the future as I have been asked about.

**Linux requires SDL 2.0.7 or later. Source SDK Base 2013 MP comes with 2.0.4, you'll need to use something newer, like the one that comes with Steam.**

I have no plans to support anything other than Linux and Windows but it may incidentally become usable on other platforms that make use of SDL2.

# Encoding compatible webms
The easiest way to encode a compatiable webm is probably to use [WebmConverter](https://argorar.github.io/WebMConverter/), as encoding webm's is what it's designed to do. 
As it is 2023, you will probably want to be using VP9 and Opus for the best results, you will also need to ensure that the pixel format is YUV420 as other formats are not currently supported. WebmConverter does this by default, but you need to make sure this is done if you're using another encoding program such as FFmpeg or HandBrake.

# Building
- Add `$Include "video_services\vpc_scripts\projects.vgc"` to `vpc_scripts\default.vgc` in your mod.
- Include `video_services` in your project group
- Regenerate and build
- Copy vpx.dll and the resulting video_services.dll into the relevant bin folder

# TODO
- The sound system is shutdown before video services is, which may result in a crash if you're doing something like a main menu background video
- Audio resampling on Linux so Opus can work properly
- Support for other pixel formats

# Issues I won't fix
- All video service features are not present such as video recording
- Not everything works identically to Bink video and Valve's implementation, notable examples are when audio stops on dragging the game window, and exact video audio volume
- Some videos may not work as expected if they are weird in any way, such as having a varible framerate or resolution changes, or may desync or stutter at abnormally low framerates

# Sourcemod usage
If you don't need audio or have created a new DirectSound object you can use this in sourcemods by placing this block of code in `CHLClient::Init`. Anywhere after all the interfaces are connected, I'd probably put it after the `WORKSHOP_IMPORT_ENABLED` block.
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

If you needed the old video services still, I would recommend instantiating a new video services object specfically for the library, and only use it when needed.
