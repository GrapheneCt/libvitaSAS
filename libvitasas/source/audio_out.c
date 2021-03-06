﻿#include <audioout.h> 
#include <libdbg.h> 
#include <kernel.h> 

#include "vitaSAS.h"
#include "heap.h"

extern unsigned int g_portIdBGM;
extern void* vitaSAS_heap_internal;

void vitaSAS_internal_output_for_decoder(Buffer *pOutput)
{
	if (pOutput != NULL) {

		/* Output audio data*/

		sceAudioOutOutput(g_portIdBGM, pOutput->op[pOutput->bufIndex]);

		/* Swap buffers */

		pOutput->bufIndex ^= 1;
	}
	else {
		
		/* Output remaining audio data */

		sceAudioOutOutput(g_portIdBGM, NULL);
	}
}

int vitaSAS_internal_update_thread(unsigned int args, void *argc)
{
	AudioOutWork *work;
	unsigned int bufferId;
	int result, aVolume[CHANNEL_MAX], portId;

	work = *(AudioOutWork**)argc;

	short* aBuffer[BUFFER_MAX];
	aBuffer[0] = heap_alloc_heap_memory(vitaSAS_heap_internal, work->numGrain * 4);
	aBuffer[1] = heap_alloc_heap_memory(vitaSAS_heap_internal, work->numGrain * 4);

	/* Open audio out port */

	result = portId = sceAudioOutOpenPort(
			work->outputPort,
			work->numGrain,
			work->outputSamplingRate,
			SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS] sceAudioOutOpenPort(): 0x%X", result);
		goto abort;
	}

	work->portId = portId;

	/* Set volume */

	aVolume[0] = aVolume[1] = SCE_AUDIO_VOLUME_0DB;

	sceAudioOutSetVolume(portId, (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), aVolume);

	/* Output loop */

	bufferId = 0;

	while (work->isAborted == 0)
	{
		/* Check render pause flag */

		sceKernelWaitEventFlag(work->eventFlagId, 1, SCE_KERNEL_EVF_WAITMODE_AND, NULL, NULL);

		/* Call rendering handler */

		(*work->renderHandler)(aBuffer[bufferId], work->systemNum);

		/* Output audio */

		result = sceAudioOutOutput(portId, aBuffer[bufferId]);
		if (result < 0) {
			goto abort;
		}

		/* Swap buffer */

		bufferId ^= 1;
	}

abort:

	/* Free buffers */

	heap_free_heap_memory(vitaSAS_heap_internal, aBuffer[0]);
	heap_free_heap_memory(vitaSAS_heap_internal, aBuffer[1]);

	/* Flush buffer */

	sceAudioOutOutput(portId, NULL);

	/* Release audio output port */

	result = sceAudioOutReleasePort(portId);

	SCE_DBG_LOG_DEBUG("[SAS] SAS sytem audio output has been aborted");

	return sceKernelExitDeleteThread(0);
}

int vitaSAS_internal_audio_out_start(AudioOutWork *work, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu)
{
	int result;

	/* Create update thread */

	result = work->updateThreadId = sceKernelCreateThread(
			"vitaSAS_audio_out_thread",
			vitaSAS_internal_update_thread,
			thPriority,
			thStackSize,
			0,
			thCpu,
			NULL);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS] sceKernelCreateThread(): 0x%X", result);
		goto failed;
	}

	/* Start update thread */

	result = sceKernelStartThread(work->updateThreadId, sizeof(work), &work);
	if (result < 0) {
		SCE_DBG_LOG_ERROR("[SAS] sceKernelStartThread(): 0x%X", result);
		goto failed;
	}

	return 0;

failed:

	if (0 < work->updateThreadId) {
		sceKernelDeleteThread(work->updateThreadId);
		work->updateThreadId = 0;
	}

	return result;
}

int vitaSAS_internal_audio_out_stop(AudioOutWork* work)
{
	/* Shutdown update thread */

	if (0 < work->updateThreadId) {
		work->isAborted = 1;
		sceKernelWaitThreadEnd(work->updateThreadId, NULL, NULL);
	}

	/* Clear work */

	sceClibMemset(work, 0, sizeof(*work));

	return 0;
}
