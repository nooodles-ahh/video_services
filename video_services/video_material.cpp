//===========================================================================//
//
// Purpose: Webm capable video_services replacement
// Written by: Noodles
// Utilising Błażej Szczygieł's libsimplewebm
//
//===========================================================================//

#include "video_material.h"
#include "video_services.h"
#include "materialsystem/imaterial.h"
#include "filesystem.h"
#include "tier0/platform.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
// 
// Video texture regenerator
// 
// Notes: Bik shader only supports YUV420, I might eventually support more 
//			video colour formats but I will need to write the shaders
// 
//=============================================================================
template <YUVChannel_t Channel>
void CYUVTextureRegenerator<Channel>::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
{
	unsigned char *imageData = pVTFTexture->ImageData();
	int rowSize = pVTFTexture->RowSizeInBytes( 0 );

	if ( m_decodedImage && m_decodedImage->chromaShiftW == 1 && m_decodedImage->chromaShiftH == 1 )
	{
		unsigned char *pixels = m_decodedImage->planes[ Channel ];
		int lineSize = m_decodedImage->linesize[ Channel ];
		for ( int y = 0; y < m_videoHeight; ++y )
		{
			V_memcpy( imageData, pixels, m_videoWidth );
			imageData += rowSize;
			pixels += lineSize;
		}
		m_decodedImage = nullptr;
	}
}

//=============================================================================
// 
// Video material
// 
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: a paranoid amount of initialisation
//-----------------------------------------------------------------------------
CVideoMaterial::CVideoMaterial()
{
	m_mkvReader = nullptr;
	m_demuxer = nullptr;
	m_videoDecoder = nullptr;
	m_audioDecoder = nullptr;
	m_image = new VPXDecoder::Image();
	m_pcm = nullptr;

	m_videoWidth = 0;
	m_videoHeight = 0;
	m_textureWidth = 0;
	m_textureHeight = 0;

	m_videoReady = false;
	m_videoPlaying = false;
	m_videoStarted = false;
	m_videoLooping = false;
	m_videoStopped = false;
	m_videoEnded = false;

	m_soundKilled = false;

	m_videoPath[0] = '\0';

	m_volume = 1.0f;
	m_videoTime = 0.0;
	m_curTime = 0.0;
	m_prevTicks = 0;

	m_currentFrame = 0;

	m_yTextureRegen = nullptr;
	m_crTextureRegen = nullptr;
	m_cbTextureRegen = nullptr;

	m_nBytesPerSample = 0;
	m_nAudioBufferFilledSize = 0;

	m_pAudioBuffer = nullptr;
#ifdef _LINUX
	m_pSDLAudioStream = nullptr;
#elif _WIN32
	m_nAudioBufferSize = 0;
	m_nAudioBufferWriteOffset = 0;
#endif
	m_awaitingUpdate = false;
}

CVideoMaterial::~CVideoMaterial()
{
	m_videoEnded = true;
	m_videoStopped = true;
	//m_videoFrames.Purge();
	// for each frame in the queue free the buffer
	while (m_videoFrames.Count() > 0)
	{
		WebMFrame frame = m_videoFrames.RemoveAtHead();
		if ( frame.buffer )
			free( frame.buffer );
	}

	DestroySoundBuffer();

	IMaterial* material = m_videoMaterial;
	m_videoMaterial.Shutdown();
	// Removes any material that might reference the video texture
	if( materials )
		materials->UncacheUnusedMaterials();

	// kill it if it remains
	if ( material )
		material->DeleteIfUnreferenced();

	if (m_crTexture.IsValid())
		m_crTexture.Shutdown(true);
	if (m_cbTexture.IsValid())
		m_cbTexture.Shutdown(true);
	if (m_yTexture.IsValid())
		m_yTexture.Shutdown(true);

	delete m_pcm;
	delete m_image;
	delete m_audioDecoder;
	delete m_videoDecoder;
	delete m_demuxer;
	delete m_mkvReader;
}

