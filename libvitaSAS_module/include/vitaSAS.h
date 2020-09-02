#ifndef VITASAS_H
#define VITASAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <psp2/audiodec.h> 
#include <psp2/audioout.h>

#define ROUND_UP(x, a)	((((unsigned int)x)+((a)-1u))&(~((a)-1u)))
#define VITASAS_GRAIN_MAX 2048
#define SCE_SAS_LOOP_DISABLE_PCM -1
#define VITASAS_NO_SUBSYSTEM -1
#define VITASAS_USE_MAIN_MEMORY 1
#define VITASAS_USE_PHYCONT_MEMORY 0

#define DEFAULT_HEAP_SIZE 1 * 1024 * 1024;

/* SAS system limits */

#define MAX_SAS_SYSTEM_NUM			8
#define CHANNEL_MAX					2
#define BUFFER_MAX					2

typedef void(*AudioOutRenderHandler)(void* buffer, int SASSystemNum);

typedef struct AudioOutWork {
	int systemNum;
	SceUID updateThreadId;
	SceUID eventFlagId;
	SceUID portId;
	volatile uint32_t isAborted;
	uint32_t numGrain;
	uint32_t outputSamplingRate;
	uint32_t outputPort;
	AudioOutRenderHandler renderHandler;
} AudioOutWork;

typedef struct vitaSASAudio {
	void* datap;
	size_t data_size;
	SceUID data_id;
} vitaSASAudio;

typedef struct vitaSASSystem {
	AudioOutWork audioWork;
	SceUID sasSystemHandle;
	int systemNum;
	int isSubSystem;
	int subSystemNum;
	uint32_t subSystemMixVolL;
	uint32_t subSystemMixVolR;
} vitaSASSystem;

typedef struct File {
	const char *pName;
	uint32_t size;
} File;

typedef struct Buffer {
	uint8_t *p;
	uint8_t *op[2];
	uint32_t size;
	uint32_t offsetR;
	uint32_t offsetW;
	uint32_t bufIndex;
} Buffer;

typedef struct FileStream {
	File file;
	Buffer buf;
} FileStream;

typedef struct CodecEngineMemBlock {
	SceUID uidMemBlock;
	SceUID uidUnmap;
	unsigned int vaContext;
	unsigned int contextSize;
} CodecEngineMemBlock;

typedef struct VitaSAS_Decoder {
	FileStream* pInput;
	FileStream* pOutput;
	SceAudiodecCtrl* pAudiodecCtrl;
	SceAudiodecInfo* pAudiodecInfo;
	CodecEngineMemBlock* codecMemBlock;
	unsigned int headerSize;
	unsigned int decodeStatus;
} VitaSAS_Decoder;

typedef struct vitaSASVoiceParam {
	SceUInt32 loop;
	SceUInt32 loopSize;
	SceUInt32 pitch;
	SceUInt32 volLDry;
	SceUInt32 volRDry;
	SceUInt32 volLWet;
	SceUInt32 volRWet;
	SceUInt32 adsr1;
	SceUInt32 adsr2;
} vitaSASVoiceParam;

typedef struct VitaSASSystemParam {
	SceUInt32 outputPort;
	SceUInt32 samplingRate;
	SceUInt32 numGrain;
	SceUInt32 thPriority;
	SceUInt32 thStackSize;
	SceUInt32 thCpu;
	SceUInt32 isSubSystem;
	SceUInt32 subSystemMixVolL;
	SceUInt32 subSystemMixVolR;
	SceInt32 subSystemNum;
} VitaSASSystemParam;

/*----------------------------- Common -----------------------------*/

/**
 * Set internal heap size (1MB by default). Call this before initialization.
 *
 * @param[in] size - size of the heap in bytes
 *
 */
void vitaSAS_set_heap_size(unsigned int size);

/**
 * Initialize libvitaSAS
 *
 * @param[in] openBGM - set to 1 if you are going to use auidio decoder functions, otherwise set to 0
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_init(unsigned int openBGM);

/**
 * Finalize libvitaSAS
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_finish(void);

/**
 * Create SAS system instance
 *
 * @param[in] systemInitParam - parameters related to SAS system instance
 *
 * @return SAS system number, <0 on error.
 */
int vitaSAS_create_system(VitaSASSystemParam* systemInitParam);

