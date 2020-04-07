#include <stdio.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h> 
#include <psp2/audiodec.h> 
#include <psp2/audioout.h> 
#include <vita2d_sys.h>

#include <audio_dec_at9.h>

extern void* sceClibMspaceCreate(void* base, uint32_t size);
extern void* sceClibMspaceMemalign(void* space, uint32_t alignment, uint32_t size);

extern SceUID sceCodecEngineOpenUnmapMemBlock(void *pMemBlock, SceUInt32 memBlockSize);
extern SceInt32 sceCodecEngineCloseUnmapMemBlock(SceUID uid);
extern SceUIntVAddr sceCodecEngineAllocMemoryFromUnmapMemBlock(SceUID uid, SceUInt32 size, SceUInt32 alignment);
extern SceInt32 sceCodecEngineFreeMemoryFromUnmapMemBlock(SceUID uid, SceUIntVAddr p);

static const uint8_t s_subFormat[16] = {
	// 0x47E142D2, 0x36BA, 0x4D8D, 0x88FC61654F8C836C
	0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D, 0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C
};

static AudioOut s_audioOut;

int parseRiffWaveHeaderForAt9(At9Header *pHeader, const uint8_t * pBuf, uint32_t bufSize)
{
	// Error check
	if (pHeader == NULL || pBuf == NULL) {
		sceClibPrintf("error: invalid pointer\n");
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
			sceClibPrintf("error: buffer size is too small\n");
			return -1;
		}
		sceClibMemcpy(&pHeader->riffWaveHeader, pBuf + readSize, sizeof(RiffWaveHeader));
		readSize += sizeof(RiffWaveHeader);

		if (pHeader->riffWaveHeader.chunkId != 0x46464952) { // "RIFF"
			sceClibPrintf("error: beginning of header is not \"RIFF\"\n");
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
			sceClibPrintf("error: buffer size is too small\n");
			return -1;
		}
		readSize += chunkDataSize - sizeof(pHeader->riffWaveHeader.typeId);
	}

	while (headerSize == 0) {
		ChunkHeader chunkHeader;

		if (bufSize < readSize + sizeof(ChunkHeader)) {
			sceClibPrintf("error: buffer size is too small\n");
			return -1;
		}
		sceClibMemcpy(&chunkHeader, pBuf + readSize, sizeof(ChunkHeader));
		readSize += sizeof(ChunkHeader);

		chunkDataSize = chunkHeader.chunkDataSize;
		if (chunkDataSize % 2 == 1) {
			// If chunkDataSize is odd, add padding data length
			chunkDataSize += 1;
		}
		switch (chunkHeader.chunkId) {
		case 0x20746D66: // "fmt "
			sceClibMemcpy(&pHeader->fmtChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			if (bufSize < readSize + sizeof(FmtChunk)) {
				sceClibPrintf("error: buffer size is too small\n");
				return -1;
			}
			sceClibMemcpy(&pHeader->fmtChunk, pBuf + readSize, sizeof(FmtChunk));
			readSize += sizeof(FmtChunk);

			if (pHeader->fmtChunk.formatTag != WAVE_FORMAT_EXTENSIBLE) {
				sceClibPrintf("unknown ID: %X\n", pHeader->fmtChunk.formatTag);
				return -1;
			}
			if (sceClibMemcmp(s_subFormat, pHeader->fmtChunk.subFormat, sizeof(s_subFormat))) {
				const uint32_t *p = (uint32_t *)pHeader->fmtChunk.subFormat;
				sceClibPrintf("unknown codec ID: %08X%08X%08X%08X\n", p[0], p[1], p[3], p[4]);
				return -1;
			}
			// Rest is unknown data, so just skip them
			chunkDataSize = (chunkDataSize < sizeof(FmtChunk)) ? 0 : chunkDataSize - sizeof(FmtChunk);
			break;
		case 0x74636166: // "fact"
			sceClibMemcpy(&pHeader->factChunkHeader, &chunkHeader, sizeof(ChunkHeader));

			if (bufSize < readSize + sizeof(FactChunk)) {
				sceClibPrintf("error: buffer size is too small\n");
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
				sceClibPrintf("error: buffer size is too small\n");
				return -1;
			}
			sceClibMemcpy(&pHeader->smplChunk, pBuf + readSize, sizeof(SmplChunk));
			readSize += sizeof(SmplChunk);

			// Rest is unknown data, so just skip them
			chunkDataSize = (chunkDataSize < sizeof(SmplChunk)) ? 0 : chunkDataSize - sizeof(SmplChunk);
			break;
		default: // unknown chunk
			sceClibPrintf("unknown\n");
			break;
		}
		if (bufSize < readSize + chunkDataSize) {
			sceClibPrintf("error: buffer size is too small\n");
			return -1;
		}
		readSize += chunkDataSize;
	}

#ifdef DISPLAY_HEADER
	sceClibPrintf("= ATRAC9 Audio header ===============\n");
	sceClibPrintf("RiffWaveHeader.chunkId                    : 0x%X (0x45564157 (\"RIFF\") fixed)\n", pHeader->riffWaveHeader.chunkId);
	sceClibPrintf("RiffWaveHeader.chunkDataSize              : %u\n", pHeader->riffWaveHeader.chunkDataSize);
	sceClibPrintf("RiffWaveHeader.typeId                     : 0x%X (0x45564157 (\"WAVE\") fixed)\n", pHeader->riffWaveHeader.typeId);
	sceClibPrintf("ChunkHeader.chunkId                       : 0x%X (0x20746D66 (\"fmt \") fixed)\n", pHeader->fmtChunkHeader.chunkId);
	sceClibPrintf("ChunkHeader.chunkDataSize                 : %u\n", pHeader->fmtChunkHeader.chunkDataSize);
	sceClibPrintf("FmtChunk.formatTag                        : 0x%X (0xFFFE fixed)\n", pHeader->fmtChunk.formatTag);
	sceClibPrintf("FmtChunk.channels                         : %u\n", pHeader->fmtChunk.channels);
	sceClibPrintf("FmtChunk.samplesPerSec                    : %u Hz\n", pHeader->fmtChunk.samplesPerSec);
	sceClibPrintf("FmtChunk.avgBytesPerSec                   : %u kbps\n", pHeader->fmtChunk.avgBytesPerSec * 8 / 1000);
	sceClibPrintf("FmtChunk.blockAlign                       : %u\n", pHeader->fmtChunk.blockAlign);
	sceClibPrintf("FmtChunk.bitsPerSample                    : %u\n", pHeader->fmtChunk.bitsPerSample);
	sceClibPrintf("FmtChunk.cbSize                           : %u\n", pHeader->fmtChunk.cbSize);
	sceClibPrintf("FmtChunk.samplesPerBlock                  : %u\n", pHeader->fmtChunk.samplesPerBlock);
	sceClibPrintf("FmtChunk.chennelMask                      : 0x%X\n", pHeader->fmtChunk.chennelMask);
	sceClibPrintf("FmtChunk.subFormat                        : 0x%X* (47E142D2-36BA-4D8D-88FC61654F8C836C fixed)\n", pHeader->fmtChunk.subFormat[0]);
	sceClibPrintf("FmtChunk.versionInfo                      : 0x%X\n", pHeader->fmtChunk.versionInfo);
	sceClibPrintf("FmtChunk.configData                       : 0x%X*\n", pHeader->fmtChunk.configData[0]);
	sceClibPrintf("FmtChunk.reserved                         : 0x%X\n", pHeader->fmtChunk.reserved);
	sceClibPrintf("ChunkHeader.chunkId                       : 0x%X (0x74636166 (\"fact\") fixed)\n", pHeader->factChunkHeader.chunkId);
	sceClibPrintf("ChunkHeader.chunkDataSize                 : %u\n", pHeader->factChunkHeader.chunkDataSize);
	sceClibPrintf("FactChunk.totalSamples                    : %u\n", pHeader->factChunk.totalSamples);
	sceClibPrintf("FactChunk.delaySamplesInputOverlap        : %u\n", pHeader->factChunk.delaySamplesInputOverlap);
	sceClibPrintf("FactChunk.delaySamplesInputOverlapEncoder : %u\n", pHeader->factChunk.delaySamplesInputOverlapEncoder);
	sceClibPrintf("ChunkHeader.chunkId                       : 0x%X (0x6C706D73 (\"smpl\") fixed)\n", pHeader->smplChunkHeader.chunkId);
	sceClibPrintf("ChunkHeader.chunkDataSize                 : %u\n", pHeader->smplChunkHeader.chunkDataSize);
	sceClibPrintf("SmplChunk.sampleLoops                     : %u\n", pHeader->smplChunk.sampleLoops);
	sceClibPrintf("SmplChunk.SampleLoop.start                : %u\n", pHeader->smplChunk.sampleLoop.start);
	sceClibPrintf("SmplChunk.SampleLoop.end                  : %u\n", pHeader->smplChunk.sampleLoop.end);
	sceClibPrintf("ChunkHeader.chunkId                       : 0x%X (0x61746164 (\"data\") fixed)\n", pHeader->dataChunkHeader.chunkId);
	sceClibPrintf("ChunkHeader.chunkDataSize                 : %u\n", pHeader->dataChunkHeader.chunkDataSize);
	sceClibPrintf("=====================================\n");
#endif /* DISPLAY_HEADER */

	return headerSize;
}

