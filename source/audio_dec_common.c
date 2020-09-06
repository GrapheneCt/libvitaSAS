#include <arm_neon.h>
#include <psp2/codecengine.h> 
#include <psp2/kernel/clib.h> 
#include <psp2/kernel/iofilemgr.h> 
#include <psp2/libdbg.h> 
#include <psp2/kernel/sysmem.h> 
#include <psp2/kernel/threadmgr.h> 

#include "vitaSAS.h"
#include "heap.h"

extern void* vitaSAS_heap_internal;
extern unsigned int g_portIdBGM;

void vitaSAS_separate_channels_PCM(short* pBufL, short* pBufR, short* pBufSrc, unsigned int bufSrcSize)
{
	unsigned int num16x8 = bufSrcSize / 8;
	int16x8x2_t separated;
	for (int i = 0; i < num16x8; i++) {
		separated = vld2q_s16(pBufSrc + 2 * 8 * i);
		vst1q_s16(pBufL + 8 * i, separated.val[0]);
		vst1q_s16(pBufR + 8 * i, separated.val[1]);
	}
}

int vitaSAS_internal_getFileSize(const char *pInputFileName, uint32_t *pInputFileSize)
{
	int ret = 0;

	SceIoStat stat;
	ret = sceIoGetstat(pInputFileName, &stat);

	*pInputFileSize = (uint32_t)stat.st_size;

	return ret;
}

int vitaSAS_internal_readFile(const char *pInputFileName, void *pInputBuf, uint32_t inputFileSize)
{
	int ret = 0;

	SceUID uidFd = -1;

	/* Open an input file */

	uidFd = sceIoOpen(pInputFileName, SCE_O_RDONLY, 0);
	if (uidFd < 0)
		return uidFd;

	/* Read an input file */

	ret = sceIoRead(uidFd, pInputBuf, inputFileSize);
	if (ret < 0)
		return ret;

	ret = sceIoClose(uidFd);

	return ret;
}

void vitaSAS_internal_decode_to_buffer(SceAudiodecCtrl *pCtrl, Buffer *pInput, Buffer *pOutput)
{
	/* Set elementary stream and PCM buffer */

	pCtrl->pEs = pInput->p + pInput->offsetR;
	pCtrl->pPcm = pOutput->p + pOutput->offsetW;

	/* Decode audio data */

	sceAudiodecDecode(pCtrl);

	/* Update offset */
	pInput->offsetR += pCtrl->inputEsSize;
	pOutput->offsetW = pOutput->offsetW + pOutput->size;
}

void vitaSAS_internal_decode(SceAudiodecCtrl *pCtrl, Buffer *pInput, Buffer *pOutput)
{
	/* Set elementary stream and PCM buffer */

	pCtrl->pEs = pInput->p + pInput->offsetR;
	pCtrl->pPcm = pOutput->op[pOutput->bufIndex];

	/* Decode audio data */

	sceAudiodecDecode(pCtrl);

	/* Update offset */

	pInput->offsetR += pCtrl->inputEsSize;
}

int vitaSAS_internal_decoder_thread(unsigned int args, void *argc)
{
	VitaSAS_Decoder* decoderInfo;
	decoderInfo = *(VitaSAS_Decoder**)argc;
	while (1) {

		/* Check if there is any undecoded elementary stream */

		if (decoderInfo->pInput->file.size <= decoderInfo->pInput->buf.offsetR) {
			break;
		}
		if (decoderInfo->decodeStatus) {

			/* Decode audio data */

			vitaSAS_internal_decode(decoderInfo->pAudiodecCtrl, &decoderInfo->pInput->buf, &decoderInfo->pOutput->buf);

			/* Output audio data */

			vitaSAS_internal_output_for_decoder(&decoderInfo->pOutput->buf);
		}
		else
			sceAudioOutOutput(g_portIdBGM, NULL);
	}
	return sceKernelExitDeleteThread(0);
}

void vitaSAS_decoder_start_playback(VitaSAS_Decoder* decoderInfo, unsigned int thPriority, unsigned int thStackSize, unsigned int thCpu)
{
	SceUID decodeThread;

	/* Reinitialize context */

	sceAudiodecClearContext(decoderInfo->pAudiodecCtrl);

	/* Reset es offset */

	decoderInfo->pInput->buf.offsetR = decoderInfo->headerSize;

	/* Create decoder thread */

	decodeThread = sceKernelCreateThread(
		"vitaSAS_decoder_thread",
		vitaSAS_internal_decoder_thread,
		thPriority,
		thStackSize,
		0,
		thCpu,
		NULL);

	/* Start decoder thread */

	decoderInfo->decodeStatus = 1;
	sceKernelStartThread(decodeThread, sizeof(decoderInfo), &decoderInfo);
}

void vitaSAS_decoder_pause_playback(VitaSAS_Decoder* decoderInfo)
{
	decoderInfo->decodeStatus = 0;
}

void vitaSAS_decoder_resume_playback(VitaSAS_Decoder* decoderInfo)
{
	decoderInfo->decodeStatus = 1;
}

void vitaSAS_decoder_stop_playback(VitaSAS_Decoder* decoderInfo)
{
	vitaSAS_decoder_pause_playback(decoderInfo);
	decoderInfo->pInput->buf.offsetR = decoderInfo->pInput->file.size + 1;
}

