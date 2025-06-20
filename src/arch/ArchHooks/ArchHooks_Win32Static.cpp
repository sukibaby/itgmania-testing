#include "global.h"
#include "ArchHooks.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "archutils/Win32/SpecialDirs.h"
#include "ProductInfo.h"
#include "RageFileManager.h"

#include <cstdint>
#include <vector>
#include <mutex>

#include <windows.h>
#include <mmsystem.h>
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace {
	std::once_flag g_timerInitFlag;

	// QueryPerformanceCounter and QueryPerformanceFrequency expect
	// a LARGE_INTEGER, which is a union. These functions store data
	// in the QuadPart of the LARGE_INTEGER, which is a 64-bit integer.
	LARGE_INTEGER g_liFrequency;  // constant after InitTimer() is called
	LARGE_INTEGER g_liCurrentTime;  // TODO: move within GetSystemTimeInMicroseconds()

	void InitTimer()
	{
		// Set the thread scheduler to let us update every 1ms.
		timeBeginPeriod(1);

		// Retrieve the number of ticks per second.
		QueryPerformanceFrequency(&g_liFrequency);
	}

	RString GetMountDir(const RString &dir_of_executable)
	{
		// All Windows data goes in the directory one level above the executable.
		CHECKPOINT_M(ssprintf("DOE \"%s\"", dir_of_executable.c_str()));
		std::vector<RString> parts;
		split(dir_of_executable, "/", parts);
		CHECKPOINT_M(ssprintf("... %i parts", parts.size()));
		ASSERT_M(parts.size() > 1, ssprintf("Strange dir_of_executable: %s", dir_of_executable.c_str()));
		RString dir = join("/", parts.begin(), parts.end() - 1);
		return dir;
	}

	RString LangIdToString()
	{
		LANGID lang_id = GetUserDefaultLangID();
		switch (PRIMARYLANGID(lang_id))
		{
		case LANG_ARABIC: return "ar";
		case LANG_BULGARIAN: return "bg";
		case LANG_CATALAN: return "ca";
		case LANG_CHINESE:
			switch (SUBLANGID(lang_id))
			{
			case SUBLANG_CHINESE_TRADITIONAL:
			case SUBLANG_CHINESE_HONGKONG:
			case SUBLANG_CHINESE_MACAU:
				return "zh-Hant";
			case SUBLANG_CHINESE_SIMPLIFIED:
			case SUBLANG_CHINESE_SINGAPORE:
				return "zh-Hans";
			default:
				LOG->Warn("Unknown Chinese sublanguage. Using zh-Hans.");
				return "zh-Hans";
			}
		case LANG_CZECH: return "cs";
		case LANG_DANISH: return "da";
		case LANG_GERMAN: return "de";
		case LANG_GREEK: return "el";
		case LANG_SPANISH: return "es";
		case LANG_FINNISH: return "fi";
		case LANG_FRENCH: return "fr";
		case LANG_HEBREW: return "iw";
		case LANG_HUNGARIAN: return "hu";
		case LANG_ICELANDIC: return "is";
		case LANG_ITALIAN: return "it";
		case LANG_JAPANESE: return "ja";
		case LANG_KOREAN: return "ko";
		case LANG_DUTCH: return "nl";
		case LANG_NORWEGIAN: return "no";
		case LANG_POLISH: return "pl";
		case LANG_PORTUGUESE: return "pt";
		case LANG_ROMANIAN: return "ro";
		case LANG_RUSSIAN: return "ru";
		case LANG_CROATIAN: return "hr";
		case LANG_SLOVAK: return "sk";
		case LANG_ALBANIAN: return "sq";
		case LANG_SWEDISH: return "sv";
		case LANG_THAI: return "th";
		case LANG_TURKISH: return "tr";
		case LANG_URDU: return "ur";
		case LANG_INDONESIAN: return "in";
		case LANG_UKRAINIAN: return "uk";
		case LANG_SLOVENIAN: return "sl";
		case LANG_ESTONIAN: return "et";
		case LANG_LATVIAN: return "lv";
		case LANG_LITHUANIAN: return "lt";
		case LANG_VIETNAMESE: return "vi";
		case LANG_ARMENIAN: return "hy";
		case LANG_BASQUE: return "eu";
		case LANG_MACEDONIAN: return "mk";
		case LANG_AFRIKAANS: return "af";
		case LANG_GEORGIAN: return "ka";
		case LANG_FAEROESE: return "fo";
		case LANG_HINDI: return "hi";
		case LANG_MALAY: return "ms";
		case LANG_KAZAK: return "kk";
		case LANG_SWAHILI: return "sw";
		case LANG_UZBEK: return "uz";
		case LANG_TATAR: return "tt";
		case LANG_PUNJABI: return "pa";
		case LANG_GUJARATI: return "gu";
		case LANG_TAMIL: return "ta";
		case LANG_KANNADA: return "kn";
		case LANG_MARATHI: return "mr";
		case LANG_SANSKRIT: return "sa";
		default:
			LOG->Warn("Unable to determine system language. Using English.");
			[[fallthrough]];
		case LANG_ENGLISH: return "en";
		}
	}
}  // namespace

int64_t ArchHooks::GetSystemTimeInMicroseconds()
{
	// Make sure the timer is initialized.
	std::call_once(g_timerInitFlag, InitTimer);

	// Get the current time.
	QueryPerformanceCounter(&g_liCurrentTime);

	// Calculate the elapsed time in microseconds.
	return (g_liCurrentTime.QuadPart * 1000000LL) / g_liFrequency.QuadPart;
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
		"/Themes"};

	for (const RString& dir : winDirectoryStructureITGm) {
		FILEMAN->Mount("dir", baseDir + dir, dir);
	}
}

void ArchHooks::MountInitialFilesystems(const RString &dir_of_executable)
{
	RString dir = GetMountDir(dir_of_executable);
	FILEMAN->Mount("dirro", dir, "/");

	if (DoesFileExist("/Portable.ini"))
	{
		MountDirectories(dir);
	}
}

void ArchHooks::MountUserFilesystems(const RString &dir_of_executable)
{
	RString appdata_dir = SpecialDirs::GetAppDataDir() + PRODUCT_ID;
	MountDirectories(appdata_dir);
}

RString ArchHooks::GetPreferredLanguage()
{
	return LangIdToString();
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