int getFileSize(const char *pInputFileName, uint32_t *pInputFileSize)
{
	int res = 0;
	int resTmp;

	SceUID uidFd = -1;

	// Set initial value
	*pInputFileSize = 0;

	// Open an input file
	res = sceIoOpen(pInputFileName, SCE_O_RDONLY, 0);
	if (res < 0) {
		sceClibPrintf("error: sceIoOpen() failed: 0x%08X\n", res);
	}
	uidFd = res;
	res = 0;
	// Get the file size
	res = sceIoLseek(uidFd, 0, SCE_SEEK_END);
	if (res < 0) {
		sceClibPrintf("error: sceIoLseek() failed: 0x%08X\n", res);
	}
	*pInputFileSize = res;
	res = 0;

	if (0 <= uidFd) {
		// Close an input file
		resTmp = sceIoClose(uidFd);
		if (resTmp < 0) {
			sceClibPrintf("error: sceIoClose() failed: 0x%08X\n", resTmp);
			res = resTmp;
		}
	}

	return res;
}

int readFile(const char *pInputFileName, void *pInputBuf, uint32_t inputFileSize)
{
	int res = 0;
	int resTmp;

	SceUID uidFd = -1;

	// Open an input file
	res = sceIoOpen(pInputFileName, SCE_O_RDONLY, 0);
	if (res < 0) {
		sceClibPrintf("error: sceIoOpen() failed: 0x%08X\n", res);
	}
	uidFd = res;
	res = 0;
	// Read an input file
	do {
		res = sceIoRead(uidFd, pInputBuf, inputFileSize);
		if (res < 0) {
			sceClibPrintf("error: sceIoRead() failed: 0x%08X\n", res);
			res = -1;
		}
		pInputBuf = (uint8_t *)pInputBuf + res;
		inputFileSize -= res;
	} while (0 < inputFileSize);
	res = 0;

	if (0 <= uidFd) {
		// Close an input file
		resTmp = sceIoClose(uidFd);
		if (resTmp < 0) {
			sceClibPrintf("error: sceIoClose() failed: 0x%08X\n", resTmp);
			res = resTmp;
		}
	}

	return res;
}

