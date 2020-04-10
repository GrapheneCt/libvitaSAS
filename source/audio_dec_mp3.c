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

int vitaSAS_internal_parseMpegHeader(MpegHeader *pHeader, const uint8_t * pBuf, unsigned int bufSize)
{
#ifdef DISPLAY_HEADER
	const char *version[4] = {
		"2.5",
		"X",
		"2",
		"1",
	};
	const uint32_t layer[4] = {
		0, 3, 2, 1
	};
#endif /* DISPLAY_HEADER */
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

#ifdef DISPLAY_HEADER
	sceClibPrintf("= MPEG Audio header ===============\n");
	sceClibPrintf("syncWord                 : 0x%X (0xFFE fixed)\n", pHeader->syncWord);
	sceClibPrintf("version                  : 0x%X (MPEG %s)\n", pHeader->version, version[pHeader->version]);
	sceClibPrintf("layer                    : 0x%X (Layer %d)\n", pHeader->layer, layer[pHeader->layer]);
	sceClibPrintf("protectionBit            : 0x%X\n", pHeader->protectionBit);
	sceClibPrintf("bitRateIndex             : 0x%X (%u kbps)\n", pHeader->bitRateIndex, pHeader->bitRate);
	sceClibPrintf("samplingRateIndex        : 0x%X (%u Hz)\n", pHeader->samplingRateIndex, pHeader->samplingRate);
	sceClibPrintf("paddingBit               : 0x%X\n", pHeader->paddingBit);
	sceClibPrintf("privateBit               : 0x%X\n", pHeader->privateBit);
	sceClibPrintf("chMode                   : 0x%X (%u channels)\n", pHeader->chMode, pHeader->channels);
	sceClibPrintf("modeExtension            : 0x%X\n", pHeader->modeExtension);
	sceClibPrintf("copyrightBit             : 0x%X\n", pHeader->copyrightBit);
	sceClibPrintf("originalBit              : 0x%X\n", pHeader->originalBit);
	sceClibPrintf("emphasis                 : 0x%X\n", pHeader->emphasis);
	sceClibPrintf("===================================\n");
#endif /* DISPLAY_HEADER */

	return 0;
}

VitaSAS_Decoder* vitaSAS_create_MP3_decoder(const char* soundPath)
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

	vitaSAS_internal_getFileSize(pInput->file.pName, &pInput->file.size);

	pInput->buf.size = SCE_AUDIODEC_ROUND_UP(pInput->file.size + SCE_AUDIODEC_MP3_MAX_ES_SIZE);

	/* Allocate an input buffer */

	pInput->buf.p = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, pInput->buf.size);

	/* Read whole of an input file */

	vitaSAS_internal_readFile(pInput->file.pName, pInput->buf.p, pInput->file.size);
	pInput->buf.offsetW = pInput->file.size;

	vitaSAS_internal_parseMpegHeader(&header, pInput->buf.p, pInput->buf.size);
	pAudiodecCtrl->pInfo->size = sizeof(pAudiodecCtrl->pInfo->mp3);
	pAudiodecCtrl->pInfo->mp3.ch = header.channels;
	pAudiodecCtrl->pInfo->mp3.version = header.version;
	pInput->buf.offsetR = 0;
	decoderInfo->headerSize = 0;
	
	/* Allocate codec engine memory for decoder */

	CodecEngineMemBlock* codecMemBlock = vitaSAS_internal_allocate_memory_for_codec_engine(SCE_AUDIODEC_TYPE_MP3, pAudiodecCtrl);
	decoderInfo->codecMemBlock = codecMemBlock;

	/* Create a decoder */

	sceAudiodecCreateDecoderExternal(pAudiodecCtrl, SCE_AUDIODEC_TYPE_MP3, codecMemBlock->vaContext, codecMemBlock->contextSize);

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




