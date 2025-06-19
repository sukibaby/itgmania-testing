#include "global.h"
#include "RageSoundDriver_WaveOut.h"

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

#include "RageTimer.h"
#include "RageLog.h"
#include "RageSound.h"
#include "RageUtil.h"
#include "RageSoundManager.h"
#include "PrefsManager.h"
#include "archutils/Win32/ErrorStrings.h"

#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <vector>


REGISTER_SOUND_DRIVER_CLASS( WaveOut );

namespace {
	// WaveOut targets a specific latency calculated from the sample rate,
	// number of buffers, and buffer size. For example, at 44100 Hz, with
	// 15 buffers of 256 frames each, the latency would be ~87 ms. At 48000 Hz,
	// with 16 buffers of 256 frames each, the latency would be ~85 ms.
	// We target a specific latency (118 ms) to ensure a consistent experience
	// for both 44100 and 48000 Hz sample rates.
	// This value was chosen because it has the smallest difference in actual
	// latency between 44100 and 48000 Hz (0.29 ms), which is almost negligible.
	constexpr int kTargetLatencyMilliseconds = 118;
	constexpr int kChannels = 2;
	constexpr int kChunkSizeFrames = 512;
	constexpr int kBytesPerFrame = kChannels * 2;  // 16 bit

	inline int CalculteNumBufferChunks( int sampleRate )
	{
	  return(sampleRate * kTargetLatencyMilliseconds + (1000 * kChunkSizeFrames - 1)) /
			 (1000 * kChunkSizeFrames);
	}
}  // namespace

static RString WaveOutErrorToString( MMRESULT err )
{
	char szBuf[MAXERRORLENGTH] = "";
	if( waveOutGetErrorText(err, szBuf, MAXERRORLENGTH) != MMSYSERR_NOERROR )
	{
		strncpy_s( szBuf, MAXERRORLENGTH, "Unknown error", _TRUNCATE );
	}
	return RString( szBuf );
}

int RageSoundDriver_WaveOut::MixerThread_start( void *p )
{
	((RageSoundDriver_WaveOut *) p)->MixerThread();
	return 0;
}

void RageSoundDriver_WaveOut::MixerThread()
{
	if( !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) )
		LOG->Warn( werr_ssprintf(GetLastError(), "Failed to set sound thread priority") );

	while( !m_bShutdown )
	{
		while( GetData() )
			;

		WaitForSingleObject( m_hSoundEvent, 10 );
	}

	waveOutReset( m_hWaveOut );
}

bool RageSoundDriver_WaveOut::GetData()
{
	/* Look for a free buffer. */
	int b;
	for( b = 0; b < wo_num_chunks; ++b )
		if( m_aBuffers[b].dwFlags & WHDR_DONE )
			break;
	if( b == wo_num_chunks )
		return false;

	/* Call the callback. */
	this->Mix( reinterpret_cast<int16_t*>(m_aBuffers[b].lpData), kChunkSizeFrames, m_iLastCursorPos, GetPosition() );

	MMRESULT ret = waveOutWrite( m_hWaveOut, &m_aBuffers[b], sizeof(m_aBuffers[b]) );
	if( ret != MMSYSERR_NOERROR )
	{
		Init();
		if (b_InitSuccess == false)
		{
			RString fail_message = WaveOutErrorToString( ret ) + "waveOutWrite failed";
			FAIL_M(fail_message.c_str());
		}
	}

	/* Increment m_iLastCursorPos. */
	m_iLastCursorPos += kChunkSizeFrames;

	return true;
}

void RageSoundDriver_WaveOut::SetupDecodingThread()
{
	if( !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) )
		LOG->Warn( werr_ssprintf(GetLastError(), "Failed to set sound thread priority") );
}

int64_t RageSoundDriver_WaveOut::GetPosition() const
{
	MMTIME tm = {};
	tm.wType = TIME_SAMPLES;
	MMRESULT ret = waveOutGetPosition( m_hWaveOut, &tm, sizeof(tm) );
  	if( ret != MMSYSERR_NOERROR )
	{
		RString fail_message = WaveOutErrorToString( ret ) + "waveOutGetPosition failed";
		FAIL_M(fail_message.c_str());
	}

	return tm.u.sample;
}

RageSoundDriver_WaveOut::RageSoundDriver_WaveOut()
:   m_hWaveOut(nullptr),
    m_hSoundEvent(CreateEvent(nullptr, false, true, nullptr)),
    m_iLastCursorPos(0),
    m_iSampleRate(0),
    m_bShutdown(false),
    b_InitSuccess(false),
    wo_num_chunks(1),
    wo_buffer_size_frames(0),
    wo_chunk_size(0),
    wo_buffer_size(0),
    m_aBuffers{},
    MixingThread() {}

