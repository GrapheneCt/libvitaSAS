#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/audiodec.h> 

#include <audio_dec.h>
#include <vitaSAS.h>

extern void* sceClibMspaceMalloc(void* space, unsigned int size);
extern void sceClibMspaceFree(void* space, void* ptr);
extern void* sceClibMspaceMemalign(void* space, uint32_t alignment, uint32_t size);

extern void* mspace_internal;
extern unsigned int g_portIdBGM;

int vitaSAS_internal_parseAdtsHeader(AdtsHeader *pHeader, const uint8_t * pBuf, unsigned int bufSize)
{
	const uint32_t samplingRate[] = {
		96000,
		88200,
		64000,
		48000,
		44100,
		32000,
		24000,
		22050,
		16000,
		12000,
		11025,
		8000,
		0,
		0,
		0,
		0,
	};

	/* Error check */

	if (pHeader == NULL || pBuf == NULL) {
		return -1;
	}
	if (bufSize < ADTS_HEADER_SIZE) {
		return -1;
	}

	/* Clear ADTS header */
	
	sceClibMemset(pHeader, 0, sizeof(AdtsHeader));

	/* Get ADTS header */

	pHeader->syncWord = (pBuf[0] & 0xFF) << 4 | (pBuf[1] & 0xF0) >> 4;
	pHeader->id = (pBuf[1] & 0x08) >> 3;
	pHeader->layer = (pBuf[1] & 0x06) >> 1;
	pHeader->protectionAbsent = (pBuf[1] & 0x01) >> 0;
	pHeader->profile = (pBuf[2] & 0xC0) >> 6;
	pHeader->samplingFrequencyIndex = (pBuf[2] & 0x3C) >> 2;
	pHeader->privateBit = (pBuf[2] & 0x02) >> 1;
	pHeader->channelConfiguration = (pBuf[2] & 0x01) << 2 | (pBuf[3] & 0xC0) >> 6;
	pHeader->original = (pBuf[3] & 0x20) >> 5;
	pHeader->home = (pBuf[3] & 0x10) >> 4;

	if (pHeader->syncWord != 0xFFF) {
		return -1;
	}

	pHeader->samplingRate = samplingRate[pHeader->samplingFrequencyIndex];
	pHeader->channels = pHeader->channelConfiguration;

	return 0;
}

VitaSAS_Decoder* vitaSAS_create_AAC_decoder(const char* soundPath)
{
	VitaSAS_Decoder* decoderInfo = sceClibMspaceMalloc(mspace_internal, sizeof(VitaSAS_Decoder));

	FileStream* pInput = sceClibMspaceMalloc(mspace_internal, sizeof(FileStream));
	FileStream* pOutput = sceClibMspaceMalloc(mspace_internal, sizeof(FileStream));
	SceAudiodecCtrl* pAudiodecCtrl = sceClibMspaceMalloc(mspace_internal, sizeof(SceAudiodecCtrl));
	SceAudiodecInfo* pAudiodecInfo = sceClibMspaceMalloc(mspace_internal, sizeof(SceAudiodecInfo));
	AudioOut audioOut;

	decoderInfo->pInput = pInput;
	decoderInfo->pOutput = pOutput;
	decoderInfo->pAudiodecCtrl = pAudiodecCtrl;
	decoderInfo->pAudiodecInfo = pAudiodecInfo;

	AdtsHeader header;

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

	vitaSAS_internal_getFileSize(pInput->file.pName, &pInput->file.size);

	pInput->buf.size = SCE_AUDIODEC_ROUND_UP(pInput->file.size + SCE_AUDIODEC_AAC_MAX_ES_SIZE);

	/* Allocate an input buffer */

	pInput->buf.p = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, pInput->buf.size);

	/* Read whole of an input file */

	vitaSAS_internal_readFile(pInput->file.pName, pInput->buf.p, pInput->file.size);
	pInput->buf.offsetW = pInput->file.size;

	vitaSAS_internal_parseAdtsHeader(&header, pInput->buf.p, pInput->buf.size);

	pAudiodecCtrl->pInfo->size = sizeof(pAudiodecCtrl->pInfo->aac);
	pAudiodecCtrl->pInfo->aac.isAdts = 1;
	pAudiodecCtrl->pInfo->aac.ch = header.channels;
	pAudiodecCtrl->pInfo->aac.samplingRate = header.samplingRate;
	pAudiodecCtrl->pInfo->aac.isSbr = 0;
	pInput->buf.offsetR = 0;
	decoderInfo->headerSize = 0;
	
	/* Allocate codec engine memory for decoder */

	CodecEngineMemBlock* codecMemBlock = vitaSAS_internal_allocate_memory_for_codec_engine(SCE_AUDIODEC_TYPE_AAC, pAudiodecCtrl);
	decoderInfo->codecMemBlock = codecMemBlock;

	/* Create a decoder */

	sceAudiodecCreateDecoderExternal(pAudiodecCtrl, SCE_AUDIODEC_TYPE_AAC, codecMemBlock->vaContext, codecMemBlock->contextSize);

	audioOut.grain = pAudiodecCtrl->maxPcmSize / pAudiodecCtrl->pInfo->aac.ch / sizeof(int16_t);
	audioOut.samplingRate = header.samplingRate;
	switch (pAudiodecCtrl->pInfo->aac.ch) {
	case 2:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
		audioOut.ch = pAudiodecCtrl->pInfo->aac.ch;
		break;
	case 1:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO;
		audioOut.ch = pAudiodecCtrl->pInfo->aac.ch;
		break;
	default:
		audioOut.param = 0;
		audioOut.ch = 0;
		break;
	}

	/*  Get output buffers size */

	pcmSize = sizeof(int16_t) * audioOut.ch * audioOut.grain;
	pOutput->buf.size = SCE_AUDIODEC_ROUND_UP(pcmSize);

	/* Allocate output buffers */

	pOutput->buf.op[0] = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, pOutput->buf.size);
	pOutput->buf.op[1] = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, pOutput->buf.size);

	/* Set BGM port config */

	sceAudioOutSetConfig(g_portIdBGM, audioOut.grain, 
		audioOut.samplingRate, audioOut.param);

	return decoderInfo;
}




