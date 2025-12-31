

#include "global.h"

#include "RageTimer.h"

#include "LuaManager.h"

#include <chrono>

static const RageTimer::time_point g_StartTime = RageTimer::clock::now();

const RageTimer RageZeroTimer(RageTimer::time_point{});

/* The accuracy of RageTimer::GetTimeSinceStart() is directly tied to the
 * stability of the clock sync. Maintaining precision here is crucial. Too
 * much error here will manifest as a drastic shift in the game's sync, and
 * will feel like the global offset suddenly changed. Incorrect math here will
 * manifest as a _consistent_ sync offset in game. Resolution mismatches or
 * values truncated or rounded when they shouldn't be can cause errors when
 * this is calculated and manifest as a _sudden_ drift of sync. Use caution
 * and do thorough testing if you change anything here. -sukibaby */
double RageTimerGetTimeSinceStart()
{
	const auto elapsed = RageTimer::clock::now() - g_StartTime;
	return std::chrono::duration<double>(elapsed).count();
}

int RageTimerGetTimeSinceStartSeconds()
{
	const auto elapsed = RageTimer::clock::now() - g_StartTime;
	return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
}

uint64_t RageTimerGetTimeSinceStartMicroseconds()
{
	const auto elapsed = RageTimer::clock::now() - g_StartTime;
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

LuaFunction(GetTimeSinceStart, RageTimerGetTimeSinceStart())

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

