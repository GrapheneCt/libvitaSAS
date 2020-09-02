#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/sysmem.h> 
#include <psp2/kernel/clib.h>
#include <psp2/kernel/iofilemgr.h> 
#include <psp2/audioout.h> 
#include <psp2/sysmodule.h>
#include <psp2/libdbg.h>
#include <psp2/sas.h>
#include <psp2/fios2.h>

#include "vitaSAS.h"
#include "heap.h"

static int SASSystenEVF[MAX_SAS_SYSTEM_NUM];

void* heap_internal;
int g_portIdBGM = 0;

static SceUID SASSystemFlagUID = 0;
static int SASCurrentSystemNum = 0;
static vitaSASSystem* SASSystemStorage[MAX_SAS_SYSTEM_NUM];

static unsigned int heap_size = DEFAULT_HEAP_SIZE;

static void* vitaSAS_internal_search_WAV_header(int dataToSearch, void* ptr, int* bytesMoved)
{
	int bytesMovedCount = 0;
	void* new_ptr = ptr;
	while (*(int *)new_ptr != dataToSearch) {
		new_ptr++;
		bytesMovedCount++;
	}

	*bytesMoved = bytesMovedCount;

	return new_ptr;
}

static void vitaSAS_internal_load_audio_WAV_FIOS2(char *mountedFilePath, size_t *outSize, SceUID* mem_id_ret, void** datap)
{
	void *data, *header;
	SceUID mem_id;
	SceFiosFH file;
	SceFiosSize size;
	int result, offset, headerSize;
	unsigned int mem_size;
	mem_id = 0;

	file = 0;
	data = NULL;
	header = NULL;

	result = sceFiosFHOpenSync(NULL, &file, mountedFilePath, NULL);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't open %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, result);
		goto failed;
	}

	header = heap_alloc_heap_memory(heap_internal, 64);

	result = sceFiosFHReadSync(NULL, file, header, 64);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read %s sceFiosFHReadSync(): 0x%X", mountedFilePath, result);
		goto failed;
	}

	header = vitaSAS_internal_search_WAV_header(0x20746D66, header, &headerSize);

	if (*(short *)(header + 10) > 1) {
		SCE_DBG_LOG_WARNING("[SAS IO] Only mono files are supported");
		goto failed;
	}

	if (*(short *)(header + 22) != 16) {
		SCE_DBG_LOG_WARNING("[SAS IO] Only 16-bit files are supported");
		goto failed;
	}

	int bytesMoved;
	header = vitaSAS_internal_search_WAV_header(0x61746164, header, &bytesMoved);
	headerSize += bytesMoved;

	size = *(uint32_t *)(header + 4);
	offset = headerSize + 8;

	heap_free_heap_memory(heap_internal, header);

	mem_size = ROUND_UP(size, 4 * 1024);
	mem_id = sceKernelAllocMemBlock("vitaSAS_sample_storage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mem_size, NULL);

	if (mem_id < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceKernelAllocMemBlock(): 0x%X", mem_id);
		goto failed;
	}

	sceKernelGetMemBlockBase(mem_id, &data);

	result = sceFiosFHPreadSync(NULL, file, data, size, (int64_t)offset);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read %s sceFiosFHReadSync(): 0x%X", mountedFilePath, result);
		goto failed;
	}

	sceFiosFHCloseSync(NULL, file);

	*outSize = (size_t)result;
	*mem_id_ret = mem_id;
	*datap = data;

	return;

failed:

	if (header != NULL)
		heap_free_heap_memory(heap_internal, header);

	if (mem_id != 0)
		sceKernelFreeMemBlock(mem_id);

	if (0 < file) {
		sceFiosFHCloseSync(NULL, file);
	}

	return;
}

