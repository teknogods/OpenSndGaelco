/*
* Created by Harm for TeknoParrot
* This file is part of the OpenParrot project - https://teknoparrot.com / https://github.com/teknogods
*
* See LICENSE and MENTIONS in the root of the source tree for information
* regarding licensing.
*/

extern "C" {
	#include "OpenSndGaelco.h"
}
#include <vector>
#include <xaudio2.h>
#include <shlwapi.h>
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "shlwapi.lib")

#ifdef _DEBUG
void info(const char* format, ...)
{
	va_list args;
	char buffer[1024];

	va_start(args, format);
	int len = _vsnprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	buffer[len] = '\n';
	buffer[len + 1] = '\0';

	OutputDebugStringA(buffer);
}
#else
#define info(x, ...) {}
#endif

// Options
DWORD GaelcoGameId = GAELCO_UNKNOWN;
BOOL MuteMusic = FALSE;
BOOL MuteFx = FALSE;

// Buffers
BUFFER channelBuffer[CHANNELS];
SAMPLE sampleBuffer[SAMPLES];

// Xaudio2
IXAudio2* pXAudio2 = NULL;
IXAudio2MasteringVoice* pMasterVoice = NULL;
BYTE* pDataBuffer = NULL;
DWORD bufferSize = 0;
BOOL bufferReady = FALSE;
DWORD sampleCount = 0;
WAVEFORMATEX xaFormat = { 0 };

static DWORD GetChannelSample(DWORD channel)
{
	DWORD sample = 0;

	if (channel < CHANNELS)
	{
		sample = channelBuffer[channel].sample;
	}

	return sample;
}

// First table lookup for Ima-ADPCM quantizer
static const char IndexAdjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

