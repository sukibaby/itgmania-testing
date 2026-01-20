#include "global.h"
#include "RageException.h"
#include "RageLog.h"
#include "RageThreads.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#if defined(_WIN32) && defined(DEBUG)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(MACOSX)
#include "archutils/Darwin/Crash.h"
using CrashHandler::IsDebuggerPresent;
using CrashHandler::DebugBreak;
#endif

static std::atomic<uint64_t> g_HandlerThreadID{ RageThread::GetInvalidThreadID() };
static std::function<void(const std::string &sError)> g_CleanupHandler;

static std::string VFormat( const char *fmt, va_list args )
{
	if( fmt == nullptr )
		return {};

	va_list args_copy;
	va_copy( args_copy, args );
	const int size = std::vsnprintf( nullptr, 0, fmt, args_copy );
	va_end( args_copy );

	if( size <= 0 )
		return {};

	std::string buffer;
	buffer.resize( static_cast<size_t>(size) + 1 );
	std::vsnprintf( buffer.data(), buffer.size(), fmt, args );
	buffer.pop_back();
	return buffer;
}

void RageException::SetCleanupHandler( std::function<void(const std::string &sError)> handler ) noexcept
{
	g_HandlerThreadID.store( RageThread::GetCurrentThreadID(), std::memory_order_relaxed );
	g_CleanupHandler = std::move( handler );
}

/* This is no longer actually implemented by throwing an exception, but it acts
 * the same way to code in practice. */
void RageException::Throw( const char *sFmt, ... )
{
	va_list	va;
	va_start( va, sFmt );
	std::string error = VFormat( sFmt, va );
	va_end( va );

	std::string msg;
	msg.reserve( error.size() + 128 );
	msg += "\n";
	msg += "//////////////////////////////////////////////////////\n";
	msg += "Exception: ";
	msg += error;
	msg += "\n";
	msg += "//////////////////////////////////////////////////////\n";
	if( LOG )
	{
		LOG->Trace( "%s", msg.c_str() );
		LOG->Flush();
	}
	else
	{
		std::fputs( msg.c_str(), stdout );
		std::fflush( stdout );
	}

#if (defined(WINDOWS) && defined(DEBUG)) || defined(_XDBG) || defined(MACOSX)
	if( IsDebuggerPresent() )
		DebugBreak();
#endif

	const uint64_t handler_thread_id = g_HandlerThreadID.load( std::memory_order_relaxed );
	const std::string assert_msg = "RageException::Throw() on another thread: " + error;
	ASSERT_M( handler_thread_id == RageThread::GetInvalidThreadID() || handler_thread_id == RageThread::GetCurrentThreadID(),
		  assert_msg.c_str() );
	if( g_CleanupHandler != nullptr )
		g_CleanupHandler( error );

	std::abort();
}

/*
 * Copyright (c) 2001-2004 Chris Danford
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
