#ifndef VITASAS_H
#define VITASAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <psp2/audiodec.h> 
#include <psp2/audioout.h>

#define ROUND_UP(x, a)	((((unsigned int)x)+((a)-1u))&(~((a)-1u)))
#define VITASAS_GRAIN_MAX 1024
#define SCE_SAS_LOOP_DISABLE_PCM -1
#define VITASAS_USE_MAIN_MEMORY 1
#define VITASAS_USE_PHYCONT_MEMORY 0

/* SAS system limits */

#define MAX_SAS_SYSTEM_NUM			6
#define CHANNEL_MAX					2
#define BUFFER_MAX					2

typedef void(*AudioOutRenderHandler)(void* buffer, int SASSystemNum);

typedef struct AudioOutWork {
	int systemNum;
	SceUID updateThreadId;
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

/* Common */

void vitaSAS_pass_mspace(void* mspace);
int vitaSAS_init(unsigned int openBGM);
int vitaSAS_finish(void);

int vitaSAS_create_system(VitaSASSystemParam* systemInitParam);
int vitaSAS_create_system_with_config(const char* sasConfig, VitaSASSystemParam* systemInitParam);
void vitaSAS_set_sub_system_vol(unsigned int subSystemMixVolL, unsigned int subSystemMixVolR);

void vitaSAS_free_audio(vitaSASAudio* info);

vitaSASAudio* vitaSAS_load_audio_VAG(char* soundPath, int io_type);
vitaSASAudio* vitaSAS_load_audio_PCM(char* soundPath, int io_type);
vitaSASAudio* vitaSAS_load_audio_WAV(char* soundPath, int io_type);
vitaSASAudio* vitaSAS_load_audio_custom(void* pData, unsigned int dataSize);

void vitaSAS_separate_channels_PCM(short* pBufL, short* pBufR, short* pBufSrc, unsigned int bufSrcSize);

int vitaSAS_set_key_on(unsigned int voiceID);
int vitaSAS_set_key_off(unsigned int voiceID);
int vitaSAS_get_end_state(unsigned int voiceID);

/* Codec Engine decoding */

void vitaSAS_destroy_decoder(VitaSAS_Decoder* decoderInfo);
VitaSAS_Decoder* vitaSAS_create_AT9_decoder(const char* soundPath, unsigned int useMainMem);
VitaSAS_Decoder* vitaSAS_create_MP3_decoder(const char* soundPath);
VitaSAS_Decoder* vitaSAS_create_AAC_decoder(const char* soundPath, unsigned int useMainMem);
void vitaSAS_decoder_start_playback(VitaSAS_Decoder* decoderInfo, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
void vitaSAS_decoder_pause_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_resume_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_stop_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_seek(VitaSAS_Decoder* decoderInfo, unsigned int nEsSamples);
void vitaSAS_decode_to_buffer(VitaSAS_Decoder* decoderInfo, unsigned int begEsSamples, unsigned int nEsSamples, uint8_t* buffer);
unsigned int vitaSAS_decoder_get_current_es_offset(VitaSAS_Decoder* decoderInfo);
unsigned int vitaSAS_decoder_get_end_state(VitaSAS_Decoder* decoderInfo);


/* Voices */

void vitaSAS_set_voice_VAG(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam);
void vitaSAS_set_voice_PCM(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam);
void vitaSAS_set_voice_PCM_stereo(unsigned int voiceIDL, unsigned int voiceIDR, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParamL, const vitaSASVoiceParam* voiceParamR);
void vitaSAS_set_voice_noise(unsigned int voiceID, unsigned int clock, const vitaSASVoiceParam* voiceParam);

/* Effects */

void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel);
void vitaSAS_reset_effect(void);

/* Internal functions */

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
