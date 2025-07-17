#include "global.h"
#include "ArchHooks.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "archutils/Win32/SpecialDirs.h"
#include "ProductInfo.h"
#include "RageFileManager.h"

#include <cstdint>
#include <vector>
#include <mutex> // for call_once

// for QueryPerformanceCounter
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <winnls.h>
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

static std::once_flag g_timerInitFlag;

/* QueryPerformanceCounter variables below.
 * QueryPerformanceCounter and QueryPerformanceFrequency expect
 * a LARGE_INTEGER, which is a union. These functions store data
 * in the QuadPart of the LARGE_INTEGER, which is a 64-bit integer. */
namespace {
    LARGE_INTEGER g_liFrequency;
}  // namespace

static void InitTimer()
{
	// Set the thread scheduler to let us update every 1ms.
	timeBeginPeriod(1);
	
	// Retrieve the number of ticks per second.
	QueryPerformanceFrequency(&g_liFrequency);
}

int64_t ArchHooks::GetSystemTimeInMicroseconds()
{
	// Make sure the timer is initialized.
	std::call_once(g_timerInitFlag, InitTimer);
	
	// Get the current time.
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);

	// Calculate the elapsed time in microseconds.
	return (current_time.QuadPart * 1000000LL) / g_liFrequency.QuadPart;
}

static RString GetMountDir( const RString &sDirOfExecutable )
{
	/* All Windows data goes in the directory one level above the executable. */
	CHECKPOINT_M( ssprintf( "DOE \"%s\"", sDirOfExecutable.c_str()) );
	std::vector<RString> asParts;
	split( sDirOfExecutable, "/", asParts );
	CHECKPOINT_M( ssprintf( "... %i asParts", asParts.size()) );
	ASSERT_M( asParts.size() > 1, ssprintf("Strange sDirOfExecutable: %s", sDirOfExecutable.c_str()) );
	RString sDir = join( "/", asParts.begin(), asParts.end()-1 );
	return sDir;
}

void MountDirectories(const RString& baseDir) {
	const std::vector<RString> winDirectoryStructureITGm = {
		"/Announcers",
		"/BGAnimations",
		"/BackgroundEffects",
		"/BackgroundTransitions",
		"/Cache",
		"/CDTitles",
		"/Characters",
		"/Courses",
		"/Downloads",
		"/Logs",
		"/NoteSkins",
		"/Packages",
		"/Save",
		"/Screenshots",
		"/Songs",
		"/RandomMovies",
		"/Themes"
	};

	for (const RString& dir : winDirectoryStructureITGm) {
		FILEMAN->Mount("dir", baseDir + dir, dir);
	}
}

void ArchHooks::MountInitialFilesystems(const RString& sDirOfExecutable) {
	RString sDir = GetMountDir(sDirOfExecutable);
	FILEMAN->Mount("dirro", sDir, "/");

	if (DoesFileExist("/Portable.ini")) {
		MountDirectories(sDir);
	}
}

void ArchHooks::MountUserFilesystems(const RString& sDirOfExecutable) {
	RString sAppDataDir = SpecialDirs::GetAppDataDir() + PRODUCT_ID;
	MountDirectories(sAppDataDir);
}

RString ArchHooks::GetPreferredLanguage() {
	// First, we need to get the preferred UI languages from the system.
	// However, since they are returned as wide strings, we need to convert them
	// to UTF-8. We also need to truncate the language code to the part before
	// the first hyphen, unless it is a Chinese language code, because we
	// want to keep those intact as "zh-Hans" or "zh-Hant".
	wchar_t lang_buffer[128] = {0};
	ULONG num_languages = 0;
	DWORD lang_buffer_size = static_cast<DWORD>(sizeof(lang_buffer) / sizeof(wchar_t));
	if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &num_languages, lang_buffer, &lang_buffer_size)) {
		LOG->Warn("GetUserPreferredUILanguages failed, defaulting to 'en'");
		return "en";
	} // GetUserPreferredUILanguages failed

	// Get the first entry.
	const wchar_t* first_lang = lang_buffer;
	if (first_lang[0] == L'\0') {
		LOG->Warn("No preferred UI language found, defaulting to 'en'");
		return "en";
	} // highly unlikely, but still log if this happens

	if (first_lang[0] == L'z' && first_lang[1] == L'h' && first_lang[2] == L'-') {
		if (wcsstr(first_lang, L"Hans")) {
			return "zh-Hans";
		} else if (wcsstr(first_lang, L"Hant")) {
			return "zh-Hant";
		}
		return "zh";
	} // If the first language is Chinese, return "zh-Hans" or "zh-Hant" if possible

	// For all languages besides Chinese, store the first two characters and null terminate.
	wchar_t iso_code[3] = {first_lang[0], first_lang[1], L'\0'};
	char utf8iso_code[3] = {0};
	if (WideCharToMultiByte(CP_UTF8, 0, iso_code, -1, utf8iso_code, sizeof(utf8iso_code), nullptr, nullptr) == 0) {
		LOG->Warn("WideCharToMultiByte failed, defaulting to 'en'");
		return "en";
	} // WideCharToMultiByte failed

	return utf8iso_code;
}


/*
 * (c) 2003-2004 Chris Danford
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
