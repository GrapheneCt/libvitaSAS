#ifndef VITASAS_H
#define VITASAS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <psp2/audiodec.h> 

#define ROUND_UP(x, a)	((((unsigned int)x)+((a)-1u))&(~((a)-1u)))
#define VITA_SAS_GRAIN_MAX 1024
#define SCE_SAS_LOOP_DISABLE_PCM -1

typedef struct vitaSASAudio {
	void* datap;
	size_t data_size;
	SceUID data_id;
} vitaSASAudio;

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

/* Common */

void vitaSAS_pass_mspace(void* mspace);
void vitaSAS_finish(void);
int vitaSAS_init(unsigned int outputPort, unsigned int numGrain, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
void vitaSAS_free_audio(vitaSASAudio* info);

vitaSASAudio* vitaSAS_load_audio_VAG(const char* soundPath);
vitaSASAudio* vitaSAS_load_audio_PCM(const char* soundPath);

/* Codec Engine decoding */

void vitaSAS_destroy_decoder(VitaSAS_Decoder* decoderInfo);
VitaSAS_Decoder* vitaSAS_create_AT9_decoder(const char* soundPath);
//VitaSAS_Decoder* vitaSAS_create_MP3_decoder(const char* soundPath);
VitaSAS_Decoder* vitaSAS_create_AAC_decoder(const char* soundPath);
void vitaSAS_decoder_start_playback(VitaSAS_Decoder* decoderInfo, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
void vitaSAS_decoder_pause_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_resume_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_stop_playback(VitaSAS_Decoder* decoderInfo);
void vitaSAS_decoder_seek(VitaSAS_Decoder* decoderInfo, unsigned int nEsSamples);
void vitaSAS_decode_to_buffer(VitaSAS_Decoder* decoderInfo, unsigned int begEsSamples, unsigned int nEsSamples, uint8_t* buffer);
unsigned int vitaSAS_decoder_get_current_es_offset(VitaSAS_Decoder* decoderInfo);


/* Voices */

void vitaSAS_set_voice_VAG(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loop, unsigned int pitch, unsigned int volLDry, unsigned int volRDry,
	unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2);

void vitaSAS_set_voice_PCM(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loopSize, unsigned int pitch, unsigned int volLDry, unsigned int volRDry,
	unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2);

void vitaSAS_set_voice_noise(unsigned int voiceID, unsigned int clock, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2);

/* Effects */

void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel);
void vitaSAS_reset_effect(void);

/* Internal functions */

void vitaSAS_internal_set_initial_params(unsigned int voiceID, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2);

CodecEngineMemBlock* vitaSAS_internal_allocate_memory_for_codec_engine(unsigned int codecType, SceAudiodecCtrl* addecctrl);
void vitaSAS_internal_free_memory_for_codec_engine(const CodecEngineMemBlock* codecMemBlock);
void vitaSAS_internal_output_for_decoder(Buffer *pOutput);
int vitaSAS_internal_getFileSize(const char *pInputFileName, uint32_t *pInputFileSize);
int vitaSAS_internal_readFile(const char *pInputFileName, void *pInputBuf, uint32_t inputFileSize);

#ifdef __cplusplus
}
#endif

#endif
