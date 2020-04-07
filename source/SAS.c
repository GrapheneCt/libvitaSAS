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

static void* mspace_internal;

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

	mem_size = ROUND_UP(size, 1024 * 1024);
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
	const char s_sasConfig[] = "";
	void *buffer;
	SceSize bufferSize;
	int result;

	/* Load the SAS module */

	result = sceSysmoduleLoadModule(SCE_SYSMODULE_SAS);

	/* Initialize SAS system */

	result = sceSasGetNeededMemorySize(s_sasConfig, &bufferSize);
	if (SCE_SAS_FAILED(result)) {
		return result;
	}

	buffer = sceClibMspaceMalloc(mspace_internal, bufferSize);
	if (buffer == NULL) {
		return result;
	}

	result = sceSasInit(s_sasConfig, buffer, bufferSize);
	if (SCE_SAS_FAILED(result)) {
		return result;
	}

	/* Start audioout server */

	result = vitaSAS_internal_audio_out_start(&s_audioOut, outputPort, numGrain, vitaSAS_internal_update, thPriority, thStackSize, thCpu);
	if (result < 0) {
		return result;
	}

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

/*vitaSASAudio* vitaSAS_load_audio_AT9(const char* soundPath)
{
	return vitaSAS_internal_load_audio_AT9(soundPath);
}*/

void vitaSAS_create_voice_VAG(unsigned int voiceID, const vitaSASAudio* info, 
	unsigned int loop, unsigned int pitch, unsigned int volRDry, unsigned int volLDry,
	unsigned int volRWet, unsigned int volLWet, unsigned int adsr1, unsigned int adsr2)
{
	/* Set parameters for playing waveform */

	sceSasSetVoice(
		voiceID,
		(char*)info->datap + 48,
		info->data_size - 48,
		loop);
	sceSasSetPitch(voiceID, pitch);
	sceSasSetVolume(voiceID, volLDry, volRDry, volLWet, volRWet);
	sceSasSetSimpleADSR(voiceID, adsr1, adsr2);
}

void vitaSAS_create_voice_PCM(unsigned int voiceID, const vitaSASAudio* info,
	unsigned int loopSize, unsigned int pitch, unsigned int volRDry, unsigned int volLDry,
	unsigned int volRWet, unsigned int volLWet, unsigned int adsr1, unsigned int adsr2)
{
	/* Set parameters for playing waveform */

	sceSasSetVoicePCM(
		voiceID,
		(char*)info->datap,
		info->data_size,
		loopSize);
	sceSasSetPitch(voiceID, pitch);
	sceSasSetVolume(voiceID, volLDry, volRDry, volLWet, volRWet);
	sceSasSetSimpleADSR(voiceID, adsr1, adsr2);
}