static void *vitaSAS_internal_load_audio_FIOS2(char *mountedFilePath, size_t *outSize, SceUID* mem_id_ret)
{
	void *data;
	SceUID mem_id;
	SceFiosFH file;
	SceFiosSize size;
	int result;
	unsigned int mem_size;
	mem_id = 0;

	file = 0;
	data = NULL;

	SceFiosStat stat;
	result = sceFiosStatSync(NULL, mountedFilePath, &stat);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceFiosStatSync(): 0x%X", result);
		goto failed;
	}

	size = stat.fileSize;

	result = sceFiosFHOpenSync(NULL, &file, mountedFilePath, NULL);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't open %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, result);
		goto failed;
	}

	mem_size = ROUND_UP(size, 4 * 1024);
	mem_id = sceKernelAllocMemBlock("vitaSAS_sample_storage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mem_size, NULL);

	if (mem_id < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceKernelAllocMemBlock(): 0x%X", mem_id);
		goto failed;
	}

	sceKernelGetMemBlockBase(mem_id, &data);

	result = sceFiosFHReadSync(NULL, file, data, size);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read %s sceFiosFHReadSync(): 0x%X", mountedFilePath, result);
		goto failed;
	}

	sceFiosFHCloseSync(NULL, file);

	if (outSize) *outSize = size;
	*mem_id_ret = mem_id;

	return data;

failed:

	sceKernelFreeMemBlock(mem_id);

	if (0 < file) {
		sceFiosFHCloseSync(NULL, file);
	}

	return NULL;
}

static void *vitaSAS_internal_load_audio(char *path, size_t *outSize, SceUID* mem_id_ret, int io_type)
{
	if (io_type == 1)
		return vitaSAS_internal_load_audio_FIOS2(path, outSize, mem_id_ret);

	void *data;
	SceUID file, mem_id;
	int result, size;
	unsigned int mem_size;
	mem_id = 0;

	file = 0;
	data = NULL;

	SceIoStat stat;

	result = sceIoGetstat(path, &stat);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceIoGetstat(): 0x%X", result);
		goto failed;
	}

	size = (int)stat.st_size;

	file = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (file < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't open %s sceIoOpen(): 0x%X", path, file);
		goto failed;
	}

	mem_size = ROUND_UP(size, 4 * 1024);
	mem_id = sceKernelAllocMemBlock("vitaSAS_sample_storage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mem_size, NULL);

	if (mem_id < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceKernelAllocMemBlock(): 0x%X", mem_id);
		goto failed;
	}

	sceKernelGetMemBlockBase(mem_id, &data);

	result = sceIoRead(file, data, size);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read %s sceIoRead(): 0x%X", path, result);
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

static void vitaSAS_internal_load_audio_WAV(char *path, size_t *outSize, SceUID* mem_id_ret, void** datap, int io_type)
{
	if (io_type == 1)
		return vitaSAS_internal_load_audio_WAV_FIOS2(path, outSize, mem_id_ret, datap);

	void *data, *header;
	SceUID file, mem_id;
	int result, offset, headerSize;
	unsigned int mem_size, size;
	mem_id = 0;

	file = 0;
	data = NULL;
	header = NULL;

	file = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (file < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't open %s sceIoOpen(): 0x%X", path, file);
		goto failed;
	}

	header = heap_alloc_heap_memory(heap_internal, 64);

	result = sceIoRead(file, header, 64);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read header %s sceIoRead(): 0x%X", path, result);
		goto failed;
	}

	header = vitaSAS_internal_search_WAV_header(0x20746D66, header, &headerSize);

	if (*(short *)(header + 10) > 1) {
		SCE_DBG_LOG_WARNING("[SAS IO] Only mono files are supported");
		goto failed;
	}

	if (*(short *)(header + 22) != 16) {
		SCE_DBG_LOG_WARNING("[SAS IO] Only 16-bit files are supported");
		goto failed;
	}

	int bytesMoved;
	header = vitaSAS_internal_search_WAV_header(0x61746164, header, &bytesMoved);
	headerSize += bytesMoved;

	size = *(uint32_t *)(header + 4);
	offset = headerSize + 8;

	heap_free_heap_memory(heap_internal, header);

	mem_size = ROUND_UP(size, 4 * 1024);
	mem_id = sceKernelAllocMemBlock("vitaSAS_sample_storage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mem_size, NULL);

	if (mem_id < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] sceKernelAllocMemBlock(): 0x%X", mem_id);
		goto failed;
	}

	sceKernelGetMemBlockBase(mem_id, &data);

	result = sceIoPread(file, data, size, offset);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS IO] Can't read %s sceIoRead(): 0x%X", path, result);
		goto failed;
	}

	sceIoClose(file);

	*outSize = result;
	*mem_id_ret = mem_id;
	*datap = data;

	return;

