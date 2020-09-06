#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/audiodec.h> 
#include <psp2/libdbg.h> 

#include "audio_dec.h"
#include "vitaSAS.h"
#include "heap.h"

extern void* vitaSAS_heap_internal;
extern unsigned int g_portIdBGM;

int vitaSAS_internal_parseMpegHeader(MpegHeader *pHeader, const uint8_t * pBuf, unsigned int bufSize)
{
	const uint32_t bitRate[4][16] = {
		{0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160}, // MPEG2.5
		{0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0}, // reserved
		{0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160}, // MPEG2
		{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}, // MPEG1
	};
	const uint32_t samplingRate[4][3] = {
		{11025, 12000,  8000}, // MPEG2.5
		{    0,     0,     0}, // reserved
		{22050, 24000, 16000}, // MPEG2
		{44100, 48000, 32000}, // MPEG1
	};
	const uint32_t channels[4] = {
		2, 2, 2, 1
	};

	// Error check
	if (pHeader == NULL || pBuf == NULL) {
		return -1;
	}
	if (bufSize < MPEG_HEADER_SIZE) {
		return -1;
	}
	
	/* Clear MPEG header */

	sceClibMemset(pHeader, 0, sizeof(MpegHeader));

	/* Get MPEG header */

	pHeader->syncWord = (pBuf[0] & 0xFF) << 4 | (pBuf[1] & 0xE0) >> 4;
	pHeader->version = (pBuf[1] & 0x18) >> 3;
	pHeader->layer = (pBuf[1] & 0x06) >> 1;
	pHeader->protectionBit = (pBuf[1] & 0x01) >> 0;
	pHeader->bitRateIndex = (pBuf[2] & 0xF0) >> 4;
	pHeader->samplingRateIndex = (pBuf[2] & 0x06) >> 2;
	pHeader->paddingBit = (pBuf[2] & 0x02) >> 1;
	pHeader->privateBit = (pBuf[2] & 0x01) >> 0;
	pHeader->chMode = (pBuf[3] & 0xC0) >> 6;
	pHeader->modeExtension = (pBuf[3] & 0x30) >> 4;
	pHeader->copyrightBit = (pBuf[3] & 0x08) >> 3;
	pHeader->originalBit = (pBuf[3] & 0x04) >> 2;
	pHeader->emphasis = (pBuf[3] & 0x03) >> 0;

	if (pHeader->syncWord != 0xFFE) {
		return -1;
	}

	pHeader->bitRate = bitRate[pHeader->version][pHeader->bitRateIndex];
	pHeader->samplingRate = samplingRate[pHeader->version][pHeader->samplingRateIndex];
	pHeader->channels = channels[pHeader->chMode];

	return 0;
}

