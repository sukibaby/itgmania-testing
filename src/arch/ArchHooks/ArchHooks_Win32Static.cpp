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

#include <string>

// for QueryPerformanceCounter
#include <windows.h>
#include <mmsystem.h> // for timeBeginPeriod
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

/* QueryPerformanceCounter variables below.
 * QueryPerformanceCounter and QueryPerformanceFrequency expect
 * a LARGE_INTEGER, which is a union. These functions store data
 * in the QuadPart of the LARGE_INTEGER, which is a 64-bit integer. */
static std::once_flag g_timerInitFlag;
static LARGE_INTEGER g_liFrequency;

static void InitTimer() {
  // Set the thread scheduler to let us update every 1ms.
  timeBeginPeriod(1);

  // Retrieve the number of ticks per second.
  QueryPerformanceFrequency(&g_liFrequency);
}

int64_t ArchHooks::GetSystemTimeInMicroseconds() {
  // Make sure the timer is initialized.
  std::call_once(g_timerInitFlag, InitTimer);

  // Get the current time.
  LARGE_INTEGER liCurrentTime;
  QueryPerformanceCounter(&liCurrentTime);

  // Calculate the elapsed time in microseconds.
  return static_cast<int64_t>(
      (liCurrentTime.QuadPart * 1000000) / g_liFrequency.QuadPart);
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

static RString GetSystemLocale() {
  wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = {0};

  // Retrieve the system's default locale name
  if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
    char locale_name_utf8[LOCALE_NAME_MAX_LENGTH] = {0};
    WideCharToMultiByte(
        CP_UTF8, 0, locale_name, -1, locale_name_utf8, LOCALE_NAME_MAX_LENGTH,
        nullptr, nullptr);

    // Extract the language portion (e.g., "en" from "en-US")
    std::string locale_str(locale_name_utf8);
    size_t dash_pos = locale_str.find('-');
    if (dash_pos != std::string::npos) {
      return locale_str.substr(0, dash_pos);
    }
    return locale_str;  // no dash found, return locale_str as is
  }

  // Fallback to English if the locale cannot be determined.
      // Display a dialog box with the warning and log a warning.
    MessageBox(
        nullptr, // No owner window
        "Unable to determine system locale. Using English.", // Message text
        "Locale Warning", // Title of the dialog box
        MB_OK | MB_ICONWARNING // OK button with a warning icon
    );
  LOG->Warn("Unable to determine system locale. Using English.");
  return "en";
}

RString ArchHooks::GetPreferredLanguage() { return GetSystemLocale(); }

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