bool CVideoMaterial::LoadVideo( const char *pMaterialName, const char *pVideoFileName, void *pSoundDevice )
{
	// Open the video file
	m_mkvReader = new MkvReader(pVideoFileName);
	if (!m_mkvReader->Loaded())
	{
		ConWarning("Failed to open video file.\n");
		goto failed;
	}
	V_strncpy( m_videoPath, pVideoFileName, sizeof( m_videoPath ) );

	// Initialise the demuxer with our mkv reader
	m_demuxer = new WebMDemuxer( m_mkvReader );
	if (!m_demuxer->isOpen())
	{
		ConWarning("Failed to open video file for demuxing.\n");
		goto failed;
	}
	
	// assign the decoder a reasonable number of threads
	const CPUInformation& cpuInfo = *GetCPUInformation();
	unsigned int numthreads = clamp( cpuInfo.m_nLogicalProcessors - 2, 1, 8 );
	m_videoDecoder = new VPXDecoder( *m_demuxer, numthreads );
	if (!m_videoDecoder->isOpen())
	{
		ConWarning("Failed to open VPX decoder\n");
		goto failed;
	}

	// TODO - should these be cached?
	m_videoWidth = m_demuxer->getWidth();
	m_videoHeight = m_demuxer->getHeight();

	// audio is optional so we don't need to error out if it fails
	m_audioDecoder = new OpusVorbisDecoder( *m_demuxer );
	if (m_audioDecoder->isOpen())
	{
		m_pcm = new short[m_audioDecoder->getBufferSamples() * m_demuxer->getChannels()];
		if(!CreateSoundBuffer(pSoundDevice))
			ConWarning("Failed to create sound buffer. Playing video anyway.\n");
	}

	// This is a guessed framerate from the first 50 frames
	// TODO - remove this
	m_frameRate.SetFPS( m_demuxer->getFrameRate() ); 

	CreateVideoMaterial();
	return true;

	// cleanup
failed:
	if(m_videoDecoder)
		delete m_videoDecoder;
	if (m_demuxer)
		delete m_demuxer;
	if (m_mkvReader)
		delete m_mkvReader;

	return false;
}

bool CVideoMaterial::CreateSoundBuffer( void* pSoundDevice )
{
	if ( !m_audioDecoder->isOpen() )
		return false;

	if ( !pSoundDevice )
	{
		DevMsg( "No sound device!\n" );
		return false;
	}

#ifdef _LINUX
	// todo; Error checking

	// this is a copy recieved from services so we don't need to allocate it
	m_pAudioDevice = ( SDL_AudioSpec* )pSoundDevice;
	m_nBytesPerSample = m_pAudioDevice->channels * ( SDL_AUDIO_BITSIZE( m_pAudioDevice->format ) / 8 );

	// if SDL2 version is greater or equal to 2.0.7 enable SDL_AudioStream
	SDL_version ver;
	SDL_GetVersion( &ver );
	if ( !SDL_VERSION_ATLEAST( 2, 0, 7 ) )
	{
		ConColorMsg( Color( 255, 0, 0, 255 ), "SDL must be version 2.0.7 or higher in order to playback video sound!\n" );
	}
	else
	{
		m_pAudioBuffer = new Uint8[ m_pAudioDevice->size ];
		m_pSDLAudioStream = SDL_NewAudioStream( AUDIO_S16, m_demuxer->getChannels(), m_demuxer->getSampleRate(),
		m_pAudioDevice->format, m_pAudioDevice->channels, m_pAudioDevice->freq );
	}

	return true;
#elif _WIN32
	IDirectSound* pDirectSound = ( IDirectSound* )pSoundDevice;

	WAVEFORMATEX waveFormat{};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = m_demuxer->getChannels();
	waveFormat.nSamplesPerSec = m_demuxer->getSampleRate();
	waveFormat.wBitsPerSample = 16; // S16
	waveFormat.nBlockAlign = ( waveFormat.nChannels * waveFormat.wBitsPerSample ) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	DSBUFFERDESC dsbd{};
	dsbd.dwSize = sizeof( DSBUFFERDESC );
	dsbd.dwBufferBytes = waveFormat.nAvgBytesPerSec * 2.f;
	dsbd.lpwfxFormat = &waveFormat;

	// if we have the losefocus cvar determine if we want to remove the global focus flag
	dsbd.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_LOCSOFTWARE | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
	ConVarRef snd_mute_losefocus( "snd_mute_losefocus" );
	if ( snd_mute_losefocus.GetBool() )
		dsbd.dwFlags = dsbd.dwFlags & ~( DSBCAPS_GLOBALFOCUS );

	m_nAudioBufferSize = dsbd.dwBufferBytes;
	m_nBytesPerSample = waveFormat.nBlockAlign;

	IDirectSoundBuffer* tempBuffer = nullptr;
	if ( FAILED( pDirectSound->CreateSoundBuffer(&dsbd, &tempBuffer, nullptr) ) )
		return false;

	if ( FAILED(tempBuffer->QueryInterface(IID_IDirectSoundBuffer, (void**)&m_pAudioBuffer)) )
		return false;

	//m_pAudioBuffer->AddRef();
	tempBuffer->Release();
#endif
	m_soundKilled = false;
	return true;
}

