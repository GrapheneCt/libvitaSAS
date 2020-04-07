#ifndef _SCE_SAS_SAS_H
#define _SCE_SAS_SAS_H

#include <psp2/types.h>

#define SCE_SAS_VOICE_MAX				(32)

#define SCE_SAS_GRAIN_SAMPLES			(256)

#define SCE_SAS_VOLUME_MAX				(0x1000)
#define SCE_SAS_FX_VOLUME_MAX			(SCE_SAS_VOLUME_MAX)

#define SCE_SAS_LOOP_DISABLE			(0)
#define SCE_SAS_LOOP_ENABLE				(1)
#define SCE_SAS_LOOP_MODE_COUNT			(2)

#define SCE_SAS_PITCH_MIN				(1)
#define SCE_SAS_PITCH_MAX				(0x4000)
#define SCE_SAS_PITCH_BASE				(0x1000)
#define SCE_SAS_NOISE_CLOCK_MAX			(0x3f)

#define SCE_SAS_DISTORTION_FLAG_ENABLE	(1UL<<0)

#define SCE_SAS_DISTORTION_DRIVE_MIN	(0)
#define SCE_SAS_DISTORTION_DRIVE_MAX	(0x20000)
#define SCE_SAS_DISTORTION_DRIVE_BASE	(0x1000)
#define SCE_SAS_DISTORTION_GAIN_MIN		(0)
#define SCE_SAS_DISTORTION_GAIN_MAX		(0x20000)
#define SCE_SAS_DISTORTION_GAIN_BASE	(0x1000)

#define SCE_SAS_ENVELOPE_HEIGHT_MAX		(0x40000000)
#define SCE_SAS_ENVELOPE_RATE_MAX		(0x7fffffff)

#define SCE_SAS_ADSR_MODE_LINEAR_INC	(0)
#define SCE_SAS_ADSR_MODE_LINEAR_DEC	(1)
#define SCE_SAS_ADSR_MODE_BENT_LINEAR	(2)
#define SCE_SAS_ADSR_MODE_REVEXPONENT	(3)
#define SCE_SAS_ADSR_MODE_EXPONENT		(4)
#define SCE_SAS_ADSR_MODE_DIRECT		(5)


#define SCE_SAS_ATTACK_VALID			(1)
#define SCE_SAS_DECAY_VALID				(2)
#define SCE_SAS_SUSTAIN_VALID			(4)
#define SCE_SAS_RELEASE_VALID			(8)

#define SCE_SAS_OUTPUTMODE_STEREO		(0)
#define SCE_SAS_OUTPUTMODE_MULTI		(1)

#define SCE_SAS_FX_TYPE_OFF				(-1)
#define SCE_SAS_FX_TYPE_ROOM			(0)
#define SCE_SAS_FX_TYPE_STUDIOA			(1)
#define SCE_SAS_FX_TYPE_STUDIOB			(2)
#define SCE_SAS_FX_TYPE_STUDIOC			(3)
#define SCE_SAS_FX_TYPE_HALL			(4)
#define SCE_SAS_FX_TYPE_SPACE			(5)
#define SCE_SAS_FX_TYPE_ECHO			(6)
#define SCE_SAS_FX_TYPE_DELAY			(7)
#define SCE_SAS_FX_TYPE_PIPE			(8)
#define SCE_SAS_FX_TYPE_COUNT			(9)

#define SCE_SAS_PAUSE_DISABLE			(0)
#define SCE_SAS_PAUSE_ENABLE			(1)

typedef SceInt32 SceSasResult;

