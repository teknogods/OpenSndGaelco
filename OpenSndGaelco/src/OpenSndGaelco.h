/*
* Created by Harm for TeknoParrot
* This file is part of the OpenParrot project - https://teknoparrot.com / https://github.com/teknogods
*
* See LICENSE and MENTIONS in the root of the source tree for information
* regarding licensing.
*/

#pragma once

#include <windows.h>
#include <xaudio2.h>

#define CHECK_HR(exp) { HRESULT hr = exp; if (FAILED(hr)) { info("failed %s: %08x\n", #exp, hr); abort(); } }

#define GAELCO_UNKNOWN     0
#define GAELCO_TOKYO_COP   1
#define GAELCO_TUNING_RACE 2
#define GAELCO_RING_RIDERS 3

// Tokyo Cop     8
// Tuning Race   12
// Ring Riders   10
#define CHANNELS 12

// Tokyo Cop    202
// Tuning Race  464
// Ring Riders  283
#define SAMPLES 464

#define TYPE_16BIT 2
#define TYPE_ADPCM 4

typedef struct {
	DWORD offset;
	DWORD size;
	DWORD loopStart;
	DWORD loopEnd;
	WORD phaseInc;
	WORD Vnorm;
	WORD rkon;
	WORD rkoff;
	WORD ramping;
	WORD type;
	DWORD padding;
} SAMPLE_HEADER;

typedef struct
{
	XAUDIO2_BUFFER xaBuffer;
	IXAudio2SourceVoice* xaVoice;
	BOOL playing;
	BOOL loop;
	DWORD sample;
	DWORD sampleCount;
} BUFFER;

typedef struct
{
	BYTE* data;
	DWORD size;
} SAMPLE;

#ifndef SPEAKER_FRONT_LEFT
#define SPEAKER_FRONT_LEFT            0x00000001
#define SPEAKER_FRONT_RIGHT           0x00000002
#define SPEAKER_FRONT_CENTER          0x00000004
#define SPEAKER_LOW_FREQUENCY         0x00000008
#define SPEAKER_BACK_LEFT             0x00000010
#define SPEAKER_BACK_RIGHT            0x00000020
#define SPEAKER_FRONT_LEFT_OF_CENTER  0x00000040
#define SPEAKER_FRONT_RIGHT_OF_CENTER 0x00000080
#define SPEAKER_BACK_CENTER           0x00000100
#define SPEAKER_SIDE_LEFT             0x00000200
#define SPEAKER_SIDE_RIGHT            0x00000400
#define SPEAKER_TOP_CENTER            0x00000800
#define SPEAKER_TOP_FRONT_LEFT        0x00001000
#define SPEAKER_TOP_FRONT_CENTER      0x00002000
#define SPEAKER_TOP_FRONT_RIGHT       0x00004000
#define SPEAKER_TOP_BACK_LEFT         0x00008000
#define SPEAKER_TOP_BACK_CENTER       0x00010000
#define SPEAKER_TOP_BACK_RIGHT        0x00020000
#define SPEAKER_RESERVED              0x7FFC0000
#define SPEAKER_ALL                   0x80000000
#define _SPEAKER_POSITIONS_
#endif

#ifndef SPEAKER_STEREO
#define SPEAKER_MONO             (SPEAKER_FRONT_CENTER)
#define SPEAKER_STEREO           (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define SPEAKER_2POINT1          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY)
#define SPEAKER_SURROUND         (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_CENTER)
#define SPEAKER_QUAD             (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)
#define SPEAKER_4POINT1          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)
#define SPEAKER_5POINT1          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT)
#define SPEAKER_7POINT1          (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER)
#define SPEAKER_5POINT1_SURROUND (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#define SPEAKER_7POINT1_SURROUND (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT  | SPEAKER_SIDE_RIGHT)
#endif

__declspec(dllexport) void GaelcoSoundInit(DWORD gameId, BOOL muteMusic, BOOL muteFx);
__declspec(dllexport) DWORD SW_init_sound_tuning(const char* filePath);
__declspec(dllexport) DWORD SW_init_sound(DWORD soundClass, const char* filePath);
__declspec(dllexport) void sound_master_volume(FLOAT volume);
__declspec(dllexport) void sound_master_balance(FLOAT left, FLOAT right, FLOAT subwoofer, FLOAT rear);
__declspec(dllexport) DWORD get_channel_status();
__declspec(dllexport) void sound_flush_commbuf();
__declspec(dllexport) void sound_start(DWORD channel, DWORD sample, DWORD flags, FLOAT p0, FLOAT p1, FLOAT p2, FLOAT p3); // not used
__declspec(dllexport) void sound_setup_channel(DWORD channel, DWORD sample, DWORD flags);
__declspec(dllexport) void sound_start_channel(DWORD channel);
__declspec(dllexport) void sound_freq(DWORD channel, FLOAT phaseInc);
__declspec(dllexport) void sound_panning(DWORD channel, FLOAT left, FLOAT right, FLOAT subwoofer, FLOAT rear);
__declspec(dllexport) void sound_volrate(DWORD channel, FLOAT volume, FLOAT rate); // not used
__declspec(dllexport) void sound_volume(DWORD channel, FLOAT volume);
__declspec(dllexport) void sound_keyoff(FLOAT rate, DWORD channelMask);
__declspec(dllexport) void sound_stop(DWORD channelMask);
__declspec(dllexport) void sound_reset();
__declspec(dllexport) DWORD commbufferReady();