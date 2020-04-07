#include <stdio.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h> 
#include <psp2/io/fcntl.h> 
#include <psp2/audioout.h> 
#include <psp2/sas.h> 

#include <vitaSAS.h> 

#define CLIB_HEAP_SIZE 6 * 1024 * 1024

#define SAS_THREAD_STACK 128 * 1024
#define SAS_THREAD_AFFINITY 0
#define SAS_THREAD_PRIORITY 64
#define GRAIN 256

#define PITCH 4096
#define DRY_L_VOLUME 4096
#define DRY_R_VOLUME 4096
#define WET_L_VOLUME 0
#define WET_R_VOLUME 0
#define ADSR1 0
#define ADSR2 0

extern void* sceClibMspaceCreate(void* base, uint32_t size);
extern void* sceClibMspaceMalloc(void* space, uint32_t size);
extern void sceClibMspaceFree(void* space, void* ptr);

int main(void)
{
	void* mspace;
	void* clibm_base;
	SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
	sceKernelGetMemBlockBase(clib_heap, &clibm_base);
	mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

	vitaSAS_pass_mspace(mspace);
	vitaSAS_init(SCE_AUDIO_OUT_PORT_TYPE_MAIN, GRAIN, SAS_THREAD_PRIORITY, SAS_THREAD_STACK, SAS_THREAD_AFFINITY);
	vitaSASAudio* test = vitaSAS_load_audio_VAG("app0:resource/Image.vag");
	vitaSAS_create_voice_VAG(0, test, SCE_SAS_LOOP_ENABLE, PITCH, DRY_L_VOLUME, DRY_R_VOLUME, WET_L_VOLUME, WET_R_VOLUME, ADSR1, ADSR2);
	
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

	vitaSAS_free_audio(test);

	vitaSAS_finish();

}