RString RageSoundDriver_WaveOut::Init()
{
	b_InitSuccess = false;
	m_iSampleRate = PREFSMAN->m_iSoundPreferredSampleRate;
	if( m_iSampleRate == 0 )
	{
		m_iSampleRate = kFallbackSampleRate;
	}

	wo_num_chunks = CalculteNumBufferChunks( m_iSampleRate );
	wo_buffer_size_frames = kChunkSizeFrames * wo_num_chunks;
	wo_chunk_size = kChunkSizeFrames * kBytesPerFrame;
	wo_buffer_size = wo_buffer_size_frames * kBytesPerFrame;

	WAVEFORMATEX fmt;
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = kChannels;
	fmt.cbSize = 0;
	fmt.nSamplesPerSec = m_iSampleRate;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

	std::vector<UINT> deviceIds;
	if (!PREFSMAN->m_iSoundDevice.Get().empty()) {
		std::vector<RString> portNames;
		split(PREFSMAN->m_iSoundDevice.Get(), ",", portNames, true);
		for (const RString& device : portNames) {
			int id = StringToInt(device, /*pos=*/0, /*base=*/10, /*exceptVal=*/-1);
			if (id != -1) {
				deviceIds.push_back(id);
			}
		}
	}
	deviceIds.push_back(WAVE_MAPPER);  // Fallback to WAVE_MAPPER

	MMRESULT ret = MMSYSERR_ERROR;
	for (UINT id : deviceIds) {
		ret = waveOutOpen( &m_hWaveOut, id, &fmt, (DWORD_PTR) m_hSoundEvent, NULL, CALLBACK_EVENT );
		if (ret == MMSYSERR_NOERROR) {
			break;
		}
	}
	if (ret != MMSYSERR_NOERROR) {
		RString error_string = WaveOutErrorToString( ret ) + "waveOutOpen failed";
		return error_string;
	}


	memset( &(m_aBuffers), 0, sizeof(m_aBuffers) );
	for(int b = 0; b < wo_num_chunks; ++b)
	{
		m_aBuffers[b].dwBufferLength = wo_chunk_size;
		m_aBuffers[b].lpData = new char[wo_chunk_size];
		ret = waveOutPrepareHeader( m_hWaveOut, &m_aBuffers[b], sizeof(m_aBuffers[b]) );
		if( ret != MMSYSERR_NOERROR )
		{
			RString error_string = WaveOutErrorToString( ret ) + "waveOutPrepareHeader failed";
			return error_string;
		}
		m_aBuffers[b].dwFlags |= WHDR_DONE;
	}

	LOG->Info( "WaveOut software mixing at %i hz", m_iSampleRate );

	/* We have a very large writeahead; make sure we have a large enough decode
	 * buffer to recover cleanly from underruns. */
	SetDecodeBufferSize( wo_buffer_size_frames * 3/2 );
	StartDecodeThread();

	MixingThread.SetName( "Mixer thread" );
	MixingThread.Create( MixerThread_start, this );

	b_InitSuccess = true;
	return RString();
}

RageSoundDriver_WaveOut::~RageSoundDriver_WaveOut()
{
	/* Signal the mixing thread to quit. */
	if( MixingThread.IsCreated() )
	{
		m_bShutdown = true;
		SetEvent( m_hSoundEvent );
		LOG->Trace( "Shutting down mixer thread ..." );
		MixingThread.Wait();
		LOG->Trace( "Mixer thread shut down." );
	}

	if( m_hWaveOut != nullptr )
	{
		for( int b = 0; b < wo_num_chunks && m_aBuffers[b].lpData != nullptr; ++b )
		{
			waveOutUnprepareHeader( m_hWaveOut, &m_aBuffers[b], sizeof(m_aBuffers[b]) );
			delete [] m_aBuffers[b].lpData;
		}

		waveOutClose( m_hWaveOut );
	}

	CloseHandle( m_hSoundEvent );
}

float RageSoundDriver_WaveOut::GetPlayLatency() const
{
	/* If we have a 1000-byte buffer, and we fill 100 bytes at a time, we
	 * almost always have between 900 and 1000 bytes filled; on average, 950. */
	return (wo_buffer_size_frames - kChunkSizeFrames/2) * (1.0f / m_iSampleRate);
}

/*
 * (c) 2002-2004 Glenn Maynard
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
