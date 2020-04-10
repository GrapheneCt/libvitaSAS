#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/sysmem.h> 
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h> 
#include <psp2/audioout.h> 
#include <psp2/sysmodule.h>
#include <psp2/sas.h>

#include "audio_out.h"
#include "vitaSAS.h"

extern void* sceClibMspaceMalloc(void* space, unsigned int size);
extern void sceClibMspaceFree(void* space, void* ptr);

static AudioOutWork s_audioOut;

void* mspace_internal;
unsigned int g_portIdBGM;

static void *vitaSAS_internal_load_audio(const char *path, size_t *outSize, SceUID* mem_id_ret)
{
	void *data;
	SceUID file, mem_id;
	int result, size;
	unsigned int mem_size;
	mem_id = 0;

	file = 0;
	data = NULL;

	file = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (file < 0) {
		goto failed;
	}

	size = sceIoLseek(file, 0, SCE_SEEK_END);
	if (size < 0) {
		goto failed;
	}

	result = sceIoLseek(file, 0, SCE_SEEK_SET);
	if (result < 0) {
		goto failed;
	}

	mem_size = ROUND_UP(size, 4 * 1024);
	mem_id = sceKernelAllocMemBlock("vitaSAS_sample_storage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, mem_size, NULL);
	sceKernelGetMemBlockBase(mem_id, &data);

	result = sceIoRead(file, data, size);
	if (result < 0) {
		goto failed;
	}

	sceIoClose(file);

	if (outSize) *outSize = size;
	*mem_id_ret = mem_id;

	return data;

failed:

	sceKernelFreeMemBlock(mem_id);

	if (0 < file) {
		sceIoClose(file);
	}

	return NULL;
}

void vitaSAS_finish(void)
{
	size_t bufferSize;
	void *buffer;

	/* Stop audio out server*/

	vitaSAS_internal_audio_out_stop(&s_audioOut);

	/* Release decoder BGM port */

	sceAudioOutReleasePort(g_portIdBGM);

	/* Exit SAS system */

	sceSasExit(&buffer, &bufferSize);

	sceClibMspaceFree(mspace_internal, buffer);

	/* Unload the SAS modules */

	sceSysmoduleUnloadModule(SCE_SYSMODULE_SAS);
}

void vitaSAS_pass_mspace(void* mspace)
{
	mspace_internal = mspace;
}

void vitaSAS_internal_update(void* buffer)
{
	/* Rendering audio frame (grain[samples]) */

	sceSasCore(buffer);
}

int vitaSAS_init(unsigned int outputPort, unsigned int numGrain, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu)
{
	const char sasConfig[] = "";
	void *buffer;
	SceSize bufferSize;
	int result;

	/* Load the SAS module */

	result = sceSysmoduleLoadModule(SCE_SYSMODULE_SAS);

	/* Initialize SAS system */

	result = sceSasGetNeededMemorySize(sasConfig, &bufferSize);
	if (SCE_SAS_FAILED(result)) {
		return result;
	}

	buffer = sceClibMspaceMalloc(mspace_internal, bufferSize);
	if (buffer == NULL) {
		return result;
	}

	result = sceSasInitWithGrain(sasConfig, numGrain, buffer, bufferSize);
	if (SCE_SAS_FAILED(result)) {
		return result;
	}

	/* Start audioout server */

	result = vitaSAS_internal_audio_out_start(&s_audioOut, outputPort, numGrain, vitaSAS_internal_update, thPriority, thStackSize, thCpu);
	if (result < 0) {
		return result;
	}

	/* Open BGM port for Codec Engine decoders */

	g_portIdBGM = sceAudioOutOpenPort(
		SCE_AUDIO_OUT_PORT_TYPE_BGM,
		256, //This will be changed by decoder if needed
		48000, //This will be changed by decoder if needed
		SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO); //This will be changed by decoder if needed

	return result;
}

void vitaSAS_free_audio(vitaSASAudio* info)
{
	sceKernelFreeMemBlock(info->data_id);
	sceClibMspaceFree(mspace_internal, info);
}

vitaSASAudio* vitaSAS_load_audio_VAG(const char* soundPath)
{
	vitaSASAudio* info = sceClibMspaceMalloc(mspace_internal, sizeof(vitaSASAudio));

	/* Load sound */

	size_t soundDataSize = 0;
	SceUID mem_id = 0;
	info->datap = vitaSAS_internal_load_audio(soundPath, &soundDataSize, &mem_id);
	info->data_size = soundDataSize;
	info->data_id = mem_id;

	if (info->datap == NULL)
		return NULL;

	return info;
}

vitaSASAudio* vitaSAS_load_audio_PCM(const char* soundPath)
{
	return vitaSAS_load_audio_VAG(soundPath);
}

void vitaSAS_internal_set_initial_params(unsigned int voiceID, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2)
{
	sceSasSetPitch(voiceID, pitch);
	sceSasSetVolume(voiceID, volLDry, volRDry, volLWet, volRWet);
	sceSasSetSimpleADSR(voiceID, adsr1, adsr2);
}

void vitaSAS_set_voice_VAG(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loop, unsigned int pitch, unsigned int volLDry, unsigned int volRDry,
	unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2)
{
	/* Set parameters for playing waveform */

	sceSasSetVoice(
		voiceID,
		(char*)info->datap + 48,
		info->data_size - 48,
		loop);
	vitaSAS_internal_set_initial_params(voiceID, pitch, volLDry, volRDry, volLWet, volRWet, adsr1, adsr2);
}

void vitaSAS_set_voice_PCM(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loopSize, unsigned int pitch, unsigned int volLDry, unsigned int volRDry,
	unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2)
{
	/* Set parameters for playing waveform */

	sceSasSetVoicePCM(
		voiceID,
		(char*)info->datap,
		info->data_size,
		loopSize);
	vitaSAS_internal_set_initial_params(voiceID, pitch, volLDry, volRDry, volLWet, volRWet, adsr1, adsr2);
}

void vitaSAS_set_voice_noise(unsigned int voiceID, unsigned int clock, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2)
{
	/* Set parameters for playing waveform */

	sceSasSetNoise(voiceID, clock);
	vitaSAS_internal_set_initial_params(voiceID, pitch, volLDry, volRDry, volLWet, volRWet, adsr1, adsr2);
}

void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel)
{
	sceSasSetEffectType(effectType);
	sceSasSetEffectParam(delayTime, feedbackLevel);
	sceSasSetEffectVolume(volL, volR);
	sceSasSetEffect(0, 1);
}

void vitaSAS_reset_effect(void)
{
	sceSasSetEffectType(SCE_SAS_FX_TYPE_OFF);
	sceSasSetEffect(1, 0);
}

