//===========================================================================//
//
// Purpose: Webm capable video_services replacement
// Written by: Noodles
// Utilising Błażej Szczygieł's libsimplewebm
//
//===========================================================================//

#include "video_services.h"
#include "video_material.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CVideoServices g_pVideoServices;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVideoServices, CVideoServices,
	VIDEO_SERVICES_INTERFACE_VERSION, g_pVideoServices );

// --------------------------------------------------------------------
// construction/destruction
// --------------------------------------------------------------------
CVideoServices::CVideoServices()
{
	m_pSoundDevice = nullptr;
	m_iUniqueID = 0;
}

CVideoServices::~CVideoServices()
{
}

// --------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------
bool CVideoServices::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
		return false;

	if ( !BaseClass::Connect( factory ) )
		return false;

	return true;
}


// --------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------
void CVideoServices::Disconnect()
{
	BaseClass::Disconnect();
}


// --------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------
void *CVideoServices::QueryInterface( const char *pInterfaceName )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();
	return factory( pInterfaceName, NULL );
}


// --------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------
InitReturnVal_t	CVideoServices::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	return INIT_OK;
}


// --------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------
void CVideoServices::Shutdown()
{
	BaseClass::Shutdown();
}

int	CVideoServices::GetAvailableVideoSystemCount()
{
	return 1;
}

VideoSystem_t CVideoServices::GetAvailableVideoSystem( int n )
{
	return VideoSystem::EVideoSystem_t::WEBM;
}

bool CVideoServices::IsVideoSystemAvailable( VideoSystem_t videoSystem )
{
	return videoSystem == VideoSystem_t::WEBM;
}

VideoSystemStatus_t	CVideoServices::GetVideoSystemStatus( VideoSystem_t videoSystem )
{
	if ( videoSystem == VideoSystem_t::WEBM )
		return VideoSystemStatus::EVideoSystemStatus_t::OK;
	return VideoSystemStatus::EVideoSystemStatus_t::NOT_INSTALLED;
}

VideoSystemFeature_t CVideoServices::GetVideoSystemFeatures( VideoSystem_t videoSystem )
{
	return VideoSystemFeature::EVideoSystemFeature_t::FULL_PLAYBACK;
}

const char *CVideoServices::GetVideoSystemName( VideoSystem_t videoSystem )
{
	return "Webm";
}

VideoSystem_t CVideoServices::FindNextSystemWithFeature( VideoSystemFeature_t features, VideoSystem_t startAfter )
{
	return VideoSystem_t::NONE;
}

VideoResult_t CVideoServices::GetLastResult()
{
	return VideoResult::EVideoResult_t::SYSTEM_NOT_AVAILABLE;
}

int	CVideoServices::GetSupportedFileExtensionCount( VideoSystem_t videoSystem )
{
	return 1;
}

const char *CVideoServices::GetSupportedFileExtension( VideoSystem_t videoSystem, int extNum )
{
	return "webm";
}

VideoSystemFeature_t CVideoServices::GetSupportedFileExtensionFeatures( VideoSystem_t videoSystem, int extNum )
{
	if ( videoSystem == VideoSystem_t::WEBM )
		return VideoSystemFeature_t::FULL_PLAYBACK;
	return VideoSystemFeature_t::NO_FEATURES;
}

VideoSystem_t CVideoServices::LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode )
{
	if ( Q_GetFileExtension( pFileName ) && !Q_strcmp( Q_GetFileExtension( pFileName ), "webm" ) )
	{
		return VideoSystem_t::WEBM;
	}
	return VideoSystem_t::NONE;
}

VideoResult_t CVideoServices::LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode )
{
	// is this even a webm?
	if( LocateVideoSystemForPlayingFile(pSearchFileName, playMode) == VideoSystem_t::NONE )
		return VideoResult_t::VIDEO_SYSTEM_NOT_FOUND;

	if ( !g_pFullFileSystem->FileExists( pSearchFileName, pPathID ) )
		return VideoResult_t::VIDEO_FILE_NOT_FOUND;

	g_pFullFileSystem->RelativePathToFullPath( pSearchFileName, pPathID, pPlaybackFileName, fileNameMaxLen );
	return VideoResult_t::SUCCESS;
}