void vitaSAS_decoder_seek(VitaSAS_Decoder* decoderInfo, unsigned int nEsSamples)
{
	decoderInfo->pInput->buf.offsetR = decoderInfo->headerSize + decoderInfo->pAudiodecCtrl->inputEsSize * nEsSamples;
}

unsigned int vitaSAS_decoder_get_current_es_offset(VitaSAS_Decoder* decoderInfo)
{
	return decoderInfo->pInput->buf.offsetR;
}

unsigned int vitaSAS_decoder_get_end_state(VitaSAS_Decoder* decoderInfo)
{
	if (decoderInfo->pInput->file.size <= decoderInfo->pInput->buf.offsetR || 
		decoderInfo->pInput->buf.offsetR == decoderInfo->headerSize)
		return 1;
	else
		return 0;
}

void vitaSAS_decode_to_buffer(VitaSAS_Decoder* decoderInfo, unsigned int begEsSamples, unsigned int nEsSamples, uint8_t* buffer)
{
	decoderInfo->pOutput->buf.offsetW = 0;
	decoderInfo->pOutput->buf.p = buffer;
	decoderInfo->pInput->buf.offsetR = decoderInfo->headerSize + decoderInfo->pAudiodecCtrl->inputEsSize * begEsSamples;

	while (1) {

		/* Check if there is any undecoded elementary stream */

		if (decoderInfo->pInput->file.size <= decoderInfo->pInput->buf.offsetR ||
			decoderInfo->pInput->buf.offsetR > decoderInfo->headerSize + decoderInfo->pAudiodecCtrl->inputEsSize * nEsSamples) {
			break;
		}

		vitaSAS_internal_decode_to_buffer(decoderInfo->pAudiodecCtrl, &decoderInfo->pInput->buf, &decoderInfo->pOutput->buf);
	}

}

void vitaSAS_internal_free_memory_for_codec_engine(const CodecEngineMemBlock* codecMemBlock)
{
	sceCodecEngineFreeMemoryFromUnmapMemBlock(codecMemBlock->uidUnmap, codecMemBlock->vaContext);
	sceCodecEngineCloseUnmapMemBlock(codecMemBlock->uidUnmap);
	sceKernelFreeMemBlock(codecMemBlock->uidMemBlock);
}

CodecEngineMemBlock* vitaSAS_internal_allocate_memory_for_codec_engine(unsigned int codecType, SceAudiodecCtrl* addecctrl, unsigned int useMainMem)
{
	CodecEngineMemBlock* codecMemBlock = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(CodecEngineMemBlock));

	SceUID uidMemBlock, uidUnmap;
	uidMemBlock = 0;
	uidUnmap = 0;
	unsigned int memBlockSize;
	void *pMemBlock = NULL;
	unsigned int vaContext = 0;
	unsigned int contextSize = 0;
	unsigned int memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW;
	int res;

	/* Obtain required memory size */

	contextSize = sceAudiodecGetContextSize(addecctrl, codecType);
	if (contextSize <= 0) {
		goto error;
	}

	memBlockSize = ROUND_UP(contextSize, 1024 * 1024);

	/* Allocate a cache-disabled and physical continuous memory that is enabled for
	reading and writing by the user */

	if (useMainMem)
		memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE;

	uidMemBlock = sceKernelAllocMemBlock("vitaSAS_codec_engine",
		memBlockType, memBlockSize, NULL);
	if (uidMemBlock < 0) {
		goto error;
	}

	/* Obtain starting address of allocated memory */

	res = sceKernelGetMemBlockBase(uidMemBlock, &pMemBlock);
	if (res < 0) {
		goto error;
	}

	/* Remap as a cache-disabled and physical continuous memory that is enabled for
	reading and writing by the Codec Engine but not by the user */

	uidUnmap = sceCodecEngineOpenUnmapMemBlock(pMemBlock, memBlockSize);
	if (uidUnmap < 0) {
		goto error;
	}

	/* Allocate memory from remapped area */

	vaContext = sceCodecEngineAllocMemoryFromUnmapMemBlock(uidUnmap, contextSize,
		SCE_AUDIODEC_ALIGNMENT_SIZE);
	if (vaContext == 0) {
		goto error;
	}

	codecMemBlock->uidMemBlock = uidMemBlock;
	codecMemBlock->uidUnmap = uidUnmap;
	codecMemBlock->vaContext = vaContext;
	codecMemBlock->contextSize = contextSize;

	return codecMemBlock;

error:

	sceCodecEngineFreeMemoryFromUnmapMemBlock(uidUnmap, vaContext);
	sceCodecEngineCloseUnmapMemBlock(uidUnmap);
	sceKernelFreeMemBlock(uidMemBlock);

	return NULL;
}

void vitaSAS_destroy_decoder(VitaSAS_Decoder* decoderInfo)
{
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pOutput->buf.op[0]);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pOutput->buf.op[1]);
	if (decoderInfo->pAudiodecCtrl->pInfo->size != sizeof(decoderInfo->pAudiodecCtrl->pInfo->mp3)) {
		sceAudiodecDeleteDecoderExternal(decoderInfo->pAudiodecCtrl, &decoderInfo->codecMemBlock->vaContext);
		vitaSAS_internal_free_memory_for_codec_engine(decoderInfo->codecMemBlock);
	}
	else {
		sceAudiodecDeleteDecoder(decoderInfo->pAudiodecCtrl);
		sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_MP3);
	}
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->codecMemBlock);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pInput->buf.p);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pAudiodecInfo);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pAudiodecCtrl);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pOutput);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo->pInput);
	heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo);
}