/**
 * Create SAS system instance with specified configuration
 *
 * @param[in] sasConfig - SAS system configuration string. Ex. "numGrains=256 numVoices=1 numReverbs=1"
 * @param[in] systemInitParam - parameters related to SAS system instance
 *
 * @return SAS system number, <0 on error.
 */
int vitaSAS_create_system_with_config(const char* sasConfig, VitaSASSystemParam* systemInitParam);

/**
 * Destroy currently selected SAS system
 *
 */
void vitaSAS_destroy_system(void);

/**
 * Select SAS system instance to work with
 *
 * @param[in] systemNum - SAS system number to select
 *
 */
void vitaSAS_select_system(int systemNum);

/**
 * Suspend currently selected system
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_pause_system_render(void);

/**
 * Resume currently selected system
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_resume_system_render(void);

/**
 * Get currently selected SAS system handle
 *
 * @return SAS system handle
 */
SceUID vitaSAS_get_system_handle(void);

/**
 * Set mixer volume for currently selected SAS system 
 * Only effective if currently selected SAS system is running in subsystem mode
 *
 * @param[in] subSystemMixVolL - mixer volume for left channel
 * @param[in] subSystemMixVolR - mixer volume for right channel
 *
 */
void vitaSAS_set_sub_system_vol(unsigned int subSystemMixVolL, unsigned int subSystemMixVolR);

/**
 * Delete SAS sample audio data
 *
 * @param[in] info - voice information structure
 *
 */
void vitaSAS_free_audio(vitaSASAudio* info);

/**
 * Create SAS sample audio data from VAG file
 *
 * @param[in] soundPath - path to the VAG file
 * @param[in] io_type - set to 0 to use normal IO or to 1 to use FIOS2
 *
 * @return SAS voice information structure, NULL on error.
 */
vitaSASAudio* vitaSAS_load_audio_VAG(char* soundPath, int io_type);

/**
 * Create SAS sample audio data from raw PCM file
 *
 * @param[in] soundPath - path to the raw PCM file
 * @param[in] io_type - set to 0 to use normal IO or to 1 to use FIOS2
 *
 * @return SAS voice information structure, NULL on error.
 */
vitaSASAudio* vitaSAS_load_audio_PCM(char* soundPath, int io_type);

/**
 * Create SAS sample audio data from WAV file
 *
 * @param[in] soundPath - path to the WAV file
 * @param[in] io_type - set to 0 to use normal IO or to 1 to use FIOS2
 *
 * @return SAS voice information structure, NULL on error.
 */
vitaSASAudio* vitaSAS_load_audio_WAV(char* soundPath, int io_type);

/**
 * Create SAS sample audio data from data buffer
 *
 * @param[in] pData - pointer to the buffer that holds sample data
 * @param[in] dataSize - size of the sample data buffer
 *
 * @return SAS voice information structure, NULL on error.
 */
vitaSASAudio* vitaSAS_load_audio_custom(void* pData, unsigned int dataSize);

/**
 * NEON-optimized channel separation for raw PCM
 *
 * @param[out] pBufL - pointer to the output buffer that will hold left channel data
 * @param[out] pBufR - pointer to the output buffer that will hold right channel data
 * @param[in] pBufSrc - pointer to the buffer that holds input stereo PCM data
 * @param[in] bufSrcSize - size of the input PCM data buffer
 *
 */
void vitaSAS_separate_channels_PCM(short* pBufL, short* pBufR, short* pBufSrc, unsigned int bufSrcSize);

/**
 * Wrapper call for sceSasSetKeyOn(), enable vocalization of voice
 *
 * @param[in] voiceID - voice ID to enable vocalization for
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_set_key_on(unsigned int voiceID);

/**
 * Wrapper call for sceSasSetKeyOff(), disable vocalization of voice
 *
 * @param[in] voiceID - voice ID to disable vocalization for
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_set_key_off(unsigned int voiceID);

/**
 * Wrapper call for sceSasGetEndState(), get sound generation end state
 *
 * @param[in] voiceID - voice ID to get state for
 *
 * @return SCE_OK, <0 on error.
 */
int vitaSAS_get_end_state(unsigned int voiceID);

/*----------------------------- Codec Engine decoding -----------------------------*/

/**
 * Destroy decoder instance
 *
 * @param[in] decoderInfo - decoder instance information to destroy
 *
 */
void vitaSAS_destroy_decoder(VitaSAS_Decoder* decoderInfo);

