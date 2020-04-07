#include <stdio.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h> 
#include <psp2/io/fcntl.h> 
#include <psp2/audioout.h> 
#include <psp2/sas.h> 

#include <vitaSAS.h> 

#define CLIB_HEAP_SIZE 6 * 1024 * 1024

#define SAS_THREAD_STACK 128 * 1024 //Increase only if there is issues with audio playback
#define SAS_THREAD_AFFINITY 0
#define SAS_THREAD_PRIORITY 64 //Must be as high as possible to avoid gaps in audio playback
#define GRAIN 256

#define PITCH 4096 //Set pitch
#define DRY_L_VOLUME 4096 //L channel with no effects applied
#define DRY_R_VOLUME 4096 //R channel with no effects applied
#define WET_L_VOLUME 4096 //L channel with effects applied
#define WET_R_VOLUME 4096 //R channel with effects applied
#define ADSR1 0 //Set attack-decay time
#define ADSR2 0 //Set sustain-release time

extern void* sceClibMspaceCreate(void* base, uint32_t size);
extern void* sceClibMspaceMalloc(void* space, uint32_t size);
extern void sceClibMspaceFree(void* space, void* ptr);

int main(void)
{
	/* Create C heap for vitaSAS (required for system mode application support) */

	void* mspace;
	void* clibm_base;
	SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
	sceKernelGetMemBlockBase(clib_heap, &clibm_base);
	mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);
	vitaSAS_pass_mspace(mspace);

	/* Audio section */

	vitaSAS_init(SCE_AUDIO_OUT_PORT_TYPE_MAIN, GRAIN, SAS_THREAD_PRIORITY, SAS_THREAD_STACK, SAS_THREAD_AFFINITY);
	vitaSASAudio* test1 = vitaSAS_load_audio_VAG("app0:resource/Image.vag");
	vitaSAS_create_voice_VAG(0, test1, SCE_SAS_LOOP_ENABLE, PITCH, DRY_L_VOLUME, DRY_R_VOLUME, WET_L_VOLUME, WET_R_VOLUME, ADSR1, ADSR2);
	vitaSAS_set_effect(SCE_SAS_FX_TYPE_HALL, WET_L_VOLUME, WET_R_VOLUME, 0, 0);
	
	sceSasSetKeyOn(0);

	while (1) {
		int endState;

		/* Check end state (endState=1 is finish) */

		endState = sceSasGetEndState(0);
		if (SCE_SAS_SUCCEEDED(endState) && endState) {
			break;
		}

		/* Wait 10[msec] */

		sceKernelDelayThread(10 * 1000);
	}

	vitaSAS_free_audio(test1);

	vitaSAS_finish();

}