IVideoMaterial *CVideoServices::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID,
	VideoPlaybackFlags_t playbackFlags, VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
	char playable_video_file[MAX_PATH];
	char file_name[MAX_PATH];
	Q_strncpy( file_name, pVideoFileName, sizeof( file_name ) );
	// just look for a webm, there's nothing else.
	Q_SetExtension( file_name, "webm", sizeof( file_name ) );

	// find playable file
	VideoResult_t playable_file = LocatePlayableVideoFile( file_name, pPathID, nullptr,
		playable_video_file, MAX_PATH );

	if ( playable_file != VideoResult_t::SUCCESS )
		return nullptr;

	CVideoMaterial *pMaterial = new CVideoMaterial();
	if ( !pMaterial->LoadVideo( pMaterialName, playable_video_file, m_pSoundDevice ) )
	{
		delete pMaterial;
		return nullptr;
	}

	// We may have more than one video playing at a time
	m_vecVideos.AddToTail( pMaterial );
	return pMaterial;
}

VideoResult_t CVideoServices::DestroyVideoMaterial( IVideoMaterial *pVideoMaterial )
{
	int idx = m_vecVideos.Find( pVideoMaterial );
	if ( idx != -1 )
	{
		delete pVideoMaterial;
		m_vecVideos.Remove( idx );
		return VideoResult_t::SUCCESS;
	}
	return VideoResult_t::MATERIAL_NOT_FOUND;
}

// I don't know if this is ever called anywhere
int	CVideoServices::GetUniqueMaterialID()
{
	return m_iUniqueID++;
}

// Plays a given video file until it completes or the user presses ESC, SPACE, or ENTER
VideoResult_t CVideoServices::PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime,
	VideoPlaybackFlags_t playbackFlags,
	VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
#ifdef _WIN32
	HRESULT result = DirectSoundCreate8( NULL, &m_pSoundDevice, NULL );
	if ( result != DS_OK )
		return VideoResult_t::AUDIO_ERROR_OCCURED;

	m_pSoundDevice->SetCooperativeLevel( (HWND)mainWindow, DSSCL_PRIORITY );
#endif

	// TODO - do this properly so it can return a proper error
	CVideoMaterial *videoMaterial = (CVideoMaterial *)CreateVideoMaterial( "FullScreenVideo", pFileName, pPathID, playbackFlags, videoSystem, PlayAlternateIfNotAvailable );
	if ( !videoMaterial )
	{
#ifdef _WIN32
		m_pSoundDevice->Release();
#endif
		m_pSoundDevice = nullptr;
		return VideoResult_t::VIDEO_FILE_NOT_FOUND;
	}

	float flU, flV;
	int nVideoWidth, nVideoHeight;
	videoMaterial->GetVideoImageSize( &nVideoWidth, &nVideoHeight );
	videoMaterial->GetVideoTexCoordRange( &flU, &flV );
	float flRightU = flU - ( 1.0f / (float)nVideoWidth );
	float flBottomV = flV - ( 1.0f / (float)nVideoHeight );

	// get the ratio of the video so we don't stretch it out
	float flFrameRatio = ( (float)windowWidth / (float)windowHeight );
	float flVideoRatio = ( (float)nVideoWidth / (float)nVideoHeight );

	int nPlaybackWidth = windowWidth;
	int nPlaybackHeight = windowHeight;
	int x, y;
	x = y = 0;

	if ( flVideoRatio > flFrameRatio )
	{
		nPlaybackWidth = windowWidth;
		nPlaybackHeight = ( windowWidth / flVideoRatio );
		y = ( windowHeight - nPlaybackHeight ) / 2;
	}
	else if ( flVideoRatio < flFrameRatio )
	{
		nPlaybackWidth = ( windowHeight * flVideoRatio );
		nPlaybackHeight = windowHeight;
		x = ( windowWidth - nPlaybackWidth ) / 2;
	}

	CMatRenderContextPtr pRenderContext( materials );

	bool prevThreadingState = materials->AllowThreading( false, 0 );

	while ( 1 )
	{
#ifdef WIN32
		MSG msg;
		while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		if ( GetAsyncKeyState( VK_ESCAPE ) < 0 || GetAsyncKeyState( VK_SPACE ) < 0 || GetAsyncKeyState( VK_RETURN ) < 0 )
			break;
#endif

		// offset x1 and y1 by -1 so you don't see any bleeding. I've probably messed up something somewhere
		pRenderContext->DrawScreenSpaceRectangle( videoMaterial->GetMaterial(), x, y, nPlaybackWidth, nPlaybackHeight, 0, 0,
			nVideoWidth - 1, nVideoHeight - 1, nVideoWidth / flRightU, nVideoHeight / flBottomV );

		// video finished?
		if ( !videoMaterial->Update() )
			break;

		materials->SwapBuffers();
	}

	materials->AllowThreading( prevThreadingState, 0 );

	DestroyVideoMaterial( videoMaterial );

	// clear so incase we have another video of a differing aspect ratio
	pRenderContext->ClearBuffers( true, false, false );