#define SCE_SAS_SUCCEEDED(r)			((SceInt32)(r) >= 0)
#define SCE_SAS_FAILED(r)				((SceInt32)(r) < 0)
#define SCE_SAS_ERROR_FAIL						-2143158272	/* 0x80420000 */
#define SCE_SAS_ERROR_NSMPL						-2143158271	/* 0x80420001 */
#define SCE_SAS_ERROR_MFMT						-2143158269	/* 0x80420003 */
#define SCE_SAS_ERROR_ADDRESS					-2143158267	/* 0x80420005 */
#define SCE_SAS_ERROR_VOICE_INDEX				-2143158256	/* 0x80420010 */
#define SCE_SAS_ERROR_NOISE_CLOCK				-2143158255	/* 0x80420011 */
#define SCE_SAS_ERROR_PITCH_VAL					-2143158254	/* 0x80420012 */
#define SCE_SAS_ERROR_ADSR_MODE					-2143158253	/* 0x80420013 */
#define SCE_SAS_ERROR_ADPCM_SIZE				-2143158252	/* 0x80420014 */
#define SCE_SAS_ERROR_LOOP_MODE					-2143158251	/* 0x80420015 */
#define SCE_SAS_ERROR_INVALID_STATE				-2143158250	/* 0x80420016 */
#define SCE_SAS_ERROR_VOLUME_VAL				-2143158248	/* 0x80420018 */
#define SCE_SAS_ERROR_ADSR_VAL					-2143158247	/* 0x80420019 */
#define SCE_SAS_ERROR_PCM_SIZE					-2143158246	/* 0x8042001A */
#define SCE_SAS_ERROR_ATRAC3_SIZE				-2143158245	/* 0x8042001B */
#define SCE_SAS_ERROR_CONFIG					-2143158244	/* 0x8042001C */
#define SCE_SAS_ERROR_DISTORTION_VAL			-2143158235	/* 0x80420025 */
#define SCE_SAS_ERROR_FX_TYPE					-2143158240	/* 0x80420020 */
#define SCE_SAS_ERROR_FX_FEEDBACK				-2143158239	/* 0x80420021 */
#define SCE_SAS_ERROR_FX_DELAY					-2143158238	/* 0x80420022 */
#define SCE_SAS_ERROR_FX_VOLUME_VAL				-2143158237	/* 0x80420023 */
#define SCE_SAS_ERROR_FX_UNAVAILABLE			-2143158236	/* 0x80420024 */
#define SCE_SAS_ERROR_BUSY						-2143158224	/* 0x80420030 */
#define SCE_SAS_ERROR_NOTINIT					-2143158016	/* 0x80420100 */
#define SCE_SAS_ERROR_ALRDYINIT					-2143158015	/* 0x80420101 */

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

SceSasResult sceSasGetNeededMemorySize(const char *config, SceSize *outSize);
SceSasResult sceSasInit(const char *config, void *buffer, SceSize bufferSize);
SceSasResult sceSasInitWithGrain(const char *config, SceUInt32 grain, void *buffer, SceSize bufferSize);
SceSasResult sceSasExit(void **outBuffer, SceSize *outBufferSize);

SceSasResult sceSasSetGrain(SceUInt32 grain);
SceSasResult sceSasGetGrain(void);
SceSasResult sceSasSetOutputmode(SceUInt32 outputmode);
SceSasResult sceSasGetOutputmode(void);

SceSasResult sceSasCore(SceInt16 *out);
SceSasResult sceSasCoreWithMix(SceInt16 *inOut, SceInt32 lvol, SceInt32 rvol);

SceSasResult sceSasSetVoice(SceInt32 iVoiceNum, const void *vagBuf, SceSize size, SceUInt32 loopflag);
SceSasResult sceSasSetVoicePCM(SceInt32 iVoiceNum, const void *pcmBuf, SceSize size, SceInt32 loopsize);
SceSasResult sceSasSetNoise(SceInt32 iVoiceNum, SceUInt32 uClk);
SceSasResult sceSasSetVolume(SceInt32 iVoiceNum, SceInt32 l, SceInt32 r, SceInt32 wl, SceInt32 wr);
SceSasResult sceSasSetPitch(SceInt32 iVoiceNum, SceInt32 pitch);
SceSasResult sceSasSetADSR(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 ar, SceUInt32 dr, SceUInt32 sr, SceUInt32 rr);
SceSasResult sceSasSetADSRmode(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 am, SceUInt32 dm, SceUInt32 sm, SceUInt32 rm);
SceSasResult sceSasSetSL(SceInt32 iVoiceNum, SceUInt32 sl);
SceSasResult sceSasSetSimpleADSR(SceInt32 iVoiceNum, SceUInt16 adsr1, SceUInt16 adsr2);

SceSasResult sceSasSetKeyOn(SceInt32 iVoiceNum);
SceSasResult sceSasSetKeyOff(SceInt32 iVoiceNum);

SceSasResult sceSasSetPause(SceInt32 iVoiceNum, SceUInt32 pauseFlag);

SceSasResult sceSasGetPauseState(SceInt32 iVoiceNum);
SceSasResult sceSasGetEndState(SceInt32 iVoiceNum);
SceSasResult sceSasGetEnvelope(SceInt32 iVoiceNum);

SceSasResult sceSasSetEffect(SceInt32 drySwitch, SceInt32 wetSwitch);
SceSasResult sceSasSetEffectType(SceInt32 type);
SceSasResult sceSasSetEffectVolume(SceInt32 valL, SceInt32 valR);
SceSasResult sceSasSetEffectParam(SceUInt32 delayTime, SceUInt32 feedback);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif
