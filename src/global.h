#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.hpp"

#if defined(_MSC_VER)
#pragma once
#endif

//
// +---------------------------------+
// | Standard macros for integer     |
// | limits and constants            |
// +---------------------------------+
//
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

//
// +-----------------------------+
// | Platform-specific setup     |
// +-----------------------------+
//
#if defined(_WIN32)
#include "archutils/Win32/arch_setup.h"
#elif defined(PBBUILD)
#include "archutils/Darwin/arch_setup.h"
#elif defined(UNIX)
#include "archutils/Unix/arch_setup.h"
#endif

//
// +-----------------------------+
// | Standard library includes   |
// +-----------------------------+
//
#include <algorithm>
#include <utility>

//
// +-------------------------------------+
// | Branch optimization hints           |
// | (likely, unlikely)                  |
// +-------------------------------------+
//
#if defined(__GNUC__)
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

//
// +---------------------------------+
// | Ensure ASSERT is not predefined |
// +---------------------------------+
//
#ifdef ASSERT
#undef ASSERT
#endif

//
// +-----------------------------+
// | RString                      |
// | TODO: Move to std::string.   |
// +-----------------------------+
//
#include "StdString.h"
typedef StdString::CStdString RString;

//
// +-----------------------------+
// | Threading / Checkpoints     |
// +-----------------------------+
//
#include "RageThreads.h"

//
// +-----------------------------+
// | Crash handling              |
// +-----------------------------+
//
[[noreturn]] void sm_crash(const RString& reason);
[[noreturn]] void sm_crash(const char* reason = "Internal error");

//
// +-----------------------------+
// | Assertion macros            |
// +-----------------------------+
//
#define FAIL_M(MESSAGE) do { CHECKPOINT_M(MESSAGE); sm_crash(MESSAGE); } while(0)
#define ASSERT_M(COND, MESSAGE) do { if(unlikely(!(COND))) { FAIL_M(MESSAGE); } } while(0)

#if !defined(CO_EXIST_WITH_MFC)
#define ASSERT(COND) ASSERT_M((COND), "Assertion '" #COND "' failed")
#endif

#define DEFAULT_FAIL(i) default: FAIL_M(ssprintf("%s = %i", #i, (i)))

//
// +-----------------------------+
// | Warning and tracing         |
// +-----------------------------+
//
void ShowWarningOrTrace(const char* file, int line, const RString& message, bool bWarning);
void ShowWarningOrTrace(const char* file, int line, const char* message, bool bWarning);
#define WARN(MESSAGE) (ShowWarningOrTrace(__FILE__, __LINE__, MESSAGE, true))

#if !defined(CO_EXIST_WITH_MFC)
#define TRACE(MESSAGE) (ShowWarningOrTrace(__FILE__, __LINE__, MESSAGE, false))
#endif

//
// +-----------------------------+
// | Debug assertions            |
// +-----------------------------+
//
#ifdef DEBUG
#define DEBUG_ASSERT_M(COND, MESSAGE) if(unlikely(!(COND))) WARN(MESSAGE)
#define DEBUG_ASSERT(COND) DEBUG_ASSERT_M(COND, "Debug assert failed")
#else
#define DEBUG_ASSERT(x)
#define DEBUG_ASSERT_M(x, y)
#endif

//
// +-------------------------------------+
// | Unique name generation for macros   |
// +-------------------------------------+
//
#define SM_UNIQUE_NAME3(x, line) x##line
#define SM_UNIQUE_NAME2(x, line) SM_UNIQUE_NAME3(x, line)
#define SM_UNIQUE_NAME(x) SM_UNIQUE_NAME2(x, __LINE__)

//
// +-----------------------------+
// | Exceptions                  |
// +-----------------------------+
//
#include "RageException.h"

//
// +-------------------------------------------------+
// | Utility template: Call a function every N frames |
// | TODO: Move to a more appropriate location.       |
// +-------------------------------------------------+
//
#include <utility>
template <typename Func, typename... Args>
void CallEveryNFrames(int n, Func&& f, Args&&... args) {
    static int counter = 0;
    ++counter;
    if (counter == n) {
        counter = 0;
        std::forward<Func>(f)(std::forward<Args>(args)...);
    }
}

#endif

/**
 * @file
 * @author Chris Danford, Glenn Maynard (c) 2001-2004
 * @section LICENSE
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
