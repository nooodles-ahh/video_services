#ifndef VIDEO_MATERIAL_H
#define VIDEO_MATERIAL_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "video/ivideoservices.h"
#include "tier1/utlqueue.h"

#include "OpusVorbisDecoder.hpp"
#include "VPXDecoder.hpp"
#include <mkvparser/mkvparser.h>

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

#ifdef _WIN32
	static unsigned int HandleBufferUpdates(void *params);
#endif

private:
	bool NeedNewFrame( double timepassed );
	bool CreateSoundBuffer(void *pSoundDevice = nullptr);
	void DestroySoundBuffer();
	void RestartVideo();
	void CreateVideoMaterial(const char *pMaterialName);

private:

	MkvReader *m_mkvReader;
	WebMDemuxer *m_demuxer;
	VPXDecoder *m_videoDecoder;
	OpusVorbisDecoder *m_audioDecoder;
	WebMFrame *m_audioFrame;
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
	CUtlQueue< WebMFrame*> m_videoFrames;

#ifdef _LINUX
	SDL_AudioSpec* m_pAudioDevice;
	Uint8* m_pAudioBuffer;

	SDL_AudioStream *m_pSDLAudioStream;
#elif _WIN32
	IDirectSound* m_pAudioDevice;
	IDirectSoundBuffer* m_pAudioBuffer;

	CThreadMutex m_mutex;
	IDirectSoundNotify *m_directSoundNotify;
	HANDLE m_endEventHandle;
	HANDLE m_halfwayEventHandle;
	HANDLE m_videoOverEventHandle;
	ThreadHandle_t m_hBufferThreadHandle;
#endif

	bool m_soundKilled;
	short* m_pcm;
	int m_nAudioBufferWriteOffset;
	int m_nAudioBufferReadOffset;
	int m_nAudioBufferFilledSize;

	int m_nAudioBufferSize;
	int m_nBytesPerSample;
};

#endif