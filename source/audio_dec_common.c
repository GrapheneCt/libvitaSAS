#include <psp2/codecengine.h> 
#include <psp2/kernel/clib.h> 
#include <psp2/io/fcntl.h> 
#include <psp2/kernel/sysmem.h> 
#include <psp2/kernel/threadmgr.h> 

#include "audio_out.h"
#include "vitaSAS.h"

extern void sceClibMspaceFree(void* space, void* ptr);
extern void* sceClibMspaceMalloc(void* space, unsigned int size);

extern void* mspace_internal;
extern unsigned int g_portIdBGM;

int vitaSAS_internal_getFileSize(const char *pInputFileName, uint32_t *pInputFileSize)
{
	int res = 0;
	int resTmp;

	SceUID uidFd = -1;

	/* Set initial value */

	*pInputFileSize = 0;

	/* Open an input file */

	res = sceIoOpen(pInputFileName, SCE_O_RDONLY, 0);
	uidFd = res;
	res = 0;

	/* Get the file size */
	
	res = sceIoLseek(uidFd, 0, SCE_SEEK_END);
	*pInputFileSize = res;
	res = 0;

	if (0 <= uidFd) {

		/* Close an input file */

		resTmp = sceIoClose(uidFd);
		if (resTmp < 0) {
			res = resTmp;
		}
	}

	return res;
}

int vitaSAS_internal_readFile(const char *pInputFileName, void *pInputBuf, uint32_t inputFileSize)
{
	int res = 0;
	int resTmp;

	SceUID uidFd = -1;

	/* Open an input file */

	res = sceIoOpen(pInputFileName, SCE_O_RDONLY, 0);
	uidFd = res;
	res = 0;

	/* Read an input file */

	do {
		res = sceIoRead(uidFd, pInputBuf, inputFileSize);
		if (res < 0) {
			res = -1;
		}
		pInputBuf = (uint8_t *)pInputBuf + res;
		inputFileSize -= res;
	} while (0 < inputFileSize);
	res = 0;

	if (0 <= uidFd) {

		/* Close an input file */

		resTmp = sceIoClose(uidFd);
		if (resTmp < 0) {
			res = resTmp;
		}
	}

	return res;
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
	if (decoderInfo->pInput->file.size <= decoderInfo->pInput->buf.offsetR)
		return 1;
	else
		return 0;
}

void vitaSAS_decode_to_buffer(VitaSAS_Decoder* decoderInfo, unsigned int begEsSamples, unsigned int nEsSamples, uint8_t* buffer)
{
	decoderInfo->pInput->buf.offsetR = decoderInfo->headerSize + decoderInfo->pAudiodecCtrl->inputEsSize * begEsSamples;

	unsigned int offsetW = 0;

	while (1) {

		/* Check if there is any undecoded elementary stream */

		if (decoderInfo->pInput->file.size <= decoderInfo->pInput->buf.offsetR ||
			decoderInfo->pInput->buf.offsetR >= decoderInfo->headerSize + decoderInfo->pAudiodecCtrl->inputEsSize * nEsSamples) {
			break;
		}

		/* Set elementary stream and PCM buffer */

		decoderInfo->pAudiodecCtrl->pEs = decoderInfo->pInput->buf.p + decoderInfo->pInput->buf.offsetR;
		decoderInfo->pAudiodecCtrl->pPcm = buffer + offsetW;

		/* Decode audio data */

		sceAudiodecDecode(decoderInfo->pAudiodecCtrl);

		/* Update offset */

		decoderInfo->pInput->buf.offsetR += decoderInfo->pAudiodecCtrl->inputEsSize;
		offsetW += offsetW + decoderInfo->pOutput->buf.size;

	}

}

void vitaSAS_internal_free_memory_for_codec_engine(const CodecEngineMemBlock* codecMemBlock)
{
	sceCodecEngineFreeMemoryFromUnmapMemBlock(codecMemBlock->uidUnmap, codecMemBlock->vaContext);
	sceCodecEngineCloseUnmapMemBlock(codecMemBlock->uidUnmap);
	sceKernelFreeMemBlock(codecMemBlock->uidMemBlock);
}

CodecEngineMemBlock* vitaSAS_internal_allocate_memory_for_codec_engine(unsigned int codecType, SceAudiodecCtrl* addecctrl)
{
	CodecEngineMemBlock* codecMemBlock = sceClibMspaceMalloc(mspace_internal, sizeof(CodecEngineMemBlock));

	SceUID uidMemBlock, uidUnmap;
	uidMemBlock = 0;
	uidUnmap = 0;
	unsigned int memBlockSize;
	void *pMemBlock = NULL;
	unsigned int vaContext = 0;
	unsigned int contextSize = 0;
	int res;

	/* Obtain required memory size */

	contextSize = sceAudiodecGetContextSize(addecctrl, codecType);
	if (contextSize <= 0) {
		goto error;
	}

	memBlockSize = ROUND_UP(contextSize, 1024 * 1024);

	/* Allocate a cache-disabled and physical continuous memory that is enabled for
	reading and writing by the user */

	uidMemBlock = sceKernelAllocMemBlock("vitaSAS_codec_engine",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, memBlockSize, NULL);
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
	sceClibMspaceFree(mspace_internal, decoderInfo->pOutput->buf.op[0]);
	sceClibMspaceFree(mspace_internal, decoderInfo->pOutput->buf.op[1]);
	if (decoderInfo->pAudiodecCtrl->pInfo->size != sizeof(decoderInfo->pAudiodecCtrl->pInfo->mp3)) {
		sceAudiodecDeleteDecoderExternal(decoderInfo->pAudiodecCtrl, &decoderInfo->codecMemBlock->vaContext);
		vitaSAS_internal_free_memory_for_codec_engine(decoderInfo->codecMemBlock);
	}
	else {
		sceAudiodecDeleteDecoder(decoderInfo->pAudiodecCtrl);
		sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_MP3);
	}
	sceClibMspaceFree(mspace_internal, decoderInfo->codecMemBlock);
	sceClibMspaceFree(mspace_internal, decoderInfo->pInput->buf.p);
	sceClibMspaceFree(mspace_internal, decoderInfo->pAudiodecInfo);
	sceClibMspaceFree(mspace_internal, decoderInfo->pAudiodecCtrl);
	sceClibMspaceFree(mspace_internal, decoderInfo->pOutput);
	sceClibMspaceFree(mspace_internal, decoderInfo->pInput);
	sceClibMspaceFree(mspace_internal, decoderInfo);
}