int decodeAudio(SceAudiodecCtrl *pCtrl, Buffer *pInput, Buffer *pOutput)
{
	int res = 0;

	// Set elementary stream and PCM buffer
	pCtrl->pEs = pInput->p + pInput->offsetR;
	pCtrl->pPcm = pOutput->p + pOutput->offsetW;

	// Decode audio data
	res = sceAudiodecDecode(pCtrl);
	if (res < 0) {
		int32_t error = 0;
		sceAudiodecGetInternalError(pCtrl, &error);
		sceClibPrintf("error: sceAudiodecDecode() failed: 0x%08X (%d)\n", res, error);
	}

	//SCE_DBG_ALWAYS_ASSERT(pCtrl->outputPcmSize <= pOutput->size / DOUBLE_BUFFER);

	// Update offset
	pInput->offsetR += pCtrl->inputEsSize;
	pOutput->offsetW = (pOutput->offsetW + pOutput->size / 2) % pOutput->size;

	return res;
}

int outputAudio(Buffer *pOutput)
{
	int res = 0;

	if (pOutput != NULL) {
		// Output audio data
		res = sceAudioOutOutput(s_audioOut.portId, pOutput->p + pOutput->offsetR);
		if (res < 0) {
			sceClibPrintf("error: sceAudioOutOutput() failed: 0x%08X\n", res);
		}
		// Update offset
		pOutput->offsetR = (pOutput->offsetR + pOutput->size / 2) % pOutput->size;
	}
	else {
		// Output remaining audio data
		res = sceAudioOutOutput(s_audioOut.portId, NULL);
		if (res < 0) {
			sceClibPrintf("error: sceAudioOutOutput() failed: 0x%08X\n", res);
		}
	}

	return res;
}

