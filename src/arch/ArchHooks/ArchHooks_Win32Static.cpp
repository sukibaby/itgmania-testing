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

#include <windows.h> // for QueryPerformanceCounter
#include <mmsystem.h> // for timeBeginPeriod
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

/* QueryPerformanceCounter variables below.
 * QueryPerformanceCounter and QueryPerformanceFrequency expect
 * a LARGE_INTEGER, which is a union. These functions store data
 * in the QuadPart of the LARGE_INTEGER, which is a 64-bit integer. */
static std::once_flag s_timer_init_flag;
static LARGE_INTEGER s_frequency;

static void InitTimer() {
	// Set the thread scheduler to let us update every 1ms.
	timeBeginPeriod(1);

	// Retrieve the number of ticks per second.
	QueryPerformanceFrequency(&s_frequency);
}

int64_t ArchHooks::GetSystemTimeInMicroseconds() {
	// Make sure the timer is initialized.
	std::call_once(s_timer_init_flag, InitTimer);

	// Get the current time.
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);

	// Calculate the elapsed time in microseconds.
	return static_cast<int64_t>(
		(current_time.QuadPart * 1000000) / s_frequency.QuadPart);
}

static RString GetMountDir(const RString& dir_of_executable) {
	/* All Windows data goes in the directory one level above the executable. */
	CHECKPOINT_M(ssprintf("DOE \"%s\"", dir_of_executable.c_str()));
	std::vector<RString> parts;
	split(dir_of_executable, "/", parts);
	CHECKPOINT_M(ssprintf("... %i parts", parts.size()));
	ASSERT_M(parts.size() > 1, ssprintf("Strange dir_of_executable: %s", dir_of_executable.c_str()));
	RString dir = join("/", parts.begin(), parts.end() - 1);
	return dir;
}

void MountDirectories(const RString& base_dir) {
	const std::vector<RString> kWinDirectoryStructureITGm = {
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

	for (const RString& dir : kWinDirectoryStructureITGm) {
		FILEMAN->Mount("dir", base_dir + dir, dir);
	}
}

void ArchHooks::MountInitialFilesystems(const RString& dir_of_executable) {
	RString dir = GetMountDir(dir_of_executable);
	FILEMAN->Mount("dirro", dir, "/");

	if (DoesFileExist("/Portable.ini")) {
		MountDirectories(dir);
	}
}

void ArchHooks::MountUserFilesystems(const RString& dir_of_executable) {
	RString app_data_dir = SpecialDirs::GetAppDataDir() + PRODUCT_ID;
	MountDirectories(app_data_dir);
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
		return locale_str; // no dash found, return locale_str as is
	}

	// Fallback to English if the locale cannot be determined.
	MessageBox(
		nullptr, // No owner window
		"Unable to determine system locale. Using English.", // Message text
		"Locale Warning", // Title of the dialog box
		MB_OK | MB_ICONWARNING // OK button with a warning icon
	);
	LOG->Warn("Unable to determine system locale. Using English.");
	return "en";
}

RString ArchHooks::GetPreferredLanguage() {
	return GetSystemLocale();
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
