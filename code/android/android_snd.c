//
// Android audio using OpenSLES
//

#include "../client/snd_local.h"
#include "../qcommon/q_shared.h"

#if defined(__ANDROID__)
	#include <SLES/OpenSLES_Android.h>
#else
	#include <SLES/OpenSLES.h>
#endif

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;

#if defined(__ANDROID__)
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
#else
static SLBufferQueueItf bqPlayerBufferQueue;
#endif

#define OPENSL_BUFF_LEN 1024

qboolean snd_inited = qfalse;

/* Some devices may work only with 48000 */
static int tryrates[] = { 22050, 11025, 44100, 48000, 8000 };

static int dmapos = 0;
static int dmasize = 0;

static unsigned char play_buffer[OPENSL_BUFF_LEN];

void audioPause( void )
{
	int result;
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("Audio Pause ERROR: %i", result);
	}
}

void audioPlay( void )
{
	int result;
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("Audio SetPlayState ERROR : %i", result);
	}

	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, "\0", 1);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("Audio Play ERROR: Enqueue first buffer error nb: %i", result);
	}
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	int len = OPENSL_BUFF_LEN;
	int factor = 2*2;
	int FrameCount = (unsigned int)OPENSL_BUFF_LEN / factor;

	if (!snd_inited)
	{
		// shouldn't happen
		memset(play_buffer, '\0', len);
		return;
	}
	else
	{
		int tobufend = dmasize - pos; // bytes to buffer's end.
		int len1 = len;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy(play_buffer, dma.buffer + pos, len1);
		if (len2 <= 0)
			dmapos += (len1 / (dma.samplebits/8));
		else  /* wraparound? */
		{
			memcpy(play_buffer+len1, dma.buffer, len2);
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if (dmapos >= dmasize)
		dmapos = 0;

	SLresult result;

	if (FrameCount == 0)
		FrameCount = 1;

	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, play_buffer, FrameCount * factor);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES Enqueue Buffer Queue Error");
	}
}

// SLES exemple : https://github.com/android/ndk-samples/blob/master/native-audio/app/src/main/cpp/native-audio-jni.c
qboolean SNDDMA_Init( void ) {
	int i;
	cvar_t *sndbits;
	cvar_t *sndspeed;
	cvar_t *sndchannels;
	//cvar_t *snddevice;

	SLresult result;
	qboolean find = qfalse;

	if ( snd_inited ) {
		return qtrue;
	}

	sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE_ND | CVAR_LATCH);
	sndspeed = Cvar_Get("sndspeed", "22050", CVAR_ARCHIVE_ND | CVAR_LATCH);
	sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE_ND |  CVAR_LATCH);
	//snddevice = Cvar_Get("snddevice", "/dev/dsp", CVAR_ARCHIVE_ND | CVAR_LATCH);

	// set sample bits
	dma.samplebits = (int)sndbits->value;
	if (dma.samplebits != 16 && dma.samplebits != 8) {
	if (dma.samplebits < 8 || dma.samplebits > 16)
		dma.samplebits = 16;
	}

	// set sample speed
	dma.speed = (int)sndspeed->value;
	if (dma.speed) {
		for ( i = 0 ; i < sizeof(tryrates)/4 ; i++ ) {
			if (tryrates[i] == dma.speed) {
				find = qtrue;
				break;
			}
		}
	}
	if (!dma.speed || !find) {
		dma.speed = 22050;
	}
	
	// set sample channels number
	dma.channels = (int)sndchannels->value;
	if (dma.channels < 1 || dma.channels > 2)
		dma.channels = 2;
	

	//dma.samples = OPENSL_BUFF_LEN * dma.samplebits * dma.channels;// stereo ?
	dma.samples = OPENSL_BUFF_LEN * dma.samplebits;
	dma.fullsamples = dma.samples / dma.channels;
	//dma.submission_chunk = 1024*2
	dma.submission_chunk = 1;


	dmapos = 0;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc(1, dmasize);

	Com_Printf("= Audio Initialisation");
	Com_Printf("= dma.samplebits : %i", dma.samplebits);
	Com_Printf("= dma.speed : %i", dma.speed);
	Com_Printf("= dma.channels : %i", dma.channels);
	Com_Printf("= OpenSLES Initialisation");

	// create engine
	result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES engine creation Error");
		return qfalse;
	}
	(void)result;

	// realize the engine
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES engine realize Error");
		return qfalse;
	}
	(void)result;

	// get the engine interface, which is needed in order to create other objects
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES get interface Error");
		return qfalse;
	}
	(void)result;

	// create output mix
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES output mix creation Error");
		return qfalse;
	}
	(void)result;

	// realize the output mix
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES output mix realize Error");
		return qfalse;
	}
	(void)result;
	Com_Printf("= SUCCESS!");

	//CREATE THE PLAYER

	// configure audio source
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

	SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_22_05,
		SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};

	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	Com_Printf("= Create audio player");

	const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};

	const SLboolean req[1] = {SL_BOOLEAN_TRUE};

	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 1, ids, req);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES CreateAudioPlayer Error");
		return qfalse;
	}
	(void)result;

	// realize the player
	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES Realize AudioPlayer Error");
		return qfalse;
	}
	(void)result;

	// get the play interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES GetInterface AudioPlayer Error");
		return qfalse;
	}
	(void)result;

	// get the buffer queue interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES GetInterface buffer queue Error");
		return qfalse;
	}
	(void)result;

	// register callback on the buffer queue
#if defined(__ANDROID__)
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, (slAndroidSimpleBufferQueueCallback)bqPlayerCallback, NULL);
#else
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, (slBufferQueueCallback)bqPlayerCallback, NULL);
#endif

	if (result != SL_RESULT_SUCCESS) {
		Com_Printf("= SLES RegisterCallback Error");
		return qfalse;
	}
	(void)result;

	snd_inited = qtrue;

	// set the player's state to playing
	Com_Printf("= SUCCESS!");

	audioPlay();

	return qtrue;
}

int SNDDMA_GetDMAPos( void ) {
	if ( !snd_inited ) {
		return 0;
	}
	return dmapos;
}

void SNDDMA_Shutdown( void ) {
	audioPause();
	(*bqPlayerObject)->Destroy(bqPlayerObject);
	(*outputMixObject)->Destroy(outputMixObject);
	(*engineObject)->Destroy(engineObject);

	bqPlayerObject = NULL;
	outputMixObject = NULL;
	engineObject = NULL;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
 */
void SNDDMA_Submit( void )
{
}

void SNDDMA_BeginPainting( void )
{
}