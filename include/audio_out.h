#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <psp2/audioout.h>

typedef void (*AudioOutRenderHandler)(void* buffer);

typedef struct AudioOutWork {
	SceUID updateThreadId;
	volatile uint32_t isAborted;
	uint32_t numGrain;
	uint32_t outputPort;
	AudioOutRenderHandler renderHandler;
} AudioOutWork;

int vitaSAS_internal_audio_out_start(AudioOutWork *work, unsigned int outputPort, unsigned int numGrain, 
	AudioOutRenderHandler renderHandler, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu);
int vitaSAS_internal_audio_out_stop(AudioOutWork* work);

#endif
