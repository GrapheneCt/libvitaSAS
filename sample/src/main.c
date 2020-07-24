#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h> 
#include <psp2/io/fcntl.h> 
#include <psp2/audioout.h> 
#include <psp2/sysmodule.h> 
#include <psp2/sas.h>
#include <psp2/libdbg.h> 

#include <vitaSAS.h> 

#define CLIB_HEAP_SIZE 16 * 1024 * 1024

#define SAS_THREAD_STACK 128 * 1024 //Increase only if there is issues with audio playback
#define SAS_THREAD_AFFINITY 0
#define SAS_THREAD_PRIORITY 64 //Must be as high as possible to avoid gaps in audio playback
#define GRAIN 2048

#define PITCH 4096 //Set pitch
#define DRY_L_VOLUME 4096 //L channel with no effects applied
#define DRY_R_VOLUME 4096 //R channel with no effects applied
#define WET_L_VOLUME 4096 //L channel with effects applied
#define WET_R_VOLUME 4096 //R channel with effects applied
#define ADSR1 0 //Set attack-decay time
#define ADSR2 0 //Set sustain-release time

int _newlib_heap_size_user = 1 * 1024 * 1024;

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

	vitaSAS_init(0);

	sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_DEBUG);

	VitaSASSystemParam initParam;
	initParam.outputPort = SCE_AUDIO_OUT_PORT_TYPE_MAIN;
	initParam.samplingRate = 48000;
	initParam.numGrain = GRAIN;
	initParam.thPriority = SAS_THREAD_PRIORITY;
	initParam.thStackSize = SAS_THREAD_STACK;
	initParam.thCpu = SAS_THREAD_AFFINITY;
	initParam.isSubSystem = SCE_FALSE;
	initParam.subSystemNum = VITASAS_NO_SUBSYSTEM;

	vitaSAS_create_system_with_config("numGrains=2048 numVoices=1 numReverbs=1", &initParam);
	vitaSASAudio* test1 = vitaSAS_load_audio_VAG("app0:resource/Image.vag", 0);

	vitaSASVoiceParam voiceParam;
	voiceParam.loop = 0;
	voiceParam.loopSize = SCE_SAS_LOOP_DISABLE;
	voiceParam.pitch = PITCH;
	voiceParam.volLDry = DRY_L_VOLUME;
	voiceParam.volRDry = DRY_R_VOLUME;
	voiceParam.volLWet = WET_L_VOLUME;
	voiceParam.volRWet = WET_R_VOLUME;
	voiceParam.adsr1 = ADSR1;
	voiceParam.adsr2 = ADSR2;

	vitaSAS_set_voice_VAG(0, test1, &voiceParam);
	vitaSAS_set_effect(SCE_SAS_FX_TYPE_HALL, WET_L_VOLUME, WET_R_VOLUME, 0, 0);
	vitaSAS_set_switch_config(0, 1);

	vitaSAS_set_key_on(0);

	while (1) {
		int endState;

		/* Check end state (endState=1 is finish) */

		endState = vitaSAS_get_end_state(0);
		if (SCE_SAS_SUCCEEDED(endState) && endState) {
			break;
		}

		/* Wait 10[msec] */

		sceKernelDelayThread(10 * 1000);
	}

	vitaSAS_destroy_system();

	vitaSAS_free_audio(test1);

	vitaSAS_finish();

}