failed:

	if (header != NULL)
		heap_free_heap_memory(heap_internal, header);

	if (mem_id != 0)
		sceKernelFreeMemBlock(mem_id);

	if (0 < file) {
		sceIoClose(file);
	}

	return;
}

int vitaSAS_finish(void)
{
	sceKernelDeleteEventFlag(SASSystemFlagUID);

	/* Release decoder BGM port */

	if (g_portIdBGM > 0)
		sceAudioOutReleasePort(g_portIdBGM);

	/* Delete heap */

	heap_delete_heap(heap_internal);

	/* Unload the SAS module */

	return sceSysmoduleUnloadModule(SCE_SYSMODULE_SAS);
}

void vitaSAS_set_heap_size(unsigned int size)
{
	heap_size = size;
}

void vitaSAS_internal_update(void* buffer, int SASSystemNum)
{
	/* Rendering audio frame (grain[samples]) */

	if (SASSystemStorage[SASSystemNum]->subSystemNum == -1)
		sceSasCoreInternal(SASSystemStorage[SASSystemNum]->sasSystemHandle, buffer, 0, 0);
	else {
		vitaSAS_internal_update(buffer, SASSystemStorage[SASSystemNum]->subSystemNum);
		sceSasCoreInternal(SASSystemStorage[SASSystemNum]->sasSystemHandle, buffer, SASSystemStorage[SASSystemNum]->subSystemMixVolL, SASSystemStorage[SASSystemNum]->subSystemMixVolR);
	}
}

int vitaSAS_init(unsigned int openBGM)
{
	int SASFlag = 1;

	/* Initialize heap */

	heap_internal = heap_create_heap("vitaSAS_heap", heap_size, HEAP_AUTO_EXTEND, NULL);

	/* Open BGM port for Codec Engine decoders */

	if (openBGM) {
		g_portIdBGM = sceAudioOutOpenPort(
			SCE_AUDIO_OUT_PORT_TYPE_BGM,
			256, //This will be changed by decoder if needed
			48000, //This will be changed by decoder if needed
			SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO); //This will be changed by decoder if needed
		if (g_portIdBGM < 0)
			SCE_DBG_LOG_ERROR("[DEC] sceAudioOutOpenPort(): 0x%X", g_portIdBGM);
	}

	/* Initialize SAS system position flag */

	SASSystemFlagUID = sceKernelCreateEventFlag("SASSystemPostionTracker", SCE_KERNEL_ATTR_MULTI, 0, NULL);

	for (int i = 0; i < MAX_SAS_SYSTEM_NUM; i++) {
		SASSystenEVF[i] = SASFlag;
		SASFlag *= 2;
	}

	return sceSysmoduleLoadModule(SCE_SYSMODULE_SAS);
}

void vitaSAS_set_sub_system_vol(unsigned int subSystemMixVolL, unsigned int subSystemMixVolR)
{
	SASSystemStorage[SASCurrentSystemNum]->subSystemMixVolL = subSystemMixVolL;
	SASSystemStorage[SASCurrentSystemNum]->subSystemMixVolR = subSystemMixVolR;
}

void vitaSAS_select_system(int systemNum)
{
	SASCurrentSystemNum = systemNum;
}

SceUID vitaSAS_get_system_handle(void)
{
	return SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle;
}