void CVideoMaterial::CreateVideoMaterial()
{
	// ---------------------------
	// texture

	char szFileName[MAX_PATH];
	V_StripExtension(m_videoPath, szFileName, sizeof(szFileName));

	// names for our channel textures
	char ytexture[ MAX_PATH ];
	V_snprintf( ytexture, MAX_PATH, "%s_y", szFileName);
	char crtexture[ MAX_PATH ];
	V_snprintf( crtexture, MAX_PATH, "%s_cr", szFileName);
	char cbtexture[ MAX_PATH ];
	V_snprintf( cbtexture, MAX_PATH, "%s_cb", szFileName);

	const int tex_flags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP | 
					TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY;

	// You can create non-power of 2 textures but weird things can happen so don't.
	m_textureWidth = SmallestPowerOfTwoGreaterOrEqual( m_videoWidth );
	m_textureHeight = SmallestPowerOfTwoGreaterOrEqual( m_videoHeight );

	// create the textures
	m_yTexture.InitProceduralTexture( ytexture, "VideoCacheTextures", m_textureWidth, m_textureHeight, IMAGE_FORMAT_I8, tex_flags );
	// CB and CR are half the size of the Y (the brightness)
	m_cbTexture.InitProceduralTexture( cbtexture, "VideoCacheTextures", m_textureWidth >> 1, m_textureHeight >> 1, IMAGE_FORMAT_I8, tex_flags );
	m_crTexture.InitProceduralTexture( crtexture, "VideoCacheTextures", m_textureWidth >> 1, m_textureHeight >> 1, IMAGE_FORMAT_I8, tex_flags );

	m_yTextureRegen = new CYUVTextureRegenerator<YUVCHANNEL_Y>( m_videoWidth, m_videoHeight );
	m_cbTextureRegen = new CYUVTextureRegenerator<YUVCHANNEL_CB>( m_videoWidth / 2, m_videoHeight / 2 );
	m_crTextureRegen = new CYUVTextureRegenerator<YUVCHANNEL_CR>( m_videoWidth / 2, m_videoHeight / 2 );
	m_yTexture->SetTextureRegenerator( m_yTextureRegen );
	m_crTexture->SetTextureRegenerator( m_crTextureRegen );
	m_cbTexture->SetTextureRegenerator( m_cbTextureRegen );

	// ---------------------------
	// material
	// 
	// Use the Bik shader as it deals with YUV420
	KeyValues* pVMTKeyValues = new KeyValues( "Bik" );
	pVMTKeyValues->SetString( "$ytexture", ytexture );
	pVMTKeyValues->SetString( "$cbtexture", cbtexture );
	pVMTKeyValues->SetString( "$crtexture", crtexture );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$spriteorientation", 3 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetInt( "$nolod", 1 );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$nomip", 1 );
	m_videoMaterial.Init(szFileName, pVMTKeyValues );

	// Refresh the material vars because apparently init doesn't do this
	// and retains the previous video's frame
	m_videoMaterial->Refresh();

	m_videoReady = true;
	m_videoStarted = false;

	// update the procedural texture with the first frame of the video
	WebMFrame video_frame;
	VPXDecoder::Image image;
	while ( m_demuxer->readFrame( &video_frame, nullptr ) )
	{
		if ( !m_videoDecoder->decode( video_frame ) )
			continue;

		if ( m_videoDecoder->getImage( image ) == VPXDecoder::IMAGE_ERROR::NO_IMAGE_ERROR )
		{
			m_yTextureRegen->m_decodedImage = &image;
			m_crTextureRegen->m_decodedImage = &image;
			m_cbTextureRegen->m_decodedImage = &image;

			m_yTexture->Download();
			m_crTexture->Download();
			m_cbTexture->Download();

			break;
		}
	}

	m_demuxer->resetVideo();
}

