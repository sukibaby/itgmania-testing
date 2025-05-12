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
#include "RageLog.h"
#include "RageUtil.h"

#include "arch/ArchHooks/ArchHooks.h"

#include <cmath>
#include <cstdint>

///
/// DO NOT CHANGE THIS FILE WITHOUT DOING EXTENSIVE TESTING!
/// Very minor changes here can have effects which can cascade
///  quickly, causing major problems. This code is highly optimized,
///  and a majority of the engine depends on RageTimer working in
///  a very specific and predictable way.
///
/// Too much error will manifest as a drastic shift in the game's
///  sync, and will feel like the global offset suddenly changed.
///
/// Incorrect math here will manifest as a consistent, or linearly-
///  compounding change in the global offset.
///
/// Resolution mismatches or truncations will manifest as a sudden
///  change in the global offset, and will feel like an abrupt drift.
/// 
/// A RageTimer object is, at its core, a std::pair of two int64_t.
/// For the sake of optimization, many functions below will directly
///  access the pair, and the functions below often prefer to construct
///  compatible pairs of int64_t rather than to create temporary RageTimer
///  objects. Badly performing/inefficient code manifests as noise/jitter.
///
/// m_time.first refers to the seconds part of the std::pair,
///  and m_time.second refers to the microseconds part.
/// 

// Intialize important variables and definitions
constexpr uint64_t kUsecsPerSecULL = 1000000ULL;
constexpr int64_t kUsecsPerSecLL = 1000000LL;
constexpr double kUsecsPerSecDouble = 1000000.0;
constexpr double kUsecsToSecRatio = 0.000001;
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
double RageTimer::GetTimeSinceStart()
{
	const uint64_t usecs = (GetTime() - g_iStartTime);
	return static_cast<double>(usecs / kUsecsPerSecDouble);
}

int RageTimer::GetTimeSinceStartSeconds()
{
	const uint64_t usecs = (GetTime() - g_iStartTime);
	return static_cast<int>(usecs / kUsecsPerSecULL);
}

uint64_t RageTimer::GetTimeSinceStartMicroseconds()
{
	return (GetTime() - g_iStartTime);
}

void RageTimer::Touch()
{
	uint64_t usecs = GetTime();

	m_time.first = usecs / kUsecsPerSecULL; // seconds
	m_time.second = usecs % kUsecsPerSecULL; // microseconds
}

// Ago() returns the time since the last call to Touch(), whereas GetDeltaTime()
// returns the time since the last call to GetDeltaTime() but also updates the
// stored time in the RageTimer object. This is useful for tracking elapsed time.
float RageTimer::Ago() const noexcept
{
	RageTimer currentTime;
	int64_t secs = currentTime.GetSecs() - m_time.first;
	int64_t us = currentTime.GetUs() - m_time.second;

	// Adjust the seconds and microseconds if microseconds is greater than or equal to one second
	if (us < 0) {
		us += kUsecsPerSecLL;
		--secs;
	}

	double ret = static_cast<double>(secs) + static_cast<double>(us * kUsecsToSecRatio);
	return static_cast<float>(ret);
}

// GetDeltaTime() will update the stored time in the RageTimer object as well as
// returning the time since the last call to Touch().
float RageTimer::GetDeltaTime() noexcept
{
	RageTimer currentTime;
	int64_t secs = currentTime.GetSecs() - m_time.first;
	int64_t us = currentTime.GetUs() - m_time.second;

	// Adjust the seconds and microseconds if microseconds is greater than or equal to one second
	if (us < 0) {
		us += kUsecsPerSecLL;
		--secs;
	}

	// Update the stored time
	m_time.first = secs;
	m_time.second = us;

	double ret = static_cast<double>(secs) + static_cast<double>(us * kUsecsToSecRatio);
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
RageTimer RageTimer::Half() const
{
	const float fProbableDelay = Ago() / 2;
	return *this + fProbableDelay;
}


RageTimer RageTimer::operator+(float tm) const
{
	return Sum(*this, tm);
}

float RageTimer::operator-(const RageTimer &rhs) const
{
	return Difference(*this, rhs);
}

bool RageTimer::operator<( const RageTimer &rhs ) const
{
	if (m_time.first != rhs.m_time.first) return m_time.first < rhs.m_time.first;
	return m_time.second < rhs.m_time.second;
}

RageTimer RageTimer::Sum(const RageTimer& lhs, float tm)
{
	/* Calculate the seconds and microseconds from the time:
	 * tm == 5.25  -> secs =  5, us = 5.25  - ( 5) = .25
	 * tm == -1.25 -> secs = -2, us = -1.25 - (-2) = .75 */
	int64_t seconds = std::floor(tm);
	int64_t us = static_cast<int64_t>((tm - seconds) * ONE_SECOND_IN_MICROSECONDS_LL);

	// Prevent unnecessarily checking the time
	RageTimer ret(0, 0);

	// Calculate the sum of the seconds and microseconds
	ret.m_time.first = seconds + lhs.m_time.first;
	ret.m_time.second = us + lhs.m_time.second;

	// Adjust the seconds and microseconds if microseconds is greater than or equal to TIMESTAMP_RESOLUTION
	if (ret.m_time.second >= ONE_SECOND_IN_MICROSECONDS_LL)
	{
		ret.m_time.second -= ONE_SECOND_IN_MICROSECONDS_LL;
		++ret.m_time.first;
	}

	return ret;
}

double RageTimer::Difference(const RageTimer& lhs, const RageTimer& rhs)
{
	// Calculate the difference in seconds and microseconds respectively
	int64_t secs = lhs.m_time.first - rhs.m_time.first;
	int64_t us = lhs.m_time.second - rhs.m_time.second;

	// Adjust seconds and microseconds if microseconds is negative
	if ( us < 0 )
	{
		us += ONE_SECOND_IN_MICROSECONDS_LL;
		--secs;
	}

	// Return the difference as a double to preserve the fractional part
	return static_cast<double>(secs) + static_cast<double>(us) / ONE_SECOND_IN_MICROSECONDS_DBL;
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