void vitaSAS_destroy_system(void)
{
	size_t bufferSize;
	void *buffer;

	/* Stop audio out server*/

	if (!SASSystemStorage[SASCurrentSystemNum]->isSubSystem)
		vitaSAS_internal_audio_out_stop(&SASSystemStorage[SASCurrentSystemNum]->audioWork);

	/* Exit SAS system */

	sceSasExitInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, &buffer, &bufferSize);

	heap_free_heap_memory(heap_internal, buffer);
	heap_free_heap_memory(heap_internal, SASSystemStorage[SASCurrentSystemNum]);

	/* Unregister SAS system position */

	sceKernelClearEventFlag(SASSystemFlagUID, ~SASSystenEVF[SASCurrentSystemNum]);
}

int vitaSAS_create_system_with_config(const char* sasConfig, VitaSASSystemParam* systemInitParam)
{
	void *buffer = NULL;
	SceSize bufferSize;
	int result = -1;

	/* Check input parameters */

	if (systemInitParam->outputPort != SCE_AUDIO_OUT_PORT_TYPE_MAIN
		&& systemInitParam->outputPort != SCE_AUDIO_OUT_PORT_TYPE_BGM) {
		SCE_DBG_LOG_ERROR("[SAS] Invalid port type");
		return -1;
	}

	if (VITASAS_GRAIN_MAX < systemInitParam->numGrain) {
		SCE_DBG_LOG_ERROR("[SAS] Invalid grain value");
		return -1;
	}

	/* Create SAS system instance */

	vitaSASSystem* system = heap_alloc_heap_memory(heap_internal, sizeof(vitaSASSystem));
	if (system == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] heap_alloc_heap_memory() returned NULL");
		return -1;
	}

	/* Clear work */

	sceClibMemset(&system->audioWork, 0, sizeof(AudioOutWork));

	/* Prepair work */

	system->audioWork.outputPort = systemInitParam->outputPort;
	system->audioWork.numGrain = systemInitParam->numGrain;
	system->audioWork.outputSamplingRate = systemInitParam->samplingRate;
	system->audioWork.renderHandler = vitaSAS_internal_update;

	/* Search vacant SAS system position and resister new system */

	for (int i = 0; i < MAX_SAS_SYSTEM_NUM; i++) {
		result = sceKernelPollEventFlag(SASSystemFlagUID, SASSystenEVF[i], SCE_KERNEL_EVF_WAITMODE_AND, NULL);
		if (result < 0) {
			SCE_DBG_LOG_DEBUG("[SAS] Found free position: %d", i);
			sceKernelSetEventFlag(SASSystemFlagUID, SASSystenEVF[i]);
			system->audioWork.systemNum = i;
			system->systemNum = i;
			SASSystemStorage[i] = system;
			break;
		}
	}

	if (result == 0) {
		SCE_DBG_LOG_WARNING("[SAS] Can't register new system");
		goto error;
	}

	system->isSubSystem = systemInitParam->isSubSystem;
	system->subSystemNum = systemInitParam->subSystemNum;
	system->subSystemMixVolL = systemInitParam->subSystemMixVolL;
	system->subSystemMixVolR = systemInitParam->subSystemMixVolR;

	/* Initialize SAS system */

	result = sceSasGetNeededMemorySizeInternal(sasConfig, &bufferSize);
	if (SCE_SAS_FAILED(result)) {
		SCE_DBG_LOG_ERROR("[SAS] sceSasGetNeededMemorySizeInternal(): 0x%X", result);
		goto error;
	}

	SCE_DBG_LOG_DEBUG("[SAS] SAS system requested: %f MB", (float)bufferSize / 1024.0f / 1024.0f);

	buffer = heap_alloc_heap_memory(heap_internal, bufferSize);
	if (buffer == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] heap_alloc_heap_memory() returned NULL");
		goto error;
	}

	result = sceSasInitInternal(sasConfig, buffer, bufferSize, &system->sasSystemHandle);
	if (SCE_SAS_FAILED(result)) {
		SCE_DBG_LOG_ERROR("[SAS] sceSasInitWithGrainInternal(): 0x%X", result);
		goto error;
	}

	result = sceSasSetGrainInternal(system->sasSystemHandle, systemInitParam->numGrain);
	if (SCE_SAS_FAILED(result)) {
		SCE_DBG_LOG_ERROR("[SAS] sceSasSetGrainInternal(): 0x%X", result);
		goto error;
	}

	/* Create audio out thread pause flag */

	system->audioWork.eventFlagId = sceKernelCreateEventFlag("SASSystemRenderPauseFlag", SCE_KERNEL_ATTR_MULTI, 1, NULL);

	/* Start audioout server */

	if (!system->isSubSystem) {

		result = vitaSAS_internal_audio_out_start(&system->audioWork, systemInitParam->thPriority, systemInitParam->thStackSize, systemInitParam->thCpu);

		if (result < 0) {
			SCE_DBG_LOG_ERROR("[SAS] vitaSAS_internal_audio_out_start(): 0x%X", result);
			goto error;
		}

	}

	result = system->systemNum;

	return result;