// Second table lookup for Ima-ADPCM quantizer
static const short StepSize[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static void DecodeAdpcmSample(DWORD sample)
{
	SAMPLE_HEADER* pSampleHeader = (SAMPLE_HEADER *)pDataBuffer + sample;

	BOOL decoding = TRUE;
	DWORD phaseLO = 0;
	DWORD phaseHI = 0;
	SHORT data0 = 0;
	SHORT data1 = 0;
	BYTE aindex = 0;
	BOOL next = FALSE;
	DWORD cursor = 0;
	BOOL skipFirst = TRUE;

	DWORD phaseEndLO = pSampleHeader->size << 12;
	DWORD phaseEndHI = pSampleHeader->size >> 20;
	DWORD decodedSize = (ULONG64)pSampleHeader->size * 0x1000 / pSampleHeader->phaseInc * 2;

	sampleBuffer[sample].data = new BYTE[decodedSize];
	sampleBuffer[sample].size = decodedSize;

	while (decoding)
	{
		INT d0;
		DWORD ph0;
		DWORD ph1;
		BYTE *pp;

		INT idx;
		INT data;

		ph0 = phaseLO;

		if (!next) {
			idx = (ph0 >> 12) + (phaseHI << 20);
			pp = (BYTE *)pDataBuffer + pSampleHeader->offset;
			data = pp[idx >> 1];
			if (!(idx & 1)) data >>= 4;
			data1 = 0;

			INT index, sign, vout, step, vpdiff;
			INT delta = data;

			delta &= 0x0f;
			index = aindex;
			step = StepSize[index];

			sign = delta & 8;
			delta = delta & 7;

			index += IndexAdjust[delta];

			if (index < 0) index = 0;
			if (index > 88) index = 88;

			vout = data0;
			vpdiff = step >> 3;

			if (delta & 4) vpdiff += step;
			if (delta & 2) vpdiff += step >> 1;
			if (delta & 1) vpdiff += step >> 2;

			if (sign)
				vout -= vpdiff;
			else
				vout += vpdiff;

			if (vout > 32767)
				vout = 32767;
			else if (vout < -32768)
				vout = -32768;

			aindex = index;

			data1 = (SHORT)vout;
		}

		d0 = (INT)(ph0 & 0xFFF);
		data = data0 + (((data1 - data0) * d0) >> 12);
		data = (data * 0x1000) >> 12;

		ph1 = ph0 + pSampleHeader->phaseInc;
		if ((0xffffffff - ph0) < pSampleHeader->phaseInc) phaseHI++;
		phaseLO = ph1;

		if (!((ph0 ^ ph1) & 0x1000))
		{
			next = TRUE;
		}
		else
		{
			next = FALSE;
			data0 = data1;
		}
		
		// Check if we are done
		if ((ph1 >= phaseEndLO) && (phaseHI >= phaseEndHI))
			decoding = FALSE;

		// Clamp output
		if (data < -0x7FFF)
			data = -0x7FFF;
		else if (data > 0x7FFF)
			data = 0x7FFF;

		// Put in buffer
		if (!skipFirst)
		{
			sampleBuffer[sample].data[cursor] = (BYTE)(data & 0xFF);
			sampleBuffer[sample].data[cursor + 1] = (BYTE)(data >> 8);
			cursor += 2;
		}

		skipFirst = FALSE;
	}
}

static void Decode16Sample(DWORD sample)
{
	SAMPLE_HEADER* pSampleHeader = (SAMPLE_HEADER*)pDataBuffer + sample;

	BOOL decoding = TRUE;
	DWORD phase = 0;
	DWORD phaseEnd = pSampleHeader->size << 12;
	DWORD decodedSize = phaseEnd / pSampleHeader->phaseInc * 2;
	DWORD cursor = 0;
	BOOL skipFirst = TRUE;

	sampleBuffer[sample].data = new BYTE[decodedSize];
	sampleBuffer[sample].size = decodedSize;

	while (decoding)
	{
		INT d0;
		INT d1;
		INT data;
		DWORD idx;

		idx = ((phase >> 12) << 1);
		d0 = *(SHORT*)(pDataBuffer + pSampleHeader->offset + idx);

		if ((idx + 2) < pSampleHeader->size * 2)
			d1 = *(SHORT*)(pDataBuffer + pSampleHeader->offset + idx + 2);
		else
			d1 = d0;

		// Linear interpolation
		data = d0 + (((d1 - d0) * ((INT)phase & 0xfff)) >> 12);
		data = (data * 0x1000) >> 12;
		phase += pSampleHeader->phaseInc;

		// Check if we are done
		if (phase >= phaseEnd)
			decoding = FALSE;

		// Clamp output
		if (data < -0x7fff)
			data = -0x7fff;
		else if (data > 0x7fff)
			data = 0x7fff;

		// Put in buffer
		if (!skipFirst)
		{
			sampleBuffer[sample].data[cursor] = (BYTE)(data & 0xFF);
			sampleBuffer[sample].data[cursor + 1] = (BYTE)(data >> 8);
			cursor += 2;
		}

		skipFirst = FALSE;
	}
}

__declspec(dllexport) void GaelcoSoundInit(DWORD gameId, BOOL muteMusic, BOOL muteFx)
{
	info("GaelcoSoundInit gameId: %u muteMusic: %u muteFx: %u", gameId, muteMusic, muteFx);

	GaelcoGameId = gameId;
	MuteMusic = muteMusic;
	MuteFx = muteFx;
}

static BOOL ToMuteOrNotToMute(DWORD sample)
{
	BOOL mute = FALSE;
	BOOL music = FALSE;

	if (!MuteMusic && !MuteFx)
		return mute;

	switch (GaelcoGameId)
	{
		case GAELCO_TOKYO_COP:
			if ((sample >= 12 && sample <= 25) || (sample >= 56 && sample <= 133))
				music = TRUE;

			break;
		case GAELCO_TUNING_RACE:
			if ((sample >= 55 && sample <= 56) || (sample >= 165 && sample <= 276) || (sample >= 321 && sample <= 432))
				music = TRUE;

			break;
		case GAELCO_RING_RIDERS:
			if ((sample >= 1 && sample <= 10) || (sample >= 152 && sample <= 249) || (sample >= 276 && sample <= 279))
				music = TRUE;

			break;
		default:
			return mute;
			break;
	}

	if ((MuteMusic && music) || (MuteFx && !music))
		mute = TRUE;

	return mute;
}

__declspec(dllexport) DWORD SW_init_sound_tuning(const char* filePath)
{
	return SW_init_sound(0x1337, filePath);
}

__declspec(dllexport) DWORD SW_init_sound(DWORD soundClass, const char* filePath)
{
	// Convert relative path to absolute
	char buf[MAX_PATH];
	memset(buf, 0, sizeof(buf));
	GetCurrentDirectoryA(256, buf);
	auto len = strlen(buf);
	buf[len] = '\\';
	strcat_s(buf, "roms.new");

	info("OpenSndGaelco::SW_init_sound filePath: %s %s", filePath, buf);

	bufferReady = FALSE;

	// Init xAudio2
	CoInitialize(nullptr);
	CHECK_HR(XAudio2Create(&pXAudio2));
	CHECK_HR(pXAudio2->CreateMasteringVoice(&pMasterVoice));

	// Load sprite into memory
	HANDLE hFile = CreateFileA((LPCSTR)buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		info("OpenSndGaelco::SW_init_sound ERROR! Opening file failed(%u)", GetLastError());
		return 0;
	}

	bufferSize = GetFileSize(hFile, NULL);
	pDataBuffer = new BYTE[bufferSize];
	DWORD bytesRead = 0; // this is needed for windows 7
	ReadFile(hFile, pDataBuffer, bufferSize, &bytesRead, NULL);
	sampleCount = *((DWORD*)pDataBuffer) / sizeof(SAMPLE_HEADER);

	info("OpenSndGaelco::SW_init_sound bufferSize: %u pDataBuffer: %08X sampleCount: %u", bufferSize, pDataBuffer, sampleCount);

	// Init samples buffer
	for (DWORD sample = 0; sample < SAMPLES; sample++)
	{
		sampleBuffer[sample].data = NULL;
		sampleBuffer[sample].size = 0;
	}

	// Init channel buffer
	for (DWORD channel = 0; channel < CHANNELS; channel++)
	{
		channelBuffer[channel].xaBuffer = { 0 };
		channelBuffer[channel].xaVoice = NULL;
		channelBuffer[channel].playing = FALSE;
		channelBuffer[channel].loop = FALSE;
		channelBuffer[channel].sample = 0;
		channelBuffer[channel].sampleCount = 0;
	}

	// Decode all samples
	for (DWORD sample = 0; sample < sampleCount; sample++)
	{
		SAMPLE_HEADER* pSampleHeader = (SAMPLE_HEADER*)pDataBuffer + sample;

		// Skip holes
		if (pSampleHeader->size == 0)
			continue;

		// How to decode
		if (pSampleHeader->type == TYPE_16BIT)
			Decode16Sample(sample);
		else if (pSampleHeader->type == TYPE_ADPCM)
			DecodeAdpcmSample(sample);
		else
			info("OpenSndGaelco::SW_init_sound ERROR! Sample type(%u) not supported", pSampleHeader->type);
	}

	// Setup format (always the same because we decode everything first)
	xaFormat.nAvgBytesPerSec = 88200;
	xaFormat.nSamplesPerSec = 44100;
	xaFormat.wBitsPerSample = 16;
	xaFormat.nChannels = 1;
	xaFormat.wFormatTag = WAVE_FORMAT_PCM;
	xaFormat.nBlockAlign = 2;

	bufferReady = TRUE;

	return 1;
}

__declspec(dllexport) void sound_master_volume(FLOAT volume)
{
	info("OpenSndGaelco::sound_master_volume Volume: %f", volume);

	if (!bufferReady) { info("OpenSndGaelco::sound_master_volume ERROR! Buffer not ready!"); return; }

	CHECK_HR(pMasterVoice->SetVolume(volume));

	return;
}

__declspec(dllexport) void sound_master_balance(FLOAT left, FLOAT right, FLOAT subwoofer, FLOAT rear)
{
	info("OpenSndGaelco::sound_master_balance left: %f right: %f subwoofer: %f rear: %f", left, right, subwoofer, rear);

	if (!bufferReady) { info("OpenSndGaelco::sound_master_balance ERROR! Buffer not ready!"); return; }

	return;
}

__declspec(dllexport) DWORD get_channel_status()
{
	info("OpenSndGaelco::get_channel_status");

	if (!bufferReady) { info("OpenSndGaelco::get_channel_status ERROR! Buffer not ready!"); return 0; }

	DWORD status = 0;

	for (DWORD i = 0; i < CHANNELS; i++)
	{
		if (channelBuffer[i].playing)
		{
			XAUDIO2_VOICE_STATE vs;
			channelBuffer[i].xaVoice->GetState(&vs);

			DWORD sPlayed = 123;
			DWORD sCount = 123;
			sPlayed = vs.SamplesPlayed;
			sCount = channelBuffer[i].sampleCount;

			info("OpenSndGaelco::get_channel_status channel: %u sample: %u BuffersQueued: %u loop: %u SamplesPlayed: %u sampleCount: %u", i, channelBuffer[i].sample, vs.BuffersQueued, channelBuffer[i].loop, sPlayed, sCount);

			//if (vs.BuffersQueued == 0 || (!channelBuffer[i].loop && vs.SamplesPlayed >= channelBuffer[i].sampleCount))
			//if (vs.BuffersQueued == 0)
			if (vs.BuffersQueued == 0)
			{
				info("stopping: %u (done) loop: %u sampleCount: %u SamplesPlayed: %u", i, channelBuffer[i].loop, sCount, sPlayed);
				CHECK_HR(channelBuffer[i].xaVoice->Stop());
				channelBuffer[i].playing = FALSE;
			}
			else
			{
				status |= 1 << i;
			}
		}
	}

	info("OpenSndGaelco::get_channel_status Status: %u", status);

	return status;
}

__declspec(dllexport) void sound_flush_commbuf()
{
	info("OpenSndGaelco::sound_flush_commbuf");
	return;
}

// Not used
__declspec(dllexport) void sound_start(DWORD channel, DWORD sample, DWORD flags, FLOAT p0, FLOAT p1, FLOAT p2, FLOAT p3)
{
	info("OpenSndGaelco::sound_start Channel: %u Sample: %u Flags: %u p0: %f p1: %f p2: %f p3: %f", channel, sample, flags, p0, p1, p2, p3);

	return;
}

__declspec(dllexport) void sound_setup_channel(DWORD channel, DWORD sample, DWORD flags)
{
	info("OpenSndGaelco::sound_setup_channel Channel: %u Sample: %u Flags: %x", channel, sample, flags);

	if (!bufferReady) { info("OpenSndGaelco::sound_setup_channel ERROR! Buffer not ready!"); return; }
	if (channel >= CHANNELS) { info("OpenSndGaelco::sound_setup_channel ERROR! Channel out of range"); return; }
	if (sample >= sampleCount) { info("OpenSndGaelco::sound_setup_channel ERROR! Sample out of range"); return; }
	if (sampleBuffer[sample].size == 0) { info("OpenSndGaelco::sound_setup_channel ERROR! Sample invalid"); return; }

	// Do we already have a voice?
	if (channelBuffer[channel].xaVoice == NULL)
	{
		CHECK_HR(pXAudio2->CreateSourceVoice(&channelBuffer[channel].xaVoice, &xaFormat));
	}
	else
	{
		CHECK_HR(channelBuffer[channel].xaVoice->FlushSourceBuffers());
	}

	// Stop if needed
	if (channelBuffer[channel].playing)
	{
		CHECK_HR(channelBuffer[channel].xaVoice->Stop());
		channelBuffer[channel].playing = FALSE;
	}

	// Reset voice properties
	CHECK_HR(channelBuffer[channel].xaVoice->SetFrequencyRatio(1.0f));
	sound_panning(channel, 1.0f, 1.0f, 1.0f, 1.0f);
	if (ToMuteOrNotToMute(sample))
	{
		CHECK_HR(channelBuffer[channel].xaVoice->SetVolume(0.0f));
	}
	else
	{
		CHECK_HR(channelBuffer[channel].xaVoice->SetVolume(1.0f));
	}

	// Setup buffer
	SAMPLE_HEADER* pSampleHeader = (SAMPLE_HEADER *)pDataBuffer + sample;

	channelBuffer[channel].sample = sample;
	channelBuffer[channel].sampleCount = sampleBuffer[sample].size / 2;

	channelBuffer[channel].loop = FALSE;
	channelBuffer[channel].xaBuffer.AudioBytes = sampleBuffer[sample].size;
	channelBuffer[channel].xaBuffer.pAudioData = sampleBuffer[sample].data;
	channelBuffer[channel].xaBuffer.Flags = XAUDIO2_END_OF_STREAM;
	channelBuffer[channel].xaBuffer.LoopBegin = 0;
	channelBuffer[channel].xaBuffer.LoopLength = 0;
	channelBuffer[channel].xaBuffer.LoopCount = 0;
	channelBuffer[channel].xaBuffer.PlayBegin = 0;
	channelBuffer[channel].xaBuffer.PlayLength = 0;
	channelBuffer[channel].xaBuffer.pContext = NULL;

	// Loop
	if (flags > 0)
	{
		channelBuffer[channel].loop = TRUE;
		channelBuffer[channel].xaBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	CHECK_HR(channelBuffer[channel].xaVoice->SubmitSourceBuffer(&channelBuffer[channel].xaBuffer));

	return;
}

__declspec(dllexport) void sound_start_channel(DWORD channel)
{
	info("OpenSndGaelco::sound_start_channel Channel: %u Sample: %u", channel, GetChannelSample(channel));

	if (!bufferReady) { info("OpenSndGaelco::sound_start_channel ERROR! Buffer not ready!"); return; }
	if (channel >= CHANNELS) { info("OpenSndGaelco::sound_start_channel ERROR! Channel out of range"); return; }
	if (channelBuffer[channel].xaVoice == NULL) { info("OpenSndGaelco::sound_start_channel ERROR! Voice is null"); return; }

	CHECK_HR(channelBuffer[channel].xaVoice->Start(0));
	channelBuffer[channel].playing = TRUE;

	return;
}

__declspec(dllexport) void sound_freq(DWORD channel, FLOAT phaseInc)
{
	info("OpenSndGaelco::sound_freq Channel: %u PhaseInc: %f Sample: %u", channel, phaseInc, GetChannelSample(channel));

	if (!bufferReady) { info("OpenSndGaelco::sound_freq ERROR! Buffer not ready!"); return; }
	if (channel >= CHANNELS) { info("OpenSndGaelco::sound_freq ERROR! Channel out of range"); return; }
	if (channelBuffer[channel].xaVoice == NULL) { info("OpenSndGaelco::sound_freq ERROR! Voice is null"); return; }

	CHECK_HR(channelBuffer[channel].xaVoice->SetFrequencyRatio(phaseInc));

	return;
}

__declspec(dllexport) void sound_panning(DWORD channel, FLOAT left, FLOAT right, FLOAT subwoofer, FLOAT rear)
{
	info("OpenSndGaelco::sound_panning Channel: %u left: %f right: %f subwoofer: %f rear: %f Sample: %u", channel, left, right, subwoofer, rear, GetChannelSample(channel));

	if (!bufferReady) { info("OpenSndGaelco::sound_panning ERROR! Buffer not ready!"); return; }
	if (channel >= CHANNELS) { info("OpenSndGaelco::sound_panning ERROR! Channel out of range"); return; }
	if (channelBuffer[channel].xaVoice == NULL) { info("OpenSndGaelco::sound_panning ERROR! Voice is null"); return; }

	DWORD dwChannelMask;
	CHECK_HR(pMasterVoice->GetChannelMask(&dwChannelMask));

	FLOAT outputMatrix[8];
	for (BYTE i = 0; i < 8; i++)
		outputMatrix[i] = 0.0f;

	info("OpenSndGaelco::sound_panning dwChannelMask: %u", dwChannelMask);

	switch (dwChannelMask)
	{
		case SPEAKER_MONO:
			outputMatrix[0] = max(left, right);
			break;
		case SPEAKER_STEREO:
			outputMatrix[0] = left;
			outputMatrix[1] = right;
			break;
		case SPEAKER_2POINT1:
			outputMatrix[0] = left;
			outputMatrix[1] = right;
			outputMatrix[2] = subwoofer;
			break;
		case SPEAKER_SURROUND:
			outputMatrix[0] = left;
			outputMatrix[1] = right;
			break;
		case SPEAKER_QUAD:
			outputMatrix[0] = outputMatrix[2] = left;
			outputMatrix[1] = outputMatrix[3] = right;
			break;
		case SPEAKER_4POINT1:
			outputMatrix[0] = outputMatrix[3] = left;
			outputMatrix[1] = outputMatrix[4] = right;
			outputMatrix[2] = subwoofer;
			break;
		case SPEAKER_5POINT1:
		case SPEAKER_7POINT1:
		case SPEAKER_5POINT1_SURROUND:
			outputMatrix[0] = outputMatrix[4] = left;
			outputMatrix[1] = outputMatrix[5] = right;
			outputMatrix[3] = subwoofer;
			break;
		case SPEAKER_7POINT1_SURROUND:
			outputMatrix[0] = outputMatrix[4] = outputMatrix[6] = left;
			outputMatrix[1] = outputMatrix[5] = outputMatrix[7] = right;
			outputMatrix[3] = subwoofer;
			break;
	}

	XAUDIO2_VOICE_DETAILS VoiceDetails;
	channelBuffer[channel].xaVoice->GetVoiceDetails(&VoiceDetails);

	XAUDIO2_VOICE_DETAILS MasterVoiceDetails;
	pMasterVoice->GetVoiceDetails(&MasterVoiceDetails);

	CHECK_HR(channelBuffer[channel].xaVoice->SetOutputMatrix(NULL, VoiceDetails.InputChannels, MasterVoiceDetails.InputChannels, outputMatrix));

	return;
}

__declspec(dllexport) void sound_volrate(DWORD channel, FLOAT volume, FLOAT rate)
{
	info("OpenSndGaelco::sound_volrate Channel: %u Volume: %f Rate: %f", channel, volume, rate);

	if (!bufferReady) { info("OpenSndGaelco::sound_volrate ERROR! Buffer not ready!"); return; }

	sound_volume(channel, volume);

	return;
}

__declspec(dllexport) void sound_volume(DWORD channel, FLOAT volume)
{
	info("OpenSndGaelco::sound_volume Channel: %u Volume: %f Sample: %u", channel, volume, GetChannelSample(channel));

	if (!bufferReady) { info("OpenSndGaelco::sound_volume ERROR! Buffer not ready!"); return; }
	if (channel >= CHANNELS) { info("OpenSndGaelco::sound_volume ERROR! Channel out of range"); return; }
	if (channelBuffer[channel].xaVoice == NULL) { info("OpenSndGaelco::sound_volume ERROR! Voice is null"); return; }

	if (ToMuteOrNotToMute(GetChannelSample(channel)))
		volume = 0.0f;

	CHECK_HR(channelBuffer[channel].xaVoice->SetVolume(volume));

	return;
}

__declspec(dllexport) void sound_keyoff(FLOAT rate, DWORD channelMask)
{
	info("OpenSndGaelco::sound_keyoff Rate: %f ChannelMask: %u", rate, channelMask);

	if (!bufferReady) { info("OpenSndGaelco::sound_keyoff ERROR! Buffer not ready!"); return; }

	return;
}

__declspec(dllexport) void sound_stop(DWORD channelMask)
{
	info("OpenSndGaelco::sound_stop ChannelMask: %u", channelMask);

	if (!bufferReady) { info("OpenSndGaelco::sound_stop ERROR! Buffer not ready!"); return; }

	for (DWORD i = 0; i < CHANNELS; i++)
	{
		if ((channelMask >> i) & 1)
		{
			// Dont print this, it happens alot
			if (!channelBuffer[i].playing) { continue; }
			if (channelBuffer[i].xaVoice == NULL) { continue; }

			info("OpenSndGaelco::sound_stop Stopping channel %u Sample: %u", i, GetChannelSample(i));
			CHECK_HR(channelBuffer[i].xaVoice->Stop());
			channelBuffer[i].playing = FALSE;
		}
	}
	
	return;
}

__declspec(dllexport) void sound_reset()
{
	info("OpenSndGaelco::sound_reset");

	if (!bufferReady) { info("OpenSndGaelco::sound_reset ERROR! Buffer not ready!"); return; }
	
	DWORD channelMask = 0;

	for (DWORD channel = 0; channel < CHANNELS; channel++)
		channelMask |= 1 << channel;

	sound_stop(channelMask);

	return;
}

__declspec(dllexport) DWORD commbufferReady()
{
	info("OpenSndGaelco::commbufferReady Ready: %u", bufferReady);

	return bufferReady;
}