/**
 * Create AT9 decoder
 *
 * @param[in] soundPath - path to audio file for decoder instance
 * @param[in] useMainMem - set to 0 to use PHYCONT memory (faster), set to 1 to use main memory (for system mode apps)
 *
 * @return decoder information structure, NULL on error.
 */
VitaSAS_Decoder* vitaSAS_create_AT9_decoder(const char* soundPath, unsigned int useMainMem);

/**
 * Create MP3 decoder
 *
 * @param[in] soundPath - path to audio file for decoder instance
 *
 * @return decoder information structure, NULL on error.
 */
VitaSAS_Decoder* vitaSAS_create_MP3_decoder(const char* soundPath);

/**
 * Create AAC decoder
 *
 * @param[in] soundPath - path to audio file for decoder instance
 * @param[in] useMainMem - set to 0 to use PHYCONT memory (faster), set to 1 to use main memory (for system mode apps)
 *
 * @return decoder information structure, NULL on error.
 */
VitaSAS_Decoder* vitaSAS_create_AAC_decoder(const char* soundPath, unsigned int useMainMem);

/**
 * Start decoder playback
 *
 * @param[in] decoderInfo - information structure of decoder
 * @param[in] thPriority - decoding thread priority
 * @param[in] thStackSize - decoding thread stack size
 * @param[in] thCpu - decoding thread CPU affinity mask
 *
 */
void vitaSAS_decoder_start_playback(VitaSAS_Decoder* decoderInfo, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);

/**
 * Pause decoder playback
 *
 * @param[in] decoderInfo - information structure of decoder
 *
 */
void vitaSAS_decoder_pause_playback(VitaSAS_Decoder* decoderInfo);

/**
 * Resume decoder playback
 *
 * @param[in] decoderInfo - information structure of decoder
 *
 */
void vitaSAS_decoder_resume_playback(VitaSAS_Decoder* decoderInfo);

/**
 * Stop decoder playback
 *
 * @param[in] decoderInfo - information structure of decoder
 *
 */
void vitaSAS_decoder_stop_playback(VitaSAS_Decoder* decoderInfo);

/**
 * Seek decode position
 *
 * @param[in] decoderInfo - information structure of decoder
 * @param[in] nEsSamples - position in elementary stream samples to seek
 *
 */
void vitaSAS_decoder_seek(VitaSAS_Decoder* decoderInfo, unsigned int nEsSamples);

/**
 * Decode to buffer
 *
 * @param[in] decoderInfo - information structure of decoder
 * @param[in] begEsSamples - starting position in elementary stream samples
 * @param[in] nEsSamples - size of data to decode in elementary stream samples
 * @param[out] buffer - pointer to buffer to hold decoded PCM data
 *
 */
void vitaSAS_decode_to_buffer(VitaSAS_Decoder* decoderInfo, unsigned int begEsSamples, unsigned int nEsSamples, uint8_t* buffer);

/**
 * Get current position
 *
 * @param[in] decoderInfo - information structure of decoder
 *
 * @return current position in elementary stream samples, <0 on error.
 */
unsigned int vitaSAS_decoder_get_current_es_offset(VitaSAS_Decoder* decoderInfo);

/**
 * Get decoding end state
 *
 * @param[in] decoderInfo - information structure of decoder
 *
 * @return 0 if decoding is not finished, 1 if decoding is finished, <0 on error.
 */
unsigned int vitaSAS_decoder_get_end_state(VitaSAS_Decoder* decoderInfo);

/*----------------------------- Voices -----------------------------*/

/**
 * Set VAG voice from preloaded voice data
 *
 * @param[in] voiceID - voice ID to assign to the new voice
 * @param[in] info - SAS voice information structure
 * @param[in] voiceParam - initial voice parameters
 *
 */
void vitaSAS_set_voice_VAG(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam);

/**
 * Set PCM voice from preloaded voice data
 *
 * @param[in] voiceID - voice ID to assign to the new voice
 * @param[in] info - SAS voice information structure (from .pcm or .wav)
 * @param[in] voiceParam - initial voice parameters
 *
 */
void vitaSAS_set_voice_PCM(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam);

/**
 * Set noise generator voice
 *
 * @param[in] voiceID - voice ID to assign to the new voice
 * @param[in] clock - SAS generator clock frequency
 * @param[in] voiceParam - initial voice parameters
 *
 */