const char *CVideoMaterial::GetVideoFileName()
{
	return m_videoPath;
}

VideoResult_t CVideoMaterial::GetLastResult()
{
	return VideoResult_t::SYSTEM_NOT_AVAILABLE;
}

VideoFrameRate_t &CVideoMaterial::GetVideoFrameRate()
{
	return m_frameRate;
}

// Video playback state functions
bool CVideoMaterial::IsVideoReadyToPlay()
{
	return m_videoReady;
}

bool CVideoMaterial::IsVideoPlaying()
{
	return m_videoPlaying;
}

bool CVideoMaterial::IsNewFrameReady()
{
	return false;
}

bool CVideoMaterial::IsFinishedPlaying()
{
	return m_videoEnded;
}

void CVideoMaterial::DestroySoundBuffer()
{
	if ( !m_pAudioBuffer )
		return;

#ifdef _LINUX
	if ( m_pSDLAudioStream )
	{
		SDL_FreeAudioStream( m_pSDLAudioStream );
	}
	m_pSDLAudioStream = nullptr;
	m_pAudioBuffer = nullptr;
	m_soundKilled = true;
#elif _WIN32

	m_pAudioBuffer->Release();
	m_pAudioBuffer = nullptr;
	m_soundKilled = true;
#endif
}

bool CVideoMaterial::StartVideo()
{
	if ( m_videoStarted )
		return true;

	m_videoStarted = true;
	m_videoPlaying = true;
	m_videoStopped = false;

	m_currentFrame = 0;
	m_videoTime = 0.0;
	m_curTime = 0.0;

	m_prevTicks = Plat_MSTime();

	return true;
}

bool CVideoMaterial::StopVideo()
{
	if ( !m_videoStarted )
		return true;

	m_videoStopped = true;
	m_videoStarted = false;
	DestroySoundBuffer();

	return true;
}

void CVideoMaterial::SetLooping( bool bLoopVideo )
{
	m_videoLooping = bLoopVideo;
}

bool CVideoMaterial::IsLooping()
{
	return m_videoLooping;
}

void CVideoMaterial::SetPaused( bool bPauseState )
{
	if ( m_videoStarted )
	{
		// Unpause
		if ( !m_videoPlaying && !bPauseState )
		{
#ifdef _WIN32
			if ( m_pAudioBuffer )
				m_pAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);
#endif
			m_prevTicks = Plat_MSTime();
		}
#ifdef _WIN32
		// Pause sound buffer
		else if ( m_videoPlaying && bPauseState )
		{
			if (m_pAudioBuffer)
				m_pAudioBuffer->Stop();
		}
#endif
	}

	m_videoPlaying = !bPauseState;
}

bool CVideoMaterial::IsPaused()
{
	return !m_videoPlaying;
}

