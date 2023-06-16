#ifndef VIDEO_MATERIAL_H
#define VIDEO_MATERIAL_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "video/ivideoservices.h"

#include "OpusVorbisDecoder.hpp"
#include "VPXDecoder.hpp"
#include <mkvparser/mkvparser.h>
#include <sys/types.h>

#ifdef _WIN32
#include <Windows.h>
#include "dsound.h"
#endif

class MkvReader : public mkvparser::IMkvReader
{
public:
	MkvReader( const char *filePath ) :
		m_file( fopen( filePath, "rb" ) )
	{}
	~MkvReader()
	{
		if ( m_file )
			fclose( m_file );
	}

	int Read( long long pos, long len, unsigned char *buf )
	{
		if ( !m_file )
			return -1;
		fseek( m_file, pos, SEEK_SET );
		const size_t size = fread( buf, 1, len, m_file );
		if ( size < size_t( len ) )
			return -1;
		return 0;
	}
	int Length( long long *total, long long *available )
	{
		if ( !m_file )
			return -1;
		const off_t pos = ftell( m_file );
		fseek( m_file, 0, SEEK_END );
		if ( total )
			*total = ftell( m_file );
		if ( available )
			*available = ftell( m_file );
		fseek( m_file, pos, SEEK_SET );
		return 0;
	}

private:
	FILE *m_file;
};

enum Channel_e {
	CHANNEL_Y,
	CHANNEL_CB,
	CHANNEL_CR,
};

class CYUVTextureRegenerator : public ITextureRegenerator
{
public:
	CYUVTextureRegenerator( Channel_e c, int w, int h )
	{
		m_decodedImage = nullptr;
		m_channel = c;
		m_videoWidth = w;
		m_videoHeight = h;
	}

	// ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect );
	virtual void Release();
	VPXDecoder::Image *m_decodedImage;

private:
	Channel_e m_channel;
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

#ifdef _WIN32
	void SetAudioBufferCopied( bool bSecondHalf, bool bCopied );
	bool WasAudioBufferCopied(bool bSecondHalf );
	static unsigned _HandleBufferEvents( void *params );
#endif

private:
	bool NeedNewFrame( double timepassed );
	bool CreateSoundBuffer();
	void DestroySoundBuffer();
	void RestartVideo();

private:

	MkvReader *m_mkvReader;
	WebMDemuxer *m_demuxer;
	VPXDecoder *m_videoDecoder;
	OpusVorbisDecoder *m_audioDecoder;
	WebMFrame *m_audioFrame;
	VideoFrameRate_t m_frameRate;
	VPXDecoder::Image *m_image;

	CMaterialReference m_videoMaterial;
	CYUVTextureRegenerator *m_yTextureRegen;
	CYUVTextureRegenerator *m_cbTextureRegen;
	CYUVTextureRegenerator *m_crTextureRegen;

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

	short *m_pcm;
	int m_pcmOverflow;
	int m_pcmOffset;

	float m_volume;
	double m_curTime;
	double m_videoTime;

	unsigned int m_prevTicks;
	unsigned int m_currentFrame;
	CUtlVector< WebMFrame *> m_vecVideoFrames;
	bool m_soundKilled;
	
	bool m_bufferCopiedFirst;
	bool m_bufferCopiedSecond;

#ifdef _WIN32
	CThreadMutex	m_mutex;
	ThreadHandle_t m_hTest;

	IDirectSound8 *m_directSound;
	LPDIRECTSOUNDNOTIFY m_directSoundNotify;
	IDirectSoundBuffer *m_directSoundBuffer;
	WAVEFORMATEX m_waveFormat;

	HANDLE m_beginningEventHandle;
	HANDLE m_halfwayEventHandle;
	HANDLE m_bufferDestroyedEventHandle;
#endif
};

#endif