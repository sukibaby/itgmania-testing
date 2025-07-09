#include "global.h"
#include "RageSoundDriver.h"
#include "RageSoundManager.h"
#include "RageLog.h"
#include "RageUtil.h"

#include "arch/arch_default.h"

#include <cstddef>
#include <vector>
#include <cstdint>

namespace
{
	void WarnUserAboutBadSoundDriverEntry()
	{
		std::vector<RString> list = GetDefaultSoundDriverList();
		RString list_string = join( ",", list );
		RString trace = RString(" - Valid sound drivers for your OS are: ") + list_string;
		LOG->Trace("%s", trace.c_str());
		LOG->Trace(" - Make sure the driver entry is spelled correctly, and is supported on your OS.");
	}

	uint32_t TryToGetSampleRate()
	{
		const uint32_t kMin = 44100;   // Minimum supported sample rate (44.1 kHz)
		const uint32_t kMax = 192000; // Maximum supported sample rate (192 kHz)
		uint32_t osReportedValue = HOOKS->DetermineSampleRate();

		// Check if a failure code was returned
		if (osReportedValue == UINT32_MAX || osReportedValue == 0)
		{
			// If the OS couldn't determine the sample rate, use the fallback value
			LOG->Trace("RageSoundDriver: Using fallback sample rate %u", g_FallbackSampleRate.load());
		}

		// Clamp the sample rate to a reasonable range
		if (osReportedValue < kMin || osReportedValue > kMax)
		{
			LOG->Warn("RageSoundDriver: OS reported sample rate %u is out of range", osReportedValue);
			osReportedValue = std::clamp(osReportedValue, kMin, kMax);
		}

		// Update g_FallbackSampleRate if necessary
		const uint32_t currentFallbackSampleRate = g_FallbackSampleRate.load();
		if (osReportedValue != currentFallbackSampleRate)
		{
			g_FallbackSampleRate.store(osReportedValue);
		}

		return osReportedValue;
	}

}  // namespace

DriverList RageSoundDriver::m_pDriverList;

RageSoundDriver *RageSoundDriver::Create( const RString& drivers )
{
	std::vector<RString> driversToTry;
	if(drivers.empty())
	{
		driversToTry = GetDefaultSoundDriverList();
	}
	else
	{
		size_t start = 0;
		size_t end = drivers.find(',');

		while (end != RString::npos)
		{
			driversToTry.emplace_back(drivers.substr(start, end - start));
			start = end + 1;
			end = drivers.find(',', start);
		}

		driversToTry.emplace_back(drivers.substr(start));

		size_t to_try = 0;

		while (to_try < driversToTry.size())
		{
			if (std::find(GetDefaultSoundDriverList().begin(), GetDefaultSoundDriverList().end(), driversToTry[to_try]) == GetDefaultSoundDriverList().end())
			{
				LOG->Warn("Removed unusable sound driver %s", driversToTry[to_try].c_str());
				WarnUserAboutBadSoundDriverEntry();
				driversToTry.erase(driversToTry.begin() + to_try);
			}
			else
			{
				++to_try;
			}
		}
		if(driversToTry.empty())
		{
			driversToTry = GetDefaultSoundDriverList();
		}
	}

	const uint32_t validated_samplerate = TryToGetSampleRate();
	LOG->Info("RageSoundDriver: Sample rate set to %u", validated_samplerate);
	for (RString const &Driver : driversToTry)
	{
		RageDriver *pDriver = m_pDriverList.Create( Driver );
		char const *driverString = Driver.c_str();
		if( pDriver == nullptr )
		{
			LOG->Trace( "Unknown sound driver: %s", driverString );
			continue;
		}

		RageSoundDriver *pRet = dynamic_cast<RageSoundDriver *>( pDriver );
		ASSERT( pRet != nullptr );

		const RString sError = pRet->Init();
		if( sError.empty() )
		{
			LOG->Info( "Sound driver: %s", driverString );
			return pRet;
		}
		LOG->Info( "Couldn't load driver %s: %s", driverString, sError.c_str() );
		RageUtil::SafeDelete( pRet );
	}
	return nullptr;
}

std::vector<RString> RageSoundDriver::GetSoundDriverList()
{
	return GetDefaultSoundDriverList();
}

/*
 * (c) 2002-2006 Glenn Maynard, Steve Checkoway
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
