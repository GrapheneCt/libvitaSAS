#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <psp2/audioout.h> 
#include <psp2/kernel/clib.h> 
#include <psp2/kernel/threadmgr.h> 

#include "audio_out.h"

#define CHANNEL_MAX					2
#define BUFFER_MAX					2
#define GRAIN_MAX					1024

int vitaSAS_internal_update_thread(unsigned int args, void *argc)
{
	AudioOutWork *work;
	unsigned int bufferId;
	int result, aVolume[CHANNEL_MAX], portId;
	short aBuffer[BUFFER_MAX][GRAIN_MAX];

	work = *(AudioOutWork**)argc;

	/* Open audio out port */

	result = portId = sceAudioOutOpenPort(
			work->outputPort,
			work->numGrain,
			48000,
			SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (result < 0) {
		goto abort;
	}

	/* Set volume */

	aVolume[0] = aVolume[1] = SCE_AUDIO_VOLUME_0DB;

	sceAudioOutSetVolume(portId, (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), aVolume);

	/* Output loop */

	bufferId = 0;

	while (work->isAborted == 0)
	{
		/* Call rendering handler */

		(*work->renderHandler)(aBuffer[bufferId]);

		/* Output audio */

		result = sceAudioOutOutput(portId, aBuffer[bufferId]);
		if (result < 0) {
			goto abort;
		}

		/* Swap buffer */

		bufferId ^= 1;
	}

abort:

	/* Flush buffer */

	sceAudioOutOutput(portId, NULL);

	/* Release audio output port */

	result = sceAudioOutReleasePort(portId);

	return sceKernelExitDeleteThread(0);
}

int vitaSAS_internal_audio_out_start(AudioOutWork *work, unsigned int outputPort, unsigned int numGrain, 
	AudioOutRenderHandler renderHandler, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu)
{
	int result;

	/* Clear work */

	sceClibMemset(work, 0, sizeof(*work));

	/* Check input parameters */

	if (outputPort != SCE_AUDIO_OUT_PORT_TYPE_MAIN
	 && outputPort != SCE_AUDIO_OUT_PORT_TYPE_BGM) {
		return -1;
	}

	if (GRAIN_MAX < numGrain) {
		return -1;
	}

	if (renderHandler == NULL) {
		return -1;
	}

	work->outputPort = outputPort;
	work->numGrain = numGrain;
	work->renderHandler = renderHandler;

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
		goto failed;
	}

	/* Start update thread */

	result = sceKernelStartThread(work->updateThreadId, sizeof(work), &work);
	if (result < 0) {
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