float CVideoMaterial::GetVideoDuration()
{
	if ( m_demuxer )
		return m_demuxer->getLength();
	return 0.0f;
}

int CVideoMaterial::GetFrameCount()
{
	return 0;
}

bool CVideoMaterial::SetFrame( int FrameNum )
{
	return false;
}

int	CVideoMaterial::GetCurrentFrame()
{
	return m_currentFrame;
}

bool CVideoMaterial::SetTime( float flTime )
{
	return false;
}

float CVideoMaterial::GetCurrentVideoTime()
{
	return 0.0f;
}

bool CVideoMaterial::NeedNewFrame( double curtime )
{
	if ( m_videoFrames.Count() == 0 )
		return true;

	if ( m_videoFrames.Tail().time <= curtime )
		return true;

	if (m_pAudioBuffer && m_nAudioBufferFilledSize < m_nAudioBufferSize / 2.f)
		return true;

	return false;
}

void CVideoMaterial::RestartVideo()
{
	m_currentFrame = 0;
	m_demuxer->resetVideo();
	m_curTime = m_videoTime = 0.0;
	m_prevTicks = Plat_MSTime();
	if ( m_pAudioBuffer && !m_soundKilled )
	{
#ifdef _WIN32
		m_pAudioBuffer->Stop();
		m_pAudioBuffer->SetCurrentPosition( 0 );
		m_nAudioBufferWriteOffset = 0;
#endif
		m_nAudioBufferFilledSize = 0;
	}
}

bool CVideoMaterial::Update()
{
	if ( !StartVideo() )
		return false;

	// the video has stopped, there is nothing more to do
	if ( m_videoStopped )
		return false;

	if (m_awaitingUpdate)
	{
		m_awaitingUpdate = false;
		SetPaused(false);
	}

	// we're not stopped, but we're paused
	if ( !m_videoPlaying )
		return true;

	// Update time
	unsigned int curTicks = Plat_MSTime();

	if (m_awaitingUpdate)
	{
		m_prevTicks = curTicks;
		m_awaitingUpdate = false;
		return true;
	}

	double timepassed = ( double )( curTicks - m_prevTicks ) / 1000.0;
	m_curTime += timepassed;
	m_prevTicks = curTicks;

#ifdef _WIN32
	if ( m_pAudioBuffer )
	{
		// kinda rough but it'll do
		m_nAudioBufferFilledSize -= m_demuxer->getSampleRate() * m_nBytesPerSample * timepassed;
		if ( m_nAudioBufferFilledSize < 0 )
			m_nAudioBufferFilledSize = 0;
	}
#endif

	if (m_curTime < m_videoTime)
	{
		if (m_pAudioBuffer)
		{
			if (m_nAudioBufferFilledSize > (m_nAudioBufferSize / 2.f))
				return true;
		}
		else
			return true;
	}

	// Has the stream ended?
	if ( m_demuxer->isEOS() )
	{
		// Noodles; this might be stupid
		if ( m_videoFrames.Count() == 0 )
		{
			if ( m_videoLooping )
			{
				RestartVideo();
			}
			else
			{
				m_videoEnded = true;
				StopVideo();
				return false;
			}
		}
	}

	// if we have audio check if we should update the buffer and make sure we're playing
	bool bNeedUpdate = false;
#ifdef _WIN32
	DWORD status = NULL;
	if ( !m_demuxer->isEOS() && m_pAudioBuffer )
	{
		m_pAudioBuffer->GetStatus( &status );
		if ( !( status & DSBSTATUS_PLAYING ) )
		{
			m_pAudioBuffer->SetCurrentPosition(m_nAudioBufferWriteOffset);
			m_pAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);
		}
	}