VitaSAS_Decoder* vitaSAS_create_MP3_decoder(const char* soundPath)
{
	int ret = 0;

	VitaSAS_Decoder* decoderInfo = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(VitaSAS_Decoder));

	FileStream* pInput = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(FileStream));
	FileStream* pOutput = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(FileStream));
	SceAudiodecCtrl* pAudiodecCtrl = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(SceAudiodecCtrl));
	SceAudiodecInfo* pAudiodecInfo = heap_alloc_heap_memory(vitaSAS_heap_internal, sizeof(SceAudiodecInfo));

	if (decoderInfo == NULL || pInput == NULL || pOutput == NULL || pAudiodecCtrl == NULL || pAudiodecInfo == NULL) {
		SCE_DBG_LOG_ERROR("[DEC] heap_alloc_heap_memory() returned NULL");
		goto failed;
	}

	AudioOut audioOut;

	decoderInfo->pInput = pInput;
	decoderInfo->pOutput = pOutput;
	decoderInfo->pAudiodecCtrl = pAudiodecCtrl;
	decoderInfo->pAudiodecInfo = pAudiodecInfo;

	MpegHeader header;

	unsigned int pcmSize = 0;

	sceClibMemset(pAudiodecCtrl, 0, sizeof(SceAudiodecCtrl));
	sceClibMemset(pAudiodecInfo, 0, sizeof(SceAudiodecInfo));

	pInput->file.pName = soundPath;
	pInput->file.size = 0;
	pInput->buf.p = NULL;
	pInput->buf.offsetR = 0;
	pInput->buf.offsetW = 0;

	pOutput->buf.p = NULL;
	pOutput->buf.offsetR = 0;
	pOutput->buf.offsetW = 0;
	pOutput->buf.bufIndex = 0;

	pAudiodecCtrl->pInfo = pAudiodecInfo;
	pAudiodecCtrl->size = sizeof(SceAudiodecCtrl);
	pAudiodecCtrl->wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;

	ret = vitaSAS_internal_getFileSize(pInput->file.pName, &pInput->file.size);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] vitaSAS_internal_getFileSize(): 0x%X", ret);
		goto failed;
	}

	pInput->buf.size = SCE_AUDIODEC_ROUND_UP(pInput->file.size + SCE_AUDIODEC_MP3_MAX_ES_SIZE);

	/* Allocate an input buffer */

	heap_alloc_opt_param param;
	param.size = sizeof(heap_alloc_opt_param);
	param.alignment = SCE_AUDIODEC_ALIGNMENT_SIZE;
	pInput->buf.p = heap_alloc_heap_memory_with_option(vitaSAS_heap_internal, pInput->buf.size, &param);
	if (pInput->buf.p == NULL) {
		SCE_DBG_LOG_ERROR("[DEC] sceClibMspaceMemalign() returned NULL");
		goto failed;
	}

	/* Read whole of an input file */

	ret = vitaSAS_internal_readFile(pInput->file.pName, pInput->buf.p, pInput->file.size);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] vitaSAS_internal_readFile(): 0x%X", ret);
		goto failed;
	}

	pInput->buf.offsetW = pInput->file.size;

	ret = vitaSAS_internal_parseMpegHeader(&header, pInput->buf.p, pInput->buf.size);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] vitaSAS_internal_parseMpegHeader(): 0x%X", ret);
		goto failed;
	}

	pAudiodecCtrl->pInfo->size = sizeof(pAudiodecCtrl->pInfo->mp3);
	pAudiodecCtrl->pInfo->mp3.ch = header.channels;
	pAudiodecCtrl->pInfo->mp3.version = header.version;
	pInput->buf.offsetR = 0;
	decoderInfo->headerSize = 0;
	
	/* Initialize audiodec library */

	SceAudiodecInitParam audiodecInitParam;

	sceClibMemset(&audiodecInitParam, 0, sizeof(audiodecInitParam));

	audiodecInitParam.size = sizeof(audiodecInitParam.mp3);
	audiodecInitParam.mp3.totalStreams = 1;

	ret = sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_MP3, &audiodecInitParam);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] sceAudiodecInitLibrary(): 0x%X", ret);
		goto failed;
	}

	/* Create a decoder */

	ret = sceAudiodecCreateDecoder(pAudiodecCtrl, SCE_AUDIODEC_TYPE_MP3);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] sceAudiodecCreateDecoder(): 0x%X", ret);
		goto failed;
	}

	audioOut.grain = pAudiodecCtrl->maxPcmSize / pAudiodecCtrl->pInfo->mp3.ch / sizeof(int16_t);
	audioOut.samplingRate = header.samplingRate;
	switch (pAudiodecCtrl->pInfo->mp3.ch) {
	case 2:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
		audioOut.ch = pAudiodecCtrl->pInfo->mp3.ch;
		break;
	case 1:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO;
		audioOut.ch = pAudiodecCtrl->pInfo->mp3.ch;
		break;
	default:
		audioOut.param = 0;
		audioOut.ch = 0;
		SCE_DBG_LOG_WARNING("[DEC] Invalid channel information");
		break;
	}

	/*  Get output buffers size */

	pcmSize = sizeof(int16_t) * audioOut.ch * audioOut.grain;
	pOutput->buf.size = SCE_AUDIODEC_ROUND_UP(pcmSize);

	/* Allocate output buffers */

	pOutput->buf.op[0] = heap_alloc_heap_memory_with_option(vitaSAS_heap_internal, pOutput->buf.size, &param);
	if (pOutput->buf.op[0] == NULL) {
		SCE_DBG_LOG_ERROR("[DEC] heap_alloc_heap_memory_with_option() returned NULL");
		goto failed;
	}

	pOutput->buf.op[1] = heap_alloc_heap_memory_with_option(vitaSAS_heap_internal, pOutput->buf.size, &param);
	if (pOutput->buf.op[1] == NULL) {
		SCE_DBG_LOG_ERROR("[DEC] heap_alloc_heap_memory_with_option() returned NULL");
		goto failed;
	}

	/* Set BGM port config */

	ret = sceAudioOutSetConfig(g_portIdBGM, audioOut.grain, 
		audioOut.samplingRate, audioOut.param);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[DEC] sceAudioOutSetConfig(): 0x%X", ret);
		goto failed;
	}

	return decoderInfo;

failed:

	if (decoderInfo != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, decoderInfo);
	if (pInput != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pInput);
	if (pOutput != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pOutput);
	if (pAudiodecCtrl != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pAudiodecCtrl);
	if (pAudiodecInfo != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pAudiodecInfo);
	if (pInput->buf.p != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pInput->buf.p);
	if (pOutput->buf.op[0] != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pOutput->buf.op[0]);
	if (pOutput->buf.op[1] != NULL)
		heap_free_heap_memory(vitaSAS_heap_internal, pOutput->buf.op[1]);

	return NULL;
}




