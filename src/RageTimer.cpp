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
#include <chrono>

const std::chrono::steady_clock::time_point &RageTimerStartTime()
{
    static const auto start = std::chrono::steady_clock::now();
    return start;
}

const RageTimer RageZeroTimer(0,0);

static inline std::chrono::steady_clock::time_point GetTime() noexcept
{
    return std::chrono::steady_clock::now();
}

// Returns the time in seconds since the program started, as a double.
// Therefore 10000.0 would mean the program has been running for 164 minutes and 40 seconds.
double RageTimer::GetTimeSinceStart()
{
    const auto now = GetTime();
    const auto duration = now - RageTimerStartTime();
    return std::chrono::duration<double>(duration).count();
}

// Returns the time in seconds since the program started, as an integer.
// Therefore 10000 would mean the program has been running for 164 minutes and 40 seconds.
int RageTimer::GetTimeSinceStartSeconds()
{
    const auto now = GetTime();
    const auto duration = now - RageTimerStartTime();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
}

// Returns the time in microseconds since the program started, as an integer.
// Therefore 10000000 would mean the program has been running for 164 minutes and 40 seconds.
uint64_t RageTimer::GetTimeSinceStartMicroseconds()
{
    const auto now = GetTime();
    const auto duration = now - RageTimerStartTime();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

void RageTimer::Touch()
{
    m_time_point = std::chrono::steady_clock::now();
}


RageTimer RageTimer::Half() const
{
	return RageTimer(m_time_point + (std::chrono::steady_clock::now() - m_time_point) / 2);
}


RageTimer RageTimer::operator+(float tm) const
{
	/* Add a floating-point duration to a RageTimer.
	 * tm == 5.25  -> add 5.25 seconds
	 * tm == -1.25 -> subtract 1.25 seconds */
	const auto duration_to_add = std::chrono::duration<float>(tm);
	const auto new_tp = std::chrono::time_point_cast<std::chrono::steady_clock::duration>(m_time_point + duration_to_add);
	return RageTimer(new_tp);
}

float RageTimer::operator-(const RageTimer &rhs) const
{
	// Calculate the difference in time between two RageTimers
	const auto duration = m_time_point - rhs.m_time_point;
	return std::chrono::duration<float>(duration).count();
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