#endif
	// Read until we've filled the buffer or got enough frames
	while ( NeedNewFrame( m_curTime ) || bNeedUpdate )
	{
		WebMFrame video_frame, audio_frame;
		// did we reach the EOS
		if ( !m_demuxer->readFrame( &video_frame, &audio_frame) )
		{
			bNeedUpdate = false;
			break;
		}
		else if ( video_frame.isValid() )
		{
			m_videoFrames.Insert( video_frame );
		}

		bNeedUpdate = false;
		if (audio_frame.isValid() && m_pAudioBuffer )
		{
			int numOutSamples = 0;
			
			m_audioDecoder->getPCMS16( audio_frame, m_pcm, numOutSamples );
			if ( numOutSamples == 0 )
				continue;

			int nBytesRead = numOutSamples * m_nBytesPerSample;
#ifdef _WIN32
			int nPCMOverflowSize = 0;
			int nPCMOverflowOffset = 0;
			m_nAudioBufferFilledSize += nBytesRead;

			// can't fit the whole thing at the end so it needs to be split
			if ( ( m_nAudioBufferWriteOffset + nBytesRead ) >= m_nAudioBufferSize )
			{
				// save amount gone over
				nPCMOverflowSize = ( m_nAudioBufferWriteOffset + nBytesRead ) - m_nAudioBufferSize;
				nPCMOverflowOffset = nBytesRead - nPCMOverflowSize;
				nBytesRead -= nPCMOverflowSize;
				bNeedUpdate = true;
			}

			void *pAudioPtr = NULL;
			DWORD dwAudioBytes1;
			m_pAudioBuffer->Lock(m_nAudioBufferWriteOffset, nBytesRead, &pAudioPtr, &dwAudioBytes1, NULL, NULL, 0);

			V_memcpy( pAudioPtr, m_pcm, nBytesRead );
			m_nAudioBufferWriteOffset += nBytesRead;

			m_pAudioBuffer->Unlock(pAudioPtr, dwAudioBytes1, NULL, NULL);

			if ( m_nAudioBufferWriteOffset == m_nAudioBufferSize )
			{
				m_nAudioBufferWriteOffset = 0;
				m_pAudioBuffer->Lock(0, nBytesRead, &pAudioPtr, &dwAudioBytes1, NULL, NULL, 0);

				V_memcpy( pAudioPtr, ( char* )( m_pcm )+nPCMOverflowOffset, nPCMOverflowSize );
				m_nAudioBufferWriteOffset += nPCMOverflowSize;

				m_pAudioBuffer->Unlock(pAudioPtr, dwAudioBytes1, NULL, NULL);
			}
#elif _LINUX
			SDL_AudioStreamPut(m_pSDLAudioStream, m_pcm, nBytesRead);
#endif

			// if our timer is waayyy ahead set it back to the audio time
			if ( m_curTime > audio_frame.time )
			{
				m_curTime = audio_frame.time;
			}

			if (audio_frame.buffer)
				free(audio_frame.buffer);
		}
	}

	// roll back for videos with no audio
	if ( !m_demuxer->isEOS() && !m_audioDecoder->isOpen() )
	{
		// if our current time is out, roll it back
		// Noodles; I feel this will cause issues, but it seems fine right now
		double frameDur = 1.0 / m_frameRate.GetFPS();
		if ( m_videoFrames.Count() > 0 && ( m_curTime - m_videoFrames.Head().time ) > ( frameDur * 6.0 ) )
		{
			m_curTime = m_videoTime - frameDur;
		}
	}

	while ( m_videoFrames.Count() > 0 && m_curTime >= m_videoTime )
	{
		if ( m_videoFrames.Head().isValid() )
		{
			// TODO figure out how to skip frames
			m_videoDecoder->decode( m_videoFrames.Head() );

			VPXDecoder::IMAGE_ERROR err;
			if ( ( err = m_videoDecoder->getImage( *m_image ) ) != VPXDecoder::NO_FRAME )
			{
				if ( err == VPXDecoder::IMAGE_ERROR::NO_IMAGE_ERROR )
				{
					m_yTextureRegen->m_decodedImage = m_image;
					m_crTextureRegen->m_decodedImage = m_image;
					m_cbTextureRegen->m_decodedImage = m_image;

					m_yTexture->Download();
					m_crTexture->Download();
					m_cbTexture->Download();
				}
			}
			m_videoTime = m_videoFrames.Head().time;
			m_currentFrame++;
		}

		// you are responsible for freeing the buffer
		WebMFrame frame = m_videoFrames.RemoveAtHead();
		if (frame.buffer)
			free(frame.buffer);
	}
	
	return true;
}

