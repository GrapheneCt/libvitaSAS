#ifndef _SCE_CODECENGINE_CODECENGINE_H
#define _SCE_CODECENGINE_CODECENGINE_H

#include <psp2/types.h>

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif /* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#define SCE_CODECENGINE_ERROR_INVALID_POINTER           (-2141192192) /* 0x80600000 */
#define SCE_CODECENGINE_ERROR_INVALID_SIZE              (-2141192191) /* 0x80600001 */
#define SCE_CODECENGINE_ERROR_INVALID_ALIGNMENT         (-2141192190) /* 0x80600002 */
#define SCE_CODECENGINE_ERROR_NOT_PHYSICALLY_CONTIGUOUS (-2141192189) /* 0x80600003 */
#define SCE_CODECENGINE_ERROR_INVALID_RANGE             (-2141192188) /* 0x80600004 */
#define SCE_CODECENGINE_ERROR_INVALID_HEAP              (-2141192187) /* 0x80600005 */
#define SCE_CODECENGINE_ERROR_HEAP_BROKEN               (-2141192186) /* 0x80600006 */
#define SCE_CODECENGINE_ERROR_NO_MORE_ENTRY             (-2141192185) /* 0x80600007 */
#define SCE_CODECENGINE_ERROR_INVALID_MEMTYPE           (-2141192184) /* 0x80600008 */
#define SCE_CODECENGINE_ERROR_INVALID_VALUE             (-2141192183) /* 0x80600009 */
#define SCE_CODECENGINE_ERROR_MEMORY_NOT_ALLOCATED      (-2141192182) /* 0x8060000A */
#define SCE_CODECENGINE_ERROR_MEMORY_IN_USE             (-2141192181) /* 0x8060000B */
#define SCE_CODECENGINE_ERROR_MEMORY_NOT_IN_USE         (-2141192180) /* 0x8060000C */

extern SceUID sceCodecEngineOpenUnmapMemBlock(void *pMemBlock, SceUInt32 memBlockSize);
extern SceInt32 sceCodecEngineCloseUnmapMemBlock(SceUID uid);
extern SceUIntVAddr sceCodecEngineAllocMemoryFromUnmapMemBlock(SceUID uid, SceUInt32 size, SceUInt32 alignment);
extern SceInt32 sceCodecEngineFreeMemoryFromUnmapMemBlock(SceUID uid, SceUIntVAddr p);

typedef struct SceCodecEnginePmonProcessorLoad {
	SceUInt32 size;
	SceUInt32 average;
} SceCodecEnginePmonProcessorLoad;

extern SceInt32 sceCodecEnginePmonStart(void);
extern SceInt32 sceCodecEnginePmonStop(void);
extern SceInt32 sceCodecEnginePmonGetProcessorLoad(SceCodecEnginePmonProcessorLoad *pProcessorLoad);
extern SceInt32 sceCodecEnginePmonReset(void);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif /* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#endif /* _SCE_CODECENGINE_CODECENGINE_H */
