#ifndef VIDEO_MATERIAL_H
#define VIDEO_MATERIAL_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "video/ivideoservices.h"
#include "WebMDemuxer.hpp"
#include "tier1/utlqueue.h"

#include "OpusVorbisDecoder.hpp"
#include "VPXDecoder.hpp"
#include <mkvparser/mkvparser.h>
#include "filesystem.h"

#ifdef _WIN32
#include <windows.h>
#include "dsound.h"
#elif _LINUX
#include "SDL2/SDL.h"
#include "SDL2/SDL_audio.h"
#endif

typedef enum YUVChannel_e {
	YUVCHANNEL_Y,
	YUVCHANNEL_CB,
	YUVCHANNEL_CR,
} YUVChannel_t;

class MkvReader : public mkvparser::IMkvReader
{
public:
	MkvReader( const char *filePath ) :
		m_fileHandle( g_pFullFileSystem->Open( filePath, "rb" ) )
	{}

	~MkvReader()
	{
		if (m_fileHandle != FILESYSTEM_INVALID_HANDLE)
		{
			g_pFullFileSystem->Close(m_fileHandle);
			g_pFullFileSystem->Flush(m_fileHandle);
		}
	}

	bool Loaded() const
	{
		return m_fileHandle != FILESYSTEM_INVALID_HANDLE;
	}

	int Read( long long pos, long len, unsigned char *buf )
	{
		if ( m_fileHandle == FILESYSTEM_INVALID_HANDLE )
			return -1;

		g_pFullFileSystem->Seek( m_fileHandle, pos, FILESYSTEM_SEEK_HEAD );
		const int size = g_pFullFileSystem->Read( buf, len, m_fileHandle );
		if ( size < len )
			return -1;
		return 0;
	}
	int Length( long long *total, long long *available )
	{
		if ( m_fileHandle == FILESYSTEM_INVALID_HANDLE )
			return -1;
		const int pos = g_pFullFileSystem->Tell( m_fileHandle );
		g_pFullFileSystem->Seek( m_fileHandle, 0, FILESYSTEM_SEEK_TAIL );
		if ( total )
			*total = g_pFullFileSystem->Tell( m_fileHandle );
		if ( available )
			*available = g_pFullFileSystem->Tell( m_fileHandle );
		g_pFullFileSystem->Seek( m_fileHandle, pos, FILESYSTEM_SEEK_HEAD );
		return 0;
	}

private:
	FileHandle_t m_fileHandle;
};

template <YUVChannel_t Channel>
class CYUVTextureRegenerator : public ITextureRegenerator
{
public:
	CYUVTextureRegenerator( int w, int h )
	{
		m_decodedImage = nullptr;
		m_videoWidth = w;
		m_videoHeight = h;
	}

	// ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect );
	virtual void Release() {};
	VPXDecoder::Image *m_decodedImage;

private:
	int m_videoWidth;
	int m_videoHeight;
};

class CVideoMaterial : public IVideoMaterial
{
public:
	CVideoMaterial();
	~CVideoMaterial();

	// Video information functions		
	virtual const char *GetVideoFileName();
	virtual VideoResult_t		GetLastResult();

	virtual VideoFrameRate_t &GetVideoFrameRate();

	bool LoadVideo( const char *pMaterialName, const char *pVideoFileName, void *pSoundDevice = nullptr );

	// Audio Functions
	virtual bool				HasAudio();

	virtual bool				SetVolume( float fVolume );
	virtual float				GetVolume();

	virtual void				SetMuted( bool bMuteState );
	virtual bool				IsMuted();

	virtual VideoResult_t		SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr );

	// Video playback state functions
	virtual bool				IsVideoReadyToPlay();
	virtual bool				IsVideoPlaying();
	virtual bool				IsNewFrameReady();
	virtual bool				IsFinishedPlaying();

	virtual bool				StartVideo();
	virtual bool				StopVideo();

	virtual void				SetLooping( bool bLoopVideo );
	virtual bool				IsLooping();

	virtual void				SetPaused( bool bPauseState );
	virtual bool				IsPaused();

	// Position in playback functions
	virtual float				GetVideoDuration();
	virtual int					GetFrameCount();

	virtual bool				SetFrame( int FrameNum );
	virtual int					GetCurrentFrame();

	virtual bool				SetTime( float flTime );
	virtual float				GetCurrentVideoTime();

	// Update function
	virtual bool				Update();

	// Material / Texture Info functions
	virtual IMaterial *GetMaterial();

	virtual void				GetVideoTexCoordRange( float *pMaxU, float *pMaxV );
	virtual void				GetVideoImageSize( int *pWidth, int *pHeight );

	// for stopping the sound buffer when the main thread gets interrupted
	virtual void				FreezeSoundBuffer();

#ifdef _WIN32
	void UpdateSoundBuffer();
#endif

private:
	bool NeedNewFrame( double timepassed );
	bool CreateSoundBuffer(void *pSoundDevice = nullptr);
	void DestroySoundBuffer();
	void RestartVideo();
	void CreateVideoMaterial();

private:

	MkvReader *m_mkvReader;
	WebMDemuxer *m_demuxer;
	VPXDecoder *m_videoDecoder;
	OpusVorbisDecoder *m_audioDecoder;
	VideoFrameRate_t m_frameRate;
	VPXDecoder::Image *m_image;

	CMaterialReference m_videoMaterial;
	CYUVTextureRegenerator<YUVCHANNEL_Y> *m_yTextureRegen;
	CYUVTextureRegenerator<YUVCHANNEL_CB> *m_cbTextureRegen;
	CYUVTextureRegenerator<YUVCHANNEL_CR> *m_crTextureRegen;

	CTextureReference m_yTexture;
	CTextureReference m_cbTexture;
	CTextureReference m_crTexture;

	int m_videoWidth; // actual video width
	int m_videoHeight; // actual video height
	int m_textureWidth;
	int m_textureHeight;

	bool m_videoReady;
	bool m_videoStarted;
	bool m_videoStopped;
	bool m_videoPlaying;
	bool m_videoLooping;
	bool m_videoEnded;

	char m_videoPath[MAX_PATH];

	float m_volume;
	double m_curTime;
	double m_videoTime;

	unsigned int m_prevTicks;
	unsigned int m_currentFrame;
	CUtlQueue<WebMFrame> m_videoFrames;

#ifdef _LINUX
	SDL_AudioSpec* m_pAudioDevice;
	Uint8* m_pAudioBuffer;

	SDL_AudioStream *m_pSDLAudioStream;
#elif _WIN32
	IDirectSound* m_pAudioDevice;
	IDirectSoundBuffer* m_pAudioBuffer;

	int m_nAudioBufferWriteOffset;
	int m_nAudioBufferSize;
#endif

	bool m_soundKilled;
	short* m_pcm;
	int m_nAudioBufferFilledSize;

	int m_nBytesPerSample;
};

#endif