// Material / Texture Info functions
IMaterial *CVideoMaterial::GetMaterial()
{
	return m_videoMaterial;
}

// Where the video is actually is within the texture
void CVideoMaterial::GetVideoTexCoordRange( float *pMaxU, float *pMaxV )
{
	*pMaxU = (float)m_videoWidth / (float)m_textureWidth;
	*pMaxV = (float)m_videoHeight / (float)m_textureHeight;
}

void CVideoMaterial::GetVideoImageSize( int *pWidth, int *pHeight )
{
	*pWidth = m_videoWidth;
	*pHeight = m_videoHeight;
}

// Audio Functions
bool CVideoMaterial::HasAudio()
{
	return m_audioDecoder && m_audioDecoder->isOpen();
}

bool CVideoMaterial::SetVolume( float fVolume )
{
	m_volume = min( 1.0f, max( 0.0f, fVolume ) );
	if ( !m_pAudioBuffer )
		return false;

#ifdef _WIN32
	// TODO figure out what fucking value I'm supposed to use
	float log_volume = pow( m_volume, 0.2 );
	m_pAudioBuffer->SetVolume((LONG)(-10000 * (1.0f - log_volume)));
	return true;
#elif _LINUX
	return true;
#else
	return false;
#endif
}

float CVideoMaterial::GetVolume()
{
	return m_volume;
}

void CVideoMaterial::SetMuted( bool bMuteState )
{
}

bool CVideoMaterial::IsMuted()
{
	return false;
}

VideoResult_t CVideoMaterial::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
{
#ifdef _WIN32
	if ( operation == VideoSoundDeviceOperation_t::SET_DIRECT_SOUND_DEVICE )
	{
		// if we had a sound buffer before, kill it
		m_soundKilled = true;
		DestroySoundBuffer();
		CreateSoundBuffer( pDevice );
		return VideoResult_t::SUCCESS;
	}
#elif _LINUX
	// Maybe called when changing audio device?
	if( operation == VideoSoundDeviceOperation_t::SET_SDL_SOUND_DEVICE )
	{
	}
	// Called on start up and sound restart
	else if( operation == VideoSoundDeviceOperation_t::SET_SDL_PARAMS )
	{
		// if we had a sound buffer before, kill it
		if ( m_pAudioDevice )
			DestroySoundBuffer();

		CreateSoundBuffer( pDevice );
		return VideoResult_t::SUCCESS;
	}
	else if( operation == VideoSoundDeviceOperation_t::SDLMIXER_CALLBACK )
	{
		if ( !m_audioDecoder->isOpen() )
			return VideoResult_t::SUCCESS;

		if( !m_videoPlaying )
			return VideoResult_t::SUCCESS;

		if( m_soundKilled)
			return VideoResult_t::SUCCESS;

		int length = *(int *)pData;
		if( SDL_AudioStreamAvailable(m_pSDLAudioStream) < length)
			return VideoResult_t::SUCCESS;
		
		SDL_AudioStreamGet( m_pSDLAudioStream, m_pAudioBuffer, length);
		SDL_MixAudioFormat( (Uint8 *)pDevice, m_pAudioBuffer, m_pAudioDevice->format, length, (int)(GetVolume() * SDL_MIX_MAXVOLUME) );

		m_nAudioBufferFilledSize = SDL_AudioStreamAvailable(m_pSDLAudioStream);

		return VideoResult_t::SUCCESS;
	}
#endif
	return VideoResult_t::SYSTEM_NOT_AVAILABLE;
}

void CVideoMaterial::PauseUntilUpdate()
{
	m_awaitingUpdate = true;
	SetPaused(true);
}