vitaSASAudio* vitaSAS_internal_load_audio_AT9(const char* soundPath)
{
	vitaSASAudio* info = sceClibMspaceMalloc(mspace_internal, sizeof(vitaSASAudio));

	SceUID uidMemBlock, uidUnmap;
	const uint32_t memBlockSize = 0x100000U;
	void *pMemBlock = NULL;
	SceAudiodecCtrl audiodecCtrl;
	SceAudiodecInfo audiodecInfo;
	uint32_t vaContext = 0;
	uint32_t contextSize = 0;
	uint32_t pcmSize = 0;
	FileStream input;
	FileStream output;
	int res = 0;

	input.file.pName = "app0:15dB.at9";
	input.file.size = 0;
	input.buf.p = NULL;
	input.buf.offsetR = 0;
	input.buf.offsetW = 0;

	output.buf.p = NULL;
	output.buf.offsetR = 0;
	output.buf.offsetW = 0;

	// Get an input file size
	res = getFileSize(input.file.pName, &input.file.size);
	if (res < 0) {
		sceClibPrintf("error: getFileSize() failed: 0x%08X\n", res);
	}

	// Get an input buffer size
	uint32_t sizeEx = SCE_AUDIODEC_AT9_MAX_ES_SIZE;
	input.buf.size = SCE_AUDIODEC_ROUND_UP(input.file.size + sizeEx);

	// Allocate an input buffer
	input.buf.p = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, input.buf.size);
	if (input.buf.p == NULL) {
		sceClibPrintf("error: no more memory of %u byte\n", input.file.size);
	}

	// Read whole of an input file
	res = readFile(input.file.pName, input.buf.p, input.file.size);
	if (res < 0) {
		sceClibPrintf("error: readFile() failed: 0x%08X\n", res);
	}
	input.buf.offsetW = input.file.size;

	// Allocate a cache-disabled and physical continuous memory that is enabled for
    // reading and writing by the user
	uidMemBlock = sceKernelAllocMemBlock("PhysicallyContiguousMemoryLpddr",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, memBlockSize, NULL);
	if (uidMemBlock < 0) {
		sceClibPrintf("sceKernelAllocMemBlock error/n");
	}
	// Obtain starting address of allocated memory
	res = sceKernelGetMemBlockBase(uidMemBlock, &pMemBlock);
	if (res < 0) {
		sceClibPrintf("sceKernelGetMemBlockBase error/n");
	}

	// Remap as a cache-disabled and physical continuous memory that is enabled for
	// reading and writing by the Codec Engine but not by the user
	uidUnmap = sceCodecEngineOpenUnmapMemBlock(pMemBlock, memBlockSize);
	if (uidUnmap < 0) {
		sceClibPrintf("sceCodecEngineOpenUnmapMemBlock error/n");
	}

	At9Header header;
	uint32_t headerSize;

	res = parseRiffWaveHeaderForAt9(&header, input.buf.p, input.buf.size);
	if (res < 0) {
		sceClibPrintf("error: parseRiffWaveHeaderForAt9() failed: 0x%08X\n", res);
	}
	headerSize = res;

	// Set SceAudiodecCtrl
	sceClibMemset(&audiodecCtrl, 0, sizeof(SceAudiodecCtrl));
	audiodecCtrl.size = sizeof(SceAudiodecCtrl);
	audiodecCtrl.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;
	audiodecCtrl.pInfo = &audiodecInfo;

	// Set SceAudiodecInfo
	sceClibMemset(&audiodecInfo, 0, sizeof(SceAudiodecInfo));
	audiodecCtrl.pInfo->size = sizeof(audiodecCtrl.pInfo->at9);
	sceClibMemcpy(audiodecCtrl.pInfo->at9.configData, header.fmtChunk.configData, sizeof(audiodecCtrl.pInfo->at9.configData));
	input.buf.offsetR = headerSize;

	// Obtain required memory size
	contextSize = sceAudiodecGetContextSize(&audiodecCtrl,
		SCE_AUDIODEC_TYPE_AT9);
	if (contextSize <= 0) {
		sceClibPrintf("sceAudiodecGetContextSize error/n");
	}
	// Allocate memory from remapped area
	vaContext = sceCodecEngineAllocMemoryFromUnmapMemBlock(uidUnmap, contextSize,
		SCE_AUDIODEC_ALIGNMENT_SIZE);
	if (vaContext == 0) {
		sceClibPrintf("sceCodecEngineAllocMemoryFromUnmapMemBlock error/n");
	}
	// Generate AAC audio decoders
	res = sceAudiodecCreateDecoderExternal(&audiodecCtrl, SCE_AUDIODEC_TYPE_AT9,
		vaContext, contextSize);
	if (res < 0) {
		sceClibPrintf("sceAudiodecCreateDecoderExternal error: 0x%08x/n", res);
	}

	// grain
	s_audioOut.grain = audiodecCtrl.maxPcmSize / audiodecCtrl.pInfo->at9.ch / sizeof(int16_t);

	// Get an output buffer size
	pcmSize = sizeof(int16_t) * s_audioOut.ch * s_audioOut.grain;
	output.buf.size = SCE_AUDIODEC_ROUND_UP(pcmSize) * 2;

	// Allocate an output buffer
	output.buf.p = sceClibMspaceMemalign(mspace_internal, SCE_AUDIODEC_ALIGNMENT_SIZE, output.buf.size);
	if (output.buf.p == NULL) {
		sceClibPrintf("error: no more memory of %u byte\n", output.buf.size);
	}

	while (1) {
		// Decode audio data
		res = decodeAudio(&audiodecCtrl, &input.buf, &output.buf);
		if (res < 0) {
			sceClibPrintf("error: decodeAudio() failed: 0x%08X\n", res);
			sceClibPrintf("error: offset / filesize = %u / %u byte\n", input.buf.offsetR, input.file.size);
		}
		// Output audio data
		res = outputAudio(&output.buf);
		if (res < 0) {
			sceClibPrintf("error: outputAudio() failed: 0x%08X\n", res);
		}
	}

	return 0;
}

