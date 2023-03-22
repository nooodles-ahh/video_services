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
#ifdef _WIN32
	m_directSound = nullptr;
#endif
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
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
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
	return VideoSystem::EVideoSystem_t::NONE;
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
	if( videoSystem == VideoSystem_t::WEBM)
		return VideoSystemFeature_t::FULL_PLAYBACK;
	return VideoSystemFeature_t::NO_FEATURES;
}

VideoSystem_t CVideoServices::LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode )
{
	if ( Q_GetFileExtension( pFileName ) && !Q_strcmp(Q_GetFileExtension(pFileName), "webm") )
	{
		return VideoSystem_t::WEBM;
	}
	return VideoSystem_t::NONE;
}

VideoResult_t CVideoServices::LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode )
{
	// is this even a webm?
	if( !Q_GetFileExtension(pSearchFileName) || Q_strcmp( Q_GetFileExtension(pSearchFileName), "webm" ) )
		return VideoResult_t::VIDEO_SYSTEM_NOT_FOUND;

	if ( !g_pFullFileSystem->FileExists( pSearchFileName, pPathID ) )
	{
		return VideoResult_t::VIDEO_FILE_NOT_FOUND;
	}

	g_pFullFileSystem->RelativePathToFullPath( pSearchFileName, pPathID, pPlaybackFileName, fileNameMaxLen );
	return VideoResult_t::SUCCESS;
}

IVideoMaterial *CVideoServices::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID,
	VideoPlaybackFlags_t playbackFlags,
	VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
	// find playable file
	char file_name[MAX_PATH];
	Q_strncpy( file_name, pVideoFileName, sizeof( file_name ) );
	char playable_video_file[MAX_PATH];
	VideoResult_t found_video = LocatePlayableVideoFile( file_name, pPathID, nullptr, 
														playable_video_file, MAX_PATH );
													
	if( found_video != VideoResult_t::SUCCESS && !PlayAlternateIfNotAvailable && 
		( videoSystem != VideoSystem_t::DETERMINE_FROM_FILE_EXTENSION ) ) 
	{
		return nullptr;
	}

	// We didn't the file, try and look for a webm
	if( found_video != VideoResult_t::SUCCESS )
	{
		Q_SetExtension( file_name, "webm", sizeof( file_name ) );
		found_video = LocatePlayableVideoFile( file_name, pPathID, nullptr, 
												playable_video_file, MAX_PATH );
		// no file, very sad
		if(found_video != VideoResult_t::SUCCESS )
			return nullptr;
	}

	CVideoMaterial *pMaterial = new CVideoMaterial();
	// todo - return errors?
#ifdef _WIN32
	if ( !pMaterial->LoadVideo( pMaterialName, playable_video_file, m_directSound ) )
#else
	if ( !pMaterial->LoadVideo( pMaterialName, playable_video_file, nullptr ) )
#endif
	{
		delete pMaterial;
		return nullptr;
	}
	
	// believe it or not we might have more than one video running at a time
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

int	CVideoServices::GetUniqueMaterialID()
{
	return 0;
}

// Plays a given video file until it completes or the user presses ESC, SPACE, or ENTER
VideoResult_t CVideoServices::PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime,
	VideoPlaybackFlags_t playbackFlags,
	VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
#ifdef _WIN32
	DirectSoundCreate8( NULL, &m_directSound, NULL );
	m_directSound->SetCooperativeLevel( (HWND)mainWindow, DSSCL_PRIORITY );
#endif
	
	CVideoMaterial *video_material = (CVideoMaterial *)CreateVideoMaterial( "FullScreenVideo", pFileName, pPathID, playbackFlags, videoSystem, PlayAlternateIfNotAvailable );
	if ( !video_material )
	{
#ifdef _WIN32
		m_directSound->Release();
		m_directSound = nullptr;
#endif
		return VideoResult::EVideoResult_t::UNKNOWN_OPERATION;
	}

	bool previous_threading_state = materials->AllowThreading( false, 0 ); // 0x1e8
	
	float flVideoU, flVideoV;
	int nVideoW, nVideoH;
	video_material->GetVideoImageSize( &nVideoW, &nVideoH );
	video_material->GetVideoTexCoordRange( &flVideoU, &flVideoV );
	float flRightU = flVideoU - ( 1.0f / (float)nVideoW );
	float flBottomV = flVideoV - ( 1.0f / (float)nVideoH );

	CMatRenderContextPtr pRenderContext( materials );
#ifdef _WIN32
	MSG msg;
#endif

	while ( 1 )
	{
		// TODO - figure out OS independant window updating and input polling
#ifdef _WIN32
		while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) 
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		if ( GetAsyncKeyState( VK_ESCAPE ) < 0 || GetAsyncKeyState( VK_SPACE ) < 0 || GetAsyncKeyState( VK_RETURN ) < 0 ) 
			break;
#endif
		
		pRenderContext->DrawScreenSpaceRectangle( video_material->GetMaterial(), 0, 0, windowWidth, windowHeight, 0, 0, 
													nVideoW - 1, nVideoH - 1, nVideoW / flRightU, nVideoH / flBottomV );
		if ( !video_material->Update() )
			break;
		
		materials->SwapBuffers(); // 0xa0 
	}
	
	materials->AllowThreading( previous_threading_state, 0 ); // 0x1e8

	DestroyVideoMaterial( video_material );

#ifdef _WIN32
	m_directSound->Release();
	m_directSound = nullptr;
#endif
	return VideoResult::EVideoResult_t::SUCCESS;
}

// Sets the sound devices that the video will decode to
VideoResult_t CVideoServices::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData, VideoSystem_t videoSystem )
{
#ifdef _WIN32
	if ( operation == VideoSoundDeviceOperation_t::SET_DIRECT_SOUND_DEVICE )
	{
		m_directSound = (IDirectSound8 *)pDevice;

		FOR_EACH_VEC( m_vecVideos, vid )
		{
			m_vecVideos[vid]->SoundDeviceCommand( operation, m_directSound, pData );
		}
		
		return VideoResult::EVideoResult_t::SUCCESS;
	}
#endif
	return VideoResult::EVideoResult_t::SYSTEM_NOT_AVAILABLE;
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
	return VideoResult::EVideoResult_t::FEATURE_NOT_AVAILABLE;
}

IVideoRecorder *CVideoServices::CreateVideoRecorder( VideoSystem_t videoSystem )
{
	return nullptr;
}

VideoResult_t CVideoServices::DestroyVideoRecorder( IVideoRecorder *pVideoRecorder )
{
	return VideoResult::EVideoResult_t::SYSTEM_NOT_AVAILABLE;
}