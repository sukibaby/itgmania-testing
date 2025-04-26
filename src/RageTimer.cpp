/*
 * This can be used in two ways: as a timestamp or as a timer.
 *
 * As a timer,
 * RageTimer Timer;
 * for(;;) {
 *   printf( "Will be approximately: %f", Timer.PeekDeltaTime()) ;
 *   float fDeltaTime = Timer.GetDeltaTime();
 * }
 *
 * or as a timestamp:
 * void foo( RageTimer &timestamp ) {
 *     if( timestamp.IsZero() )
 *         printf( "The timestamp isn't set." );
 *     else
 *         printf( "The timestamp happened %f ago", timestamp.Ago() );
 *     timestamp.Touch();
 *     printf( "Near zero: %f", timestamp.Age() );
 * }
 */

#include "global.h"

#include "RageTimer.h"

#include "arch/ArchHooks/ArchHooks.h"

#include <cmath>
#include <cstdint>

// Intialize important variables and definitions
constexpr uint64_t ONE_SECOND_IN_MICROSECONDS_ULL = 1000000ULL;
constexpr int64_t ONE_SECOND_IN_MICROSECONDS_LL = 1000000LL;
constexpr double ONE_SECOND_IN_MICROSECONDS_DBL = 1000000.0;
const RageTimer RageZeroTimer(0,0);
static const uint64_t g_iStartTime = ArchHooks::GetSystemTimeInMicroseconds();

static uint64_t GetTime() noexcept
{
	return ArchHooks::GetSystemTimeInMicroseconds();
}

/* The accuracy of RageTimer::GetTimeSinceStart() is directly tied to the
 * stability of the clock sync. Maintaining precision here is crucial. Too
 * much error here will manifest as a drastic shift in the game's sync, and
 * will feel like the global offset suddenly changed. Incorrect math here will
 * manifest as a _consistent_ sync offset in game. Resolution mismatches or
 * values truncated or rounded when they shouldn't be can cause errors when
 * this is calculated and manifest as a _sudden_ drift of sync. Use caution
 * and do thorough testing if you change anything here. -sukibaby */
double RageTimer::GetTimeSinceStart() noexcept
{
	const uint64_t usecs = (GetTime() - g_iStartTime);
	return static_cast<double>(usecs / ONE_SECOND_IN_MICROSECONDS_DBL);
}

int RageTimer::GetTimeSinceStartSeconds() noexcept
{
	const uint64_t usecs = (GetTime() - g_iStartTime);
	return static_cast<int>(usecs / ONE_SECOND_IN_MICROSECONDS_ULL);
}

uint64_t RageTimer::GetTimeSinceStartMicroseconds() noexcept
{
	return (GetTime() - g_iStartTime);
}

void RageTimer::Touch() noexcept
{
	uint64_t usecs = GetTime();

	m_time.first = usecs / ONE_SECOND_IN_MICROSECONDS_ULL; // m_secs
	m_time.second = usecs % ONE_SECOND_IN_MICROSECONDS_ULL; // m_us
}

// Avoid making a temporary RageTimer when possible. Ago() and GetDeltaTime()
// are called frequently & in tight loops.  This is a performance optimization.
float RageTimer::Ago() const noexcept
{
	uint64_t usecs = GetTime();
	int64_t currentSecs = usecs / ONE_SECOND_IN_MICROSECONDS_ULL;
	int64_t currentUs = usecs % ONE_SECOND_IN_MICROSECONDS_ULL;

	// Calculate the difference in seconds and microseconds
	int64_t secs = currentSecs - m_time.first;
	int64_t us = currentUs - m_time.second;

	// Adjust for negative microseconds
	if (us < 0)
	{
		us += ONE_SECOND_IN_MICROSECONDS_LL;
		--secs;
	}

	// Return the difference as a float, but calculate
	// it as a double to preserve the fractional part.
	double ret = static_cast<double>(secs) + static_cast<double>(us) / ONE_SECOND_IN_MICROSECONDS_DBL;
	return static_cast<float>(ret);
}

float RageTimer::GetDeltaTime() noexcept
{
	uint64_t usecs = GetTime();
	int64_t currentSecs = usecs / ONE_SECOND_IN_MICROSECONDS_ULL;
	int64_t currentUs = usecs % ONE_SECOND_IN_MICROSECONDS_ULL;

	// Calculate the difference in seconds and microseconds
	int64_t secs = currentSecs - m_time.first;
	int64_t us = currentUs - m_time.second;

	// Adjust for negative microseconds
	if (us < 0)
	{
		us += ONE_SECOND_IN_MICROSECONDS_LL;
		--secs;
	}

	// Update the stored time
	m_time.first = currentSecs;
	m_time.second = currentUs;

	// Return the difference as a float, but calculate
	// it as a double to preserve the fractional part.
	double ret = static_cast<double>(secs) + static_cast<double>(us) / ONE_SECOND_IN_MICROSECONDS_DBL;
	return static_cast<float>(ret);
}

/* Get a timer representing half of the time ago as this one.  This is	
 * useful for averaging time.  For example,	
 *	
 * RageTimer tm;	
 * ... do stuff ...	
 * RageTimer AverageTime = tm.Half();	
 * printf( "Something happened approximately %f seconds ago.\n", tm.Ago() ); 
 * Note this has been reverted to the original SM3.95 function. */
RageTimer RageTimer::Half() const noexcept
{
	const float fProbableDelay = Ago() / 2;
	return *this + fProbableDelay;
}

// Calculate the seconds and microseconds from the time:
// tm == 5.25  -> secs =  5, us = 5.25  - ( 5) = .25
// tm == -1.25 -> secs = -2, us = -1.25 - (-2) = .75
RageTimer RageTimer::operator+(float tm) const noexcept
{
	int64_t seconds = std::floor(tm);
	int64_t us = static_cast<int64_t>((tm - seconds) * ONE_SECOND_IN_MICROSECONDS_LL);

	// Avoid creating a RageTimer until we have the final result
	int64_t newSecs = m_time.first + seconds;
	int64_t newUs = m_time.second + us;

	// Adjust the seconds and microseconds if microseconds is greater than or equal to one second
	if (newUs >= ONE_SECOND_IN_MICROSECONDS_LL)
	{
		newUs -= ONE_SECOND_IN_MICROSECONDS_LL;
		++newSecs;
	}

	return RageTimer(newSecs, newUs);
}

// Calculate the difference in seconds and microseconds respectively
// and adjust the seconds and microseconds if microseconds is negative
float RageTimer::operator-(const RageTimer &rhs) const noexcept
{
	int64_t secs = m_time.first - rhs.m_time.first; // m_secs
	int64_t us = m_time.second - rhs.m_time.second; // m_us

	if (us < 0)
	{
		us += ONE_SECOND_IN_MICROSECONDS_LL;
		--secs;
	}

	double ret = static_cast<double>(secs) + static_cast<double>(us) / ONE_SECOND_IN_MICROSECONDS_DBL;
	return static_cast<float>(ret);
}

bool RageTimer::operator<( const RageTimer &rhs ) const noexcept
{
	if (m_time.first != rhs.m_time.first) {
		return m_time.first < rhs.m_time.first;
	}
	return m_time.second < rhs.m_time.second;
}

#include "LuaManager.h"
LuaFunction(GetTimeSinceStart, RageTimer::GetTimeSinceStart())

/*
 * Copyright (c) 2001-2003 Chris Danford, Glenn Maynard
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

