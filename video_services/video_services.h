#ifndef VIDEO_SERVICES_H
#define VIDEO_SERVICES_H
#ifdef _WIN32
#pragma once
#endif

#include "tier3/tier3.h"
#include "video/ivideoservices.h"
#include "utlvector.h"
#ifdef _WIN32
#include <Windows.h>
#include "dsound.h"
#endif

//---------------------------------------------------------
// Video Services
//---------------------------------------------------------
class CVideoServices : public CTier3AppSystem< IVideoServices >
{
	typedef CTier3AppSystem< IVideoServices > BaseClass;

public:

	CVideoServices();
	~CVideoServices();

	//---------------------------------------------------------
	// Initialization and shutdown
	//---------------------------------------------------------

	//
	// IAppSystem
	//
	virtual bool							Connect( CreateInterfaceFn factory );
	virtual void							Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

	// these will come from the engine
	virtual InitReturnVal_t					Init();
	virtual void							Shutdown();

	//---------------------------------------------------------
	// IVideoServices implementation
	//---------------------------------------------------------
public:
	// Query the available video systems
	virtual int						GetAvailableVideoSystemCount();
	virtual VideoSystem_t			GetAvailableVideoSystem( int n );

	virtual bool					IsVideoSystemAvailable( VideoSystem_t videoSystem );
	virtual VideoSystemStatus_t		GetVideoSystemStatus( VideoSystem_t videoSystem );
	virtual VideoSystemFeature_t	GetVideoSystemFeatures( VideoSystem_t videoSystem );
	virtual const char *GetVideoSystemName( VideoSystem_t videoSystem );

	virtual VideoSystem_t			FindNextSystemWithFeature( VideoSystemFeature_t features, VideoSystem_t startAfter = VideoSystem::NONE );

	virtual VideoResult_t			GetLastResult();

	// deal with video file extensions and video system mappings
	virtual	int						GetSupportedFileExtensionCount( VideoSystem_t videoSystem );
	virtual const char *GetSupportedFileExtension( VideoSystem_t videoSystem, int extNum = 0 );
	virtual VideoSystemFeature_t    GetSupportedFileExtensionFeatures( VideoSystem_t videoSystem, int extNum = 0 );

	virtual	VideoSystem_t			LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode = VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL );
	virtual VideoResult_t			LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode = VideoSystemFeature::FULL_PLAYBACK );

	// Create/destroy a video material
	virtual IVideoMaterial *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID = nullptr,
		VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS,
		VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true );

	virtual VideoResult_t			DestroyVideoMaterial( IVideoMaterial *pVideoMaterial );
	virtual int						GetUniqueMaterialID();

	// Create/destroy a video encoder		
	virtual VideoResult_t			IsRecordCodecAvailable( VideoSystem_t videoSystem, VideoEncodeCodec_t codec );

	virtual IVideoRecorder *CreateVideoRecorder( VideoSystem_t videoSystem );
	virtual VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pVideoRecorder );

	// Plays a given video file until it completes or the user presses ESC, SPACE, or ENTER
	virtual VideoResult_t			PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime,
		VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_FULLSCREEN_OPTIONS,
		VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true );

	// Sets the sound devices that the video will decode to
	virtual VideoResult_t			SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr, VideoSystem_t videoSystem = VideoSystem::ALL_VIDEO_SYSTEMS );

	// Get the (localized) name of a codec as a string
	virtual const wchar_t *GetCodecName( VideoEncodeCodec_t nCodec );

private:
	//IVideoMaterial *m_pMaterial;
	CUtlVector< IVideoMaterial *> m_vecVideos;
#ifdef _WIN32
	IDirectSound8 *m_directSound;
#endif
};
#endif