#ifdef _WIN32
	m_pSoundDevice->Release();
#endif
	m_pSoundDevice = nullptr;
	return VideoResult_t::SUCCESS;
}

// Sets the sound devices that the video will decode to
VideoResult_t CVideoServices::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData, VideoSystem_t videoSystem )
{
#ifdef _WIN32
	if ( operation == VideoSoundDeviceOperation_t::SET_DIRECT_SOUND_DEVICE )
	{
		m_pSoundDevice = (IDirectSound8 *)pDevice;

		FOR_EACH_VEC( m_vecVideos, vid )
		{
			m_vecVideos[vid]->SoundDeviceCommand( operation, m_pSoundDevice, pData );
		}

		return VideoResult_t::SUCCESS;
	}
#elif _LINUX

	// Maybe called when changing audio device?
	if( operation == VideoSoundDeviceOperation_t::SET_SDL_SOUND_DEVICE )
	{
		ConMsg("\nSDL Sound Device Set\n\n");
	}
	// Called on start up and sound restart
	else if( operation == VideoSoundDeviceOperation_t::SET_SDL_PARAMS )
	{
		FOR_EACH_VEC( m_vecVideos, vid )
			m_vecVideos[vid]->SoundDeviceCommand( operation, pDevice, pData );
	}
	else if( operation == VideoSoundDeviceOperation_t::SDLMIXER_CALLBACK )
	{
		FOR_EACH_VEC( m_vecVideos, vid )
			m_vecVideos[vid]->SoundDeviceCommand( operation, pDevice, pData );
	}
#endif
	return VideoResult_t::SYSTEM_NOT_AVAILABLE;
}

// Get the (localized) name of a codec as a string
const wchar_t *CVideoServices::GetCodecName( VideoEncodeCodec_t nCodec )
{
	return L"";
}

// ==============================================================
// Currently don't support recording (Probably never will)
// ==============================================================
VideoResult_t CVideoServices::IsRecordCodecAvailable( VideoSystem_t videoSystem, VideoEncodeCodec_t codec )
{
	return VideoResult_t::FEATURE_NOT_AVAILABLE;
}

IVideoRecorder *CVideoServices::CreateVideoRecorder( VideoSystem_t videoSystem )
{
	return nullptr;
}

VideoResult_t CVideoServices::DestroyVideoRecorder( IVideoRecorder *pVideoRecorder )
{
	return VideoResult_t::SYSTEM_NOT_AVAILABLE;
}