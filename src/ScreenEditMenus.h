#pragma once

#include <string>
#include <vector>

#include "ScreenEdit.h"
#include "ScreenMiniMenu.h"

enum BGChangeChoice {
  song_bganimation,
  song_movie,
  song_bitmap,
  global_bganimation,
  global_movie,
  global_movie_song_group,
  global_movie_song_group_and_genre,
  dynamic_random,
  baked_random,
  none
};

int GetRowsFromAnswers(int choice, const std::vector<int>& answers);
std::string GetOneBakedRandomFile(Song* pSong, bool bTryGenre = true);
TimingData*& ScreenEditClipboardTimingSlot();

extern MenuDef g_EditHelp;
extern MenuDef g_AttackAtTimeMenu;
extern MenuDef g_IndividualAttack;
extern MenuDef g_KeysoundTrack;
extern MenuDef g_MainMenu;
extern MenuDef g_AlterMenu;
extern MenuDef g_AreaMenu;
extern MenuDef g_StepsInformation;
extern MenuDef g_StepsData;
extern MenuDef g_SongInformation;
extern MenuDef g_TimingDataInformation;
extern MenuDef g_TimingDataChangeInformation;
extern MenuDef g_BackgroundChange;
extern MenuDef g_InsertTapAttack;
extern MenuDef g_InsertCourseAttack;
extern MenuDef g_InsertStepAttack;
extern MenuDef g_CourseMode;
