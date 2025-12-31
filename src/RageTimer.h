/* RageTimer - Timer services. */

#ifndef RAGE_TIMER_H
#define RAGE_TIMER_H

#include <chrono>
#include <cstdint>

class RageTimer
{
public:
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using duration = clock::duration;

	RageTimer(): tp(clock::now()) { }
	explicit RageTimer( time_point t ): tp(t) { }

	// Thin wrapper: public storage to ease interop.
	time_point tp;
};

extern const RageTimer RageZeroTimer;

// ---- Free-function helpers (replacement for old RageTimer methods) ----
inline bool RageTimerIsZero( const RageTimer &t )
{
	return t.tp == RageTimer::time_point{};
}

inline void RageTimerSetZero( RageTimer &t )
{
	t.tp = RageTimer::time_point{};
}

inline void RageTimerTouch( RageTimer &t )
{
	t.tp = RageTimer::clock::now();
}

inline float RageTimerAgo( const RageTimer &t )
{
	if( RageTimerIsZero(t) )
		return 0.0f;
	return std::chrono::duration<float>(RageTimer::clock::now() - t.tp).count();
}

inline float RageTimerGetDeltaTime( RageTimer &t )
{
	const auto now = RageTimer::clock::now();
	if( RageTimerIsZero(t) )
	{
		t.tp = now;
		return 0.0f;
	}
	const float diff = std::chrono::duration<float>(now - t.tp).count();
	t.tp = now;
	return diff;
}

inline RageTimer RageTimerAddSeconds( const RageTimer &t, float seconds )
{
	if( RageTimerIsZero(t) )
		return t;
	const auto delta = std::chrono::duration_cast<RageTimer::duration>(std::chrono::duration<float>(seconds));
	return RageTimer(t.tp + delta);
}

inline void RageTimerAddSecondsInPlace( RageTimer &t, float seconds )
{
	t = RageTimerAddSeconds(t, seconds);
}

inline float RageTimerDiffSeconds( const RageTimer &lhs, const RageTimer &rhs )
{
	if( RageTimerIsZero(lhs) || RageTimerIsZero(rhs) )
		return 0.0f;
	return std::chrono::duration<float>(lhs.tp - rhs.tp).count();
}

// ---- Time since start (replacement for RageTimer::GetTimeSinceStart*) ----
double RageTimerGetTimeSinceStart();
int RageTimerGetTimeSinceStartSeconds();
uint64_t RageTimerGetTimeSinceStartMicroseconds();

inline bool operator<( const RageTimer &lhs, const RageTimer &rhs )
{
	return lhs.tp < rhs.tp;
}

#endif // RAGE_TIMER_H

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

