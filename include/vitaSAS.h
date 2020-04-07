#ifndef VITASAS_H
#define VITASAS_H

#ifdef __cplusplus
extern "C" {
#endif

#define ROUND_UP(x, a)	((((unsigned int)x)+((a)-1u))&(~((a)-1u)))
#define VITA_SAS_GRAIN_MAX 1024
#define SCE_SAS_LOOP_DISABLE_PCM -1

typedef struct vitaSASAudio {
	void* datap;
	size_t data_size;
	SceUID data_id;
} vitaSASAudio;

/* Common */

void vitaSAS_pass_mspace(void* mspace);
void vitaSAS_finish(void);
int vitaSAS_init(unsigned int outputPort, unsigned int numGrain, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
void vitaSAS_free_audio(vitaSASAudio* info);

vitaSASAudio* vitaSAS_load_audio_VAG(const char* soundPath);
vitaSASAudio* vitaSAS_load_audio_PCM(const char* soundPath);

/* Voices */

void vitaSAS_create_voice_VAG(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loop, unsigned int pitch, unsigned int volRDry, unsigned int volLDry,
	unsigned int volRWet, unsigned int volLWet, unsigned int adsr1, unsigned int adsr2);

void vitaSAS_create_voice_PCM(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loop, unsigned int pitch, unsigned int volRDry, unsigned int volLDry,
	unsigned int volRWet, unsigned int volLWet, unsigned int adsr1, unsigned int adsr2);

void vitaSAS_create_voice_noise(unsigned int voiceID, unsigned int clock, unsigned int pitch, unsigned int volRDry,
	unsigned int volLDry, unsigned int volRWet, unsigned int volLWet, unsigned int adsr1, unsigned int adsr2);

void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel);
void vitaSAS_reset_effect(void);

#ifdef __cplusplus
}
#endif

#endif
