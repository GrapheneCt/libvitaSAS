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

static const uint8_t s_subFormat[16] = {
	// 0x47E142D2, 0x36BA, 0x4D8D, 0x88FC61654F8C836C
	0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D, 0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C
};

int vitaSAS_internal_parseRiffWaveHeaderForAt9(At9Header *pHeader, const uint8_t * pBuf, uint32_t bufSize)
{
	// Error check
	if (pHeader == NULL || pBuf == NULL) {
		return -1;
	}

	uint32_t readSize = 0;
	uint32_t chunkDataSize;

	uint32_t headerSize = 0;

	// Clear context
	sceClibMemset(pHeader, 0, sizeof(At9Header));

	// Find RIFF-WAVE chunk
	for (; ; ) {
		if (bufSize < readSize + sizeof(RiffWaveHeader)) {
			return -1;
		}
		sceClibMemcpy(&pHeader->riffWaveHeader, pBuf + readSize, sizeof(RiffWaveHeader));
		readSize += sizeof(RiffWaveHeader);

		if (pHeader->riffWaveHeader.chunkId != 0x46464952) { // "RIFF"
			return -1;
		}
		chunkDataSize = pHeader->riffWaveHeader.chunkDataSize;
		if (chunkDataSize % 2 == 1) {
			// If chunkDataSize is odd, add padding data length
			chunkDataSize += 1;
		}

		if (pHeader->riffWaveHeader.typeId == 0x45564157) { // "WAVE"
			break;
		}

		if (bufSize < readSize + chunkDataSize - sizeof(pHeader->riffWaveHeader.typeId)) {
			return -1;
		}
		readSize += chunkDataSize - sizeof(pHeader->riffWaveHeader.typeId);
	}

	while (headerSize == 0) {
		ChunkHeader chunkHeader;

		if (bufSize < readSize + sizeof(ChunkHeader)) {
			return -1;
		}
		sceClibMemcpy(&chunkHeader, pBuf + readSize, sizeof(ChunkHeader));
		readSize += sizeof(ChunkHeader);

		chunkDataSize = chunkHeader.chunkDataSize;
		if (chunkDataSize % 2 == 1) {
			//If chunkDataSize is odd, add padding data length
			chunkDataSize += 1;
		}
		switch (chunkHeader.chunkId) {
		case 0x20746D66: // "fmt "
			sceClibMemcpy(&pHeader->fmtChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			if (bufSize < readSize + sizeof(FmtChunk)) {
				return -1;
			}
			sceClibMemcpy(&pHeader->fmtChunk, pBuf + readSize, sizeof(FmtChunk));
			readSize += sizeof(FmtChunk);

			if (pHeader->fmtChunk.formatTag != WAVE_FORMAT_EXTENSIBLE) {
				return -1;
			}
			if (sceClibMemcmp(s_subFormat, pHeader->fmtChunk.subFormat, sizeof(s_subFormat))) {
				return -1;
			}
			// Rest is unknown data, so just skip them
			chunkDataSize = (chunkDataSize < sizeof(FmtChunk)) ? 0 : chunkDataSize - sizeof(FmtChunk);
			break;
		case 0x74636166: // "fact"
			sceClibMemcpy(&pHeader->factChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			if (bufSize < readSize + sizeof(FactChunk)) {
				return -1;
			}
			sceClibMemcpy(&pHeader->factChunk, pBuf + readSize, sizeof(FactChunk));
			readSize += sizeof(FactChunk);

			// Rest is unknown data, so just skip them
			chunkDataSize = (chunkDataSize < sizeof(FactChunk)) ? 0 : chunkDataSize - sizeof(FactChunk);
			break;
		case 0x61746164: // "data"
			sceClibMemcpy(&pHeader->dataChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			headerSize = readSize;
			break;
		case 0x6C706D73: // "smpl"
			sceClibMemcpy(&pHeader->smplChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			if (bufSize < readSize + sizeof(SmplChunk)) {
				return -1;
			}
			sceClibMemcpy(&pHeader->smplChunk, pBuf + readSize, sizeof(SmplChunk));
			readSize += sizeof(SmplChunk);

			// Rest is unknown data, so just skip them
			chunkDataSize = (chunkDataSize < sizeof(SmplChunk)) ? 0 : chunkDataSize - sizeof(SmplChunk);
			break;
		default: // unknown chunk
			break;
		}
		if (bufSize < readSize + chunkDataSize) {
			return -1;
		}
		readSize += chunkDataSize;
	}

	return headerSize;
}

VitaSAS_Decoder* vitaSAS_create_AT9_decoder(const char* soundPath)
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

	At9Header header;

	unsigned int headerSize;
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

	pInput->buf.size = SCE_AUDIODEC_ROUND_UP(pInput->file.size + SCE_AUDIODEC_AT9_MAX_ES_SIZE);

	/* Allocate an input buffer */

	pInput->buf.p = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, pInput->buf.size);

	/* Read whole of an input file */

	vitaSAS_internal_readFile(pInput->file.pName, pInput->buf.p, pInput->file.size);
	pInput->buf.offsetW = pInput->file.size;

	headerSize = vitaSAS_internal_parseRiffWaveHeaderForAt9(&header, pInput->buf.p, pInput->buf.size);

	pAudiodecCtrl->pInfo->size = sizeof(pAudiodecCtrl->pInfo->at9);
	sceClibMemcpy(pAudiodecCtrl->pInfo->at9.configData, header.fmtChunk.configData, sizeof(pAudiodecCtrl->pInfo->at9.configData));
	pInput->buf.offsetR = headerSize;
	decoderInfo->headerSize = headerSize;

	/* Allocate codec engine memory for decoder */

	CodecEngineMemBlock* codecMemBlock = vitaSAS_internal_allocate_memory_for_codec_engine(SCE_AUDIODEC_TYPE_AT9, pAudiodecCtrl);
	decoderInfo->codecMemBlock = codecMemBlock;

	/* Create a decoder */

	sceAudiodecCreateDecoderExternal(pAudiodecCtrl, SCE_AUDIODEC_TYPE_AT9, codecMemBlock->vaContext, codecMemBlock->contextSize);

	audioOut.grain = pAudiodecCtrl->maxPcmSize / pAudiodecCtrl->pInfo->at9.ch / sizeof(int16_t);
	audioOut.samplingRate = pAudiodecCtrl->pInfo->at9.samplingRate;
	switch (pAudiodecCtrl->pInfo->at9.ch) {
	case 2:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
		audioOut.ch = pAudiodecCtrl->pInfo->at9.ch;
		break;
	case 1:
		audioOut.param = SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO;
		audioOut.ch = pAudiodecCtrl->pInfo->at9.ch;
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




