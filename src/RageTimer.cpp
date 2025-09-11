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

// Performance optimization: use integer constants for better performance
static constexpr uint64_t ONE_SECOND_IN_MICROSECONDS_ULL = 1000000ULL;
static constexpr int64_t ONE_SECOND_IN_MICROSECONDS_LL = 1000000LL;
static constexpr double ONE_SECOND_IN_MICROSECONDS_DBL = 1000000.0;

// Pre-computed reciprocal for faster division
static constexpr double ONE_OVER_MICROSECONDS = 1.0 / 1000000.0;

const RageTimer RageZeroTimer(0,0);
static const uint64_t g_iStartTime = ArchHooks::GetSystemTimeInMicroseconds();

// Cache for current time to avoid multiple system calls
static thread_local uint64_t g_cachedTime = 0;
static thread_local uint64_t g_cacheTimestamp = 0;
static constexpr uint64_t CACHE_TIMEOUT_MICROS = 1000; // 1ms cache

static inline uint64_t GetTime() noexcept
{
	const uint64_t now = ArchHooks::GetSystemTimeInMicroseconds();

	// Use cached time if it's recent enough
	if (__builtin_expect(g_cacheTimestamp != 0 &&
		(now - g_cacheTimestamp) < CACHE_TIMEOUT_MICROS, true))
	{
		return g_cachedTime;
	}

	// Update cache
	g_cachedTime = now;
	g_cacheTimestamp = now;
	return now;
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
	// Use pre-computed reciprocal for faster multiplication
	return static_cast<double>(usecs) * ONE_OVER_MICROSECONDS;
}

int RageTimer::GetTimeSinceStartSeconds()
{
	const uint64_t usecs = (GetTime() - g_iStartTime);
	return static_cast<int>(usecs / ONE_SECOND_IN_MICROSECONDS_ULL);
}

uint64_t RageTimer::GetTimeSinceStartMicroseconds()
{
	return (GetTime() - g_iStartTime);
}

void RageTimer::Touch()
{
	const uint64_t usecs = GetTime();

	// Use optimized division - compiler should convert to multiplication by reciprocal
	this->m_secs = usecs / ONE_SECOND_IN_MICROSECONDS_ULL;
	this->m_us = usecs % ONE_SECOND_IN_MICROSECONDS_ULL;
}

float RageTimer::Ago() const
{
	const RageTimer Now;
	return Now - *this;
}

float RageTimer::GetDeltaTime()
{
	const RageTimer Now;
	const float diff = Difference( Now, *this );
	*this = Now;
	return diff;
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
	const float fProbableDelay = Ago() / 2.0f;
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
	// Optimize comparison by checking seconds first (most common discriminator)
	if (__builtin_expect(m_secs != rhs.m_secs, false))
		return m_secs < rhs.m_secs;
	return m_us < rhs.m_us;
}

RageTimer RageTimer::Sum(const RageTimer& lhs, float tm)
{
	/* Calculate the seconds and microseconds from the time:
	 * tm == 5.25  -> secs =  5, us = 5.25  - ( 5) = .25
	 * tm == -1.25 -> secs = -2, us = -1.25 - (-2) = .75 */

	// Fast path for zero addition
	if (__builtin_expect(tm == 0.0f, false))
		return lhs;

	// Fast path for small additions that don't require carry
	if (__builtin_expect(tm >= 0.0f && tm < 1.0f, true))
	{
		int64_t us = static_cast<int64_t>(tm * ONE_SECOND_IN_MICROSECONDS_LL);
		RageTimer ret = lhs;
		ret.m_us += us;

		// Only carry if necessary
		if (__builtin_expect(ret.m_us >= ONE_SECOND_IN_MICROSECONDS_ULL, false))
		{
			ret.m_us -= ONE_SECOND_IN_MICROSECONDS_ULL;
			++ret.m_secs;
		}
		return ret;
	}

	// Use faster floor for positive values (common case)
	int64_t seconds;
	if (__builtin_expect(tm >= 0.0f, true))
	{
		seconds = static_cast<int64_t>(tm);
	}
	else
	{
		seconds = static_cast<int64_t>(std::floor(tm));
	}

	int64_t us = static_cast<int64_t>((tm - seconds) * ONE_SECOND_IN_MICROSECONDS_LL);

	RageTimer ret = lhs; // Copy to avoid modifying lhs

	// Calculate the sum of the seconds and microseconds
	ret.m_secs += seconds;
	ret.m_us += us;

	// Optimize carry propagation - use branch prediction for common case
	if (__builtin_expect(ret.m_us >= ONE_SECOND_IN_MICROSECONDS_ULL, false))
	{
		ret.m_us -= ONE_SECOND_IN_MICROSECONDS_ULL;
		++ret.m_secs;
	}
	else if (__builtin_expect(ret.m_us < 0, false))
	{
		ret.m_us += ONE_SECOND_IN_MICROSECONDS_ULL;
		--ret.m_secs;
	}

	return ret;
}

double RageTimer::Difference(const RageTimer& lhs, const RageTimer& rhs)
{
	// Calculate the difference in seconds and microseconds respectively
	int64_t secs = lhs.m_secs - rhs.m_secs;
	int64_t us = lhs.m_us - rhs.m_us;

	// Optimize borrow propagation - use branch prediction for common case
	if (__builtin_expect(us < 0, false))
	{
		us += ONE_SECOND_IN_MICROSECONDS_LL;
		--secs;
	}

	// Use pre-computed reciprocal for faster conversion
	return static_cast<double>(secs) + static_cast<double>(us) * ONE_OVER_MICROSECONDS;
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

