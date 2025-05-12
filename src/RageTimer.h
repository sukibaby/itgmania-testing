/* RageTimer - Timer services. */

#ifndef RAGE_TIMER_H
#define RAGE_TIMER_H

#include <cstdint>
#include <utility>

class RageTimer
{
public:
	/* We store the seconds and microseconds parts of the time in separate 64-bit integers.
	 * m_time.first refers to the seconds part, and m_time.second refers to the microseconds part. */
	using RageTimerPair = std::pair<int64_t, int64_t>;

	/* Default & parameterized constructors */
	RageTimer() {Touch();}
	RageTimer(int64_t secs, int64_t us) : m_time(secs, us) {}

	/* Getters to protect encapsulation */
	inline int64_t GetSecs() const { return m_time.first; }
	inline int64_t GetUs() const { return m_time.second; }

	/* Time ago this RageTimer represents. */
	float Ago() const noexcept;
	void Touch() noexcept;
	inline bool IsZero() const { return m_time.first == 0 && m_time.second == 0; }
	inline void SetZero() { m_time = { 0, 0 }; }

	/* Time between last call to GetDeltaTime() (Ago() + Touch()): */
	float GetDeltaTime() noexcept;

	static double GetTimeSinceStart() noexcept;	// seconds since the program was started
	static int GetTimeSinceStartSeconds() noexcept; 	// This is used where GetTimeSinceStart would be cast to an int without rounding.
	static uint64_t GetTimeSinceStartMicroseconds() noexcept;

	/* Get a timer representing half of the time ago as this one. */
	RageTimer Half() const noexcept;

	/* Add (or subtract) a duration from a timestamp.  The result is another timestamp. */
	RageTimer operator+( float tm ) const noexcept;
	RageTimer operator-( float tm ) const { return *this + -tm; }
	void operator+=( float tm ) { *this = *this + tm; }
	void operator-=( float tm ) { *this = *this + -tm; }

	/* Find the amount of time between two timestamps.  The result is a duration. */
	float operator-( const RageTimer &rhs ) const noexcept;

	bool operator<( const RageTimer &rhs ) const noexcept;
private:
	RageTimerPair m_time;
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