error:

	if (buffer != NULL)
		heap_free_heap_memory(heap_internal, buffer);
	heap_free_heap_memory(heap_internal, system);
	return -1;
}

int vitaSAS_pause_system_render(void)
{
	return sceKernelClearEventFlag(SASSystemStorage[SASCurrentSystemNum]->audioWork.eventFlagId, ~1);
}

int vitaSAS_resume_system_render(void)
{
	return sceKernelSetEventFlag(SASSystemStorage[SASCurrentSystemNum]->audioWork.eventFlagId, 1);
}

int vitaSAS_create_system(VitaSASSystemParam* systemInitParam)
{
	return vitaSAS_create_system_with_config("", systemInitParam);
}

void vitaSAS_free_audio(vitaSASAudio* info)
{
	if (info->data_id)
		sceKernelFreeMemBlock(info->data_id);
	heap_free_heap_memory(heap_internal, info);
}

vitaSASAudio* vitaSAS_load_audio_custom(void* pData, unsigned int dataSize)
{
	vitaSASAudio* info = heap_alloc_heap_memory(heap_internal, sizeof(vitaSASAudio));
	if (info == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	/* Load sound */

	info->datap = pData;
	info->data_size = dataSize;
	info->data_id = 0;

	if (info->datap == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] Invalid data pointer");
		return NULL;
	}

	return info;
}

vitaSASAudio* vitaSAS_load_audio_WAV(char* soundPath, int io_type)
{
	vitaSASAudio* info = heap_alloc_heap_memory(heap_internal, sizeof(vitaSASAudio));
	if (info == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	/* Load sound */

	size_t soundDataSize = 0;
	SceUID mem_id = 0;
	vitaSAS_internal_load_audio_WAV(soundPath, &soundDataSize, &mem_id, &info->datap, io_type);
	info->data_size = soundDataSize;
	info->data_id = mem_id;

	return info;
}

vitaSASAudio* vitaSAS_load_audio_VAG(char* soundPath, int io_type)
{
	vitaSASAudio* info = heap_alloc_heap_memory(heap_internal, sizeof(vitaSASAudio));
	if (info == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	/* Load sound */

	size_t soundDataSize = 0;
	SceUID mem_id = 0;
	info->datap = vitaSAS_internal_load_audio(soundPath, &soundDataSize, &mem_id, io_type);
	info->data_size = soundDataSize;
	info->data_id = mem_id;

	if (info->datap == NULL) {
		SCE_DBG_LOG_ERROR("[SAS] vitaSAS_internal_load_audio() returned NULL");
		return NULL;
	}

	return info;
}

vitaSASAudio* vitaSAS_load_audio_PCM(char* soundPath, int io_type)
{
	return vitaSAS_load_audio_VAG(soundPath, io_type);
}

void vitaSAS_internal_set_initial_params(unsigned int voiceID, unsigned int pitch, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet, unsigned int adsr1, unsigned int adsr2)
{
	sceSasSetPitchInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, pitch);
	sceSasSetVolumeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, volLDry, volRDry, volLWet, volRWet);
	sceSasSetSimpleADSRInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, adsr1, adsr2);
}

