﻿#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <psp2/audioout.h>

#define WAVE_FORMAT_EXTENSIBLE (0xFFFE)
#define SCE_AUDIODEC_AT9_MAX_ES_SIZE  1024
#define SCE_AUDIODEC_ROUND_UP(size) ((size + SCE_AUDIODEC_ALIGNMENT_SIZE - 1) & ~(SCE_AUDIODEC_ALIGNMENT_SIZE - 1))

typedef struct RiffWaveHeader {
	uint32_t chunkId;
	uint32_t chunkDataSize;
	uint32_t typeId;
} RiffWaveHeader;

typedef struct ChunkHeader {
	uint32_t chunkId;
	uint32_t chunkDataSize;
} ChunkHeader;

typedef struct FmtChunk {
	uint16_t formatTag; 
	uint16_t channels;
	uint32_t samplesPerSec;
	uint32_t avgBytesPerSec;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
	uint16_t cbSize;
	uint16_t samplesPerBlock;
	uint32_t chennelMask; 
	uint8_t subFormat[16]; 
	uint32_t versionInfo; 
	uint8_t configData[4];
	uint32_t reserved;
} FmtChunk;

typedef struct FactChunk {
	uint32_t totalSamples; // total samples per chennel
	uint32_t delaySamplesInputOverlap; // samples of input and overlap delay
	uint32_t delaySamplesInputOverlapEncoder; // samples of input and overlap and encoder delay
} FactChunk;

typedef struct SampleLoop {
	uint32_t identifier;
	uint32_t type;
	uint32_t start;
	uint32_t end;
	uint32_t fraction;
	uint32_t playCount;
} SampleLoop;

typedef struct SmplChunk {
	uint32_t manufacturer;
	uint32_t product;
	uint32_t samplePeriod;
	uint32_t midiUnityNote;
	uint32_t midiPitchFraction;
	uint32_t smpteFormat;
	uint32_t smpteOffset;
	uint32_t sampleLoops;
	uint32_t samplerData;
	SampleLoop sampleLoop;
} SmplChunk;

typedef struct At9Header {
	RiffWaveHeader riffWaveHeader;
	ChunkHeader fmtChunkHeader;
	FmtChunk fmtChunk;
	ChunkHeader factChunkHeader;
	FactChunk factChunk;
	ChunkHeader smplChunkHeader;
	SmplChunk smplChunk;
	ChunkHeader dataChunkHeader;
} At9Header;

typedef struct File {
	const char *pName;
	uint32_t size;
} File;

typedef struct Buffer {
	uint8_t *p;
	uint32_t size;
	uint32_t offsetR;
	uint32_t offsetW;
} Buffer;

typedef struct FileStream {
	File file;
	Buffer buf;
} FileStream;

typedef struct AudioOut {
	int32_t portId;
	int32_t portType;
	int32_t grain;
	int32_t samplingRate;
	int32_t param;
	int32_t volume[2];
	int32_t ch;
} AudioOut;

#endif
