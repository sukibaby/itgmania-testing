/* RageTimer - Timer services. */

#ifndef RAGE_TIMER_H
#define RAGE_TIMER_H

#include <chrono>
#include <cstdint>

// Returns the process-wide steady clock start time.
const std::chrono::steady_clock::time_point &RageTimerStartTime();

class RageTimer
{
public:
	/* Initialize the m_secs and m_us values to 0 and then fill them with the current time. */
	RageTimer() : m_time_point(std::chrono::steady_clock::now()) {}
	RageTimer(uint64_t secs, uint64_t us) : m_time_point(RageTimerStartTime() + std::chrono::seconds(secs) + std::chrono::microseconds(us)) {}

	/* Time ago this RageTimer represents. */
	inline float Ago() const
	{
		const auto duration = std::chrono::steady_clock::now() - m_time_point;
		return std::chrono::duration<float>(duration).count();
	}
	void Touch();
	inline bool IsZero() const { return m_time_point == RageTimerStartTime(); }
	inline void SetZero() { m_time_point = RageTimerStartTime(); }

	/* Time between last call to GetDeltaTime() (Ago() + Touch()): */
	inline float GetDeltaTime()
	{
		const auto now = std::chrono::steady_clock::now();
		const auto duration = now - m_time_point;
		m_time_point = now;
		return std::chrono::duration<float>(duration).count();
	}

	static double GetTimeSinceStart();	// seconds since the program was started
	static int GetTimeSinceStartSeconds(); 	// This is used where GetTimeSinceStart would be cast to an int without rounding.
	static uint64_t GetTimeSinceStartMicroseconds();
	
	// Updates the cached frame time. Call this once per frame at the start to avoid repeated system calls.
	static void UpdateFrameTime();
	
	// Returns the cached frame time. Must call UpdateFrameTime() first, typically at frame start.
	static double GetFrameTime();

	/* Get a timer representing half of the time ago as this one. */
	RageTimer Half() const;

	/* Add (or subtract) a duration from a timestamp.  The result is another timestamp. */
	RageTimer operator+( float tm ) const;
	RageTimer operator-( float tm ) const { return *this + -tm; }
	void operator+=( float tm ) { *this = *this + tm; }
	void operator-=( float tm ) { *this = *this + -tm; }

	/* Find the amount of time between two timestamps.  The result is a duration. */
	float operator-( const RageTimer &rhs ) const;

    bool operator<(const RageTimer &rhs) const { return m_time_point < rhs.m_time_point; }

	/* The following is a "time since start" RageTimer. Splitting the seconds and
	 * microseconds values into two integers and combining them later allows for
	 * better precision. Use caution when changing data types, since resolution
	 * mismatch errors are easy to cause when changing things in RageTimer. */
	std::chrono::steady_clock::time_point m_time_point;

private:
	/* Private constructor for internal use - takes a time_point directly. */
	explicit RageTimer(std::chrono::steady_clock::time_point tp) : m_time_point(tp) {}
};

extern const RageTimer RageZeroTimer;

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