void vitaSAS_set_voice_VAG(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam)
{
	/* Set parameters for playing waveform */

	sceSasSetVoiceInternal(
		SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle,
		voiceID,
		(char*)info->datap + 48,
		info->data_size - 48,
		voiceParam->loop);
	vitaSAS_internal_set_initial_params(voiceID, voiceParam->pitch, voiceParam->volLDry, voiceParam->volRDry, voiceParam->volLWet, voiceParam->volRWet, voiceParam->adsr1, voiceParam->adsr2);
}

void vitaSAS_set_voice_PCM(unsigned int voiceID, const vitaSASAudio* info, const vitaSASVoiceParam* voiceParam)
{
	/* Set parameters for playing waveform */

	int numSamples = info->data_size / 2;

	sceSasSetVoicePCMInternal(
		SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle,
		voiceID,
		(char*)info->datap,
		numSamples,
		voiceParam->loopSize);
	vitaSAS_internal_set_initial_params(voiceID, voiceParam->pitch, voiceParam->volLDry, voiceParam->volRDry, voiceParam->volLWet, voiceParam->volRWet, voiceParam->adsr1, voiceParam->adsr2);
}

void vitaSAS_set_voice_noise(unsigned int voiceID, unsigned int clock, const vitaSASVoiceParam* voiceParam)
{
	/* Set parameters for playing waveform */

	sceSasSetNoiseInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, clock);
	vitaSAS_internal_set_initial_params(voiceID, voiceParam->pitch, voiceParam->volLDry, voiceParam->volRDry, voiceParam->volLWet, voiceParam->volRWet, voiceParam->adsr1, voiceParam->adsr2);
}

void vitaSAS_set_effect(unsigned int effectType, unsigned int volL, unsigned int volR, unsigned int delayTime, unsigned int feedbackLevel)
{
	sceSasSetEffectTypeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, effectType);
	sceSasSetEffectParamInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, delayTime, feedbackLevel);
	sceSasSetEffectVolumeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, volL, volR);
}

void vitaSAS_reset_effect(void)
{
	sceSasSetEffectTypeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, SCE_SAS_FX_TYPE_OFF);
}

void vitaSAS_set_switch_config(unsigned int drySwitch, unsigned int wetSwitch)
{
	sceSasSetEffectInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, drySwitch, wetSwitch);
}

void vitaSAS_set_pitch(unsigned int voiceID, unsigned int pitch)
{
	sceSasSetPitchInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, pitch);
}

void vitaSAS_set_volume(unsigned int voiceID, unsigned int volLDry,
	unsigned int volRDry, unsigned int volLWet, unsigned int volRWet)
{
	sceSasSetVolumeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, volLDry, volRDry, volLWet, volRWet);
}

void vitaSAS_set_simple_ADSR(unsigned int voiceID, unsigned int adsr1, unsigned int adsr2)
{
	sceSasSetSimpleADSRInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, adsr1, adsr2);
}

void vitaSAS_set_SL(unsigned int voiceID, unsigned int sustainLevel)
{
	sceSasSetSLInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, sustainLevel);
}

void vitaSAS_set_ADSR_mode(unsigned int voiceID, unsigned int flag, unsigned int a, unsigned int d, unsigned int s, unsigned int r)
{
	sceSasSetADSRmodeInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, flag, a, d, s, r);
}

void vitaSAS_set_ADSR(unsigned int voiceID, unsigned int flag, unsigned int a, unsigned int d, unsigned int s, unsigned int r)
{
	sceSasSetADSRInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID, flag, a, d, s, r);
}

int vitaSAS_set_key_on(unsigned int voiceID)
{
	return sceSasSetKeyOnInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID);
}

int vitaSAS_set_key_off(unsigned int voiceID)
{
	return sceSasSetKeyOffInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID);
}

int vitaSAS_get_end_state(unsigned int voiceID)
{
	return sceSasGetEndStateInternal(SASSystemStorage[SASCurrentSystemNum]->sasSystemHandle, voiceID);
}