void vitaSAS_set_voice_noise(unsigned int voiceID, unsigned int clock, const vitaSASVoiceParam* voiceParam);

/* Effects */

/**
 * Set SAS system reverb voice effect
 *
 * @param[in] effectType - one of SAS system FX effect types
 * @param[in] volL - volume of the left FX reverb channel
 * @param[in] volR - volume of the right FX reverb channel
 * @param[in] delayTime - FX delay time
 * @param[in] feedbackLevel - FX feedback level
 *
 */
void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel);

/**
 * Clean all effects
 *
 */
void vitaSAS_reset_effect(void);

/**
 * Set SAS switch configuration (ON/OFF effects)
 *
 * @param[in] drySwitch - Dry channel switch (0 - OFF, 1 - ON)
 * @param[in] wetSwitch - Wet channel switch (0 - OFF, 1 - ON)
 *
 */
void vitaSAS_set_switch_config(unsigned int drySwitch, unsigned int wetSwitch);

/**
 * Wrapper call for sceSasSetPitch(), set voice pitch
 *
 * @param[in] voiceID - voice ID to set pitch for
 * @param[in] pitch - pitch value
 *
 */
void vitaSAS_set_pitch(unsigned int voiceID, unsigned int pitch);

/**
 * Wrapper call for sceSasSetVolume(), set voice volume
 *
 * @param[in] voiceID - voice ID to set volume for
 * @param[in] volLDry - volume of left dry channel
 * @param[in] volRDry - volume of right dry channel
 * @param[in] volLWet - volume of left wet channel
 * @param[in] volRWet - volume of right wet channel
 *
 */
void vitaSAS_set_volume(unsigned int voiceID, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet);

/**
 * Wrapper call for sceSasSetSimpleADSR(), set voice simple envelope
 *
 * @param[in] voiceID - voice ID to set volume for
 * @param[in] adsr1 - envelope value 1
 * @param[in] adsr2 - envelope value 2
 *
 */
void vitaSAS_set_simple_ADSR(unsigned int voiceID, unsigned int adsr1, unsigned int adsr2);

/**
 * Wrapper call for sceSasSetSL(), set voice sustain level
 *
 * @param[in] voiceID - voice ID to set volume for
 * @param[in] sustainLevel - sustain level
 *
 */
void vitaSAS_set_SL(unsigned int voiceID, unsigned int sustainLevel);

/**
 * Wrapper call for sceSasSetADSRmode(), set voice envelope curve type
 *
 * @param[in] voiceID - voice ID to set volume for
 * @param[in] flag - setting flag
 * @param[in] a - attack curve type
 * @param[in] d - decay curve type
 * @param[in] s - sustain curve type
 * @param[in] r - release curve type
 *
 */
void vitaSAS_set_ADSR_mode(unsigned int voiceID, unsigned int flag, unsigned int a, unsigned int d, unsigned int s, unsigned int r);

/**
 * Wrapper call for sceSasSetADSR(), set voice envelope curve rates
 *
 * @param[in] voiceID - voice ID to set volume for
 * @param[in] flag - setting flag
 * @param[in] a - attack rate
 * @param[in] d - decay rate
 * @param[in] s - sustain rate
 * @param[in] r - release rate
 *
 */
void vitaSAS_set_ADSR(unsigned int voiceID, unsigned int flag, unsigned int a, unsigned int d, unsigned int s, unsigned int r);

/*----------------------------- Internal functions -----------------------------*/

void vitaSAS_internal_set_initial_params(unsigned int voiceID, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2);

CodecEngineMemBlock* vitaSAS_internal_allocate_memory_for_codec_engine(unsigned int codecType, SceAudiodecCtrl* addecctrl, unsigned int useMainMem);
void vitaSAS_internal_free_memory_for_codec_engine(const CodecEngineMemBlock* codecMemBlock);
void vitaSAS_internal_output_for_decoder(Buffer *pOutput);
int vitaSAS_internal_getFileSize(const char *pInputFileName, uint32_t *pInputFileSize);
int vitaSAS_internal_readFile(const char *pInputFileName, void *pInputBuf, uint32_t inputFileSize);

int vitaSAS_internal_audio_out_start(AudioOutWork *work, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
int vitaSAS_internal_audio_out_stop(AudioOutWork* work);

#ifdef __cplusplus
}
#endif

#endif
