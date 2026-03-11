#include "ScreenEditMenu.h"

#include <string>

#include "ActorUtil.h"
#include "BackgroundUtil.h"
#include "Difficulty.h"
#include "EditMenu.h"
#include "GameConstantsAndTypes.h"
#include "GameManager.h"
#include "GameSoundManager.h"
#include "GameState.h"
#include "LocalizedString.h"
#include "PlayerNumber.h"
#include "RageFile.h"
#include "RageFileManager.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageUtil/RandomNumbers.h"
#include "Screen.h"
#include "ScreenManager.h"
#include "ScreenMessage.h"
#include "ScreenPrompt.h"
#include "ScreenTextEntry.h"
#include "ScreenWithMenuElements.h"
#include "Song.h"
#include "SongManager.h"
#include "SongUtil.h"
#include "Steps.h"
#include "ThemeManager.h"
#include "global.h"

static const std::string TEMP_FILE_NAME = "--temp--";

#define EXPLANATION_TEXT(row) \
  THEME->GetString(m_sName, "Explanation" + EditMenuRowToString(row))
#define EDIT_MENU_TYPE THEME->GetMetric(m_sName, "EditMenuType")

AutoScreenMessage(SM_RefreshSelector);
AutoScreenMessage(SM_BackFromEditDescription);

REGISTER_SCREEN_CLASS(ScreenEditMenu);

void ScreenEditMenu::Init() {
  // HACK: Disable any style set by the editor.
  GAMESTATE->SetCurrentStyle(nullptr, PLAYER_INVALID);

  // Enable all players.
  FOREACH_PlayerNumber(pn) GAMESTATE->JoinPlayer(pn);

  // Edit mode DOES NOT WORK if the master player is not player 1.  The same
  // is true of various parts of this poorly designed screen. -Kyz
  if (GAMESTATE->GetMasterPlayerNumber() != PLAYER_1) {
    LOG->Warn(
        "Master player number was not player 1, forcing it to player 1 so that "
        "edit mode will work.  If playing in edit mode doesn't work, this "
        "might be related.");
    GAMESTATE->SetMasterPlayerNumber(PLAYER_1);
  }

  ScreenWithMenuElements::Init();

  m_Selector.SetName("EditMenu");
  m_Selector.Load(EDIT_MENU_TYPE);
  m_Selector.SetXY(0, 0);
  this->AddChild(&m_Selector);

  m_textExplanation.SetName("Explanation");
  m_textExplanation.LoadFromFont(THEME->GetPathF(m_sName, "explanation"));
  LOAD_ALL_COMMANDS_AND_SET_XY(m_textExplanation);
  RefreshExplanationText();
  this->AddChild(&m_textExplanation);

  m_textNumStepsLoadedFromProfile.SetName("NumStepsLoadedFromProfile");
  m_textNumStepsLoadedFromProfile.LoadFromFont(
      THEME->GetPathF(m_sName, "NumStepsLoadedFromProfile"));
  LOAD_ALL_COMMANDS_AND_SET_XY_AND_ON_COMMAND(m_textNumStepsLoadedFromProfile);
  RefreshNumStepsLoadedFromProfile();
  this->AddChild(&m_textNumStepsLoadedFromProfile);
  if (!m_Selector.SafeToUse()) {
    m_NoSongsMessage.SetName("NoSongsMessage");
    m_NoSongsMessage.LoadFromFont(THEME->GetPathF(m_sName, "NoSongsMessage"));
    LOAD_ALL_COMMANDS_AND_SET_XY_AND_ON_COMMAND(m_NoSongsMessage);
    AddChild(&m_NoSongsMessage);
    m_Selector.SetVisible(false);
    m_textExplanation.SetVisible(false);
    m_textNumStepsLoadedFromProfile.SetVisible(false);
  }
}

void ScreenEditMenu::HandleScreenMessage(const ScreenMessage SM) {
  if (SM == SM_RefreshSelector) {
    m_Selector.RefreshAll();
    RefreshNumStepsLoadedFromProfile();
  } else if (
      SM == SM_Success &&
      m_Selector.GetSelectedAction() == EditMenuAction_Delete) {
    LOG->Trace("Delete successful; deleting steps from memory");

    Song* pSong = GAMESTATE->m_pCurSong;
    Steps* pStepsToDelete = GAMESTATE->m_pCurSteps[PLAYER_1];
    FOREACH_PlayerNumber(pn) { GAMESTATE->m_pCurSteps[pn].Set(nullptr); }
    bool bSaveSong = !pStepsToDelete->WasLoadedFromProfile();
    pSong->DeleteSteps(pStepsToDelete);
    SONGMAN->Invalidate(pSong);

    /* Only save to the main .SM file if the steps we're deleting
     * were loaded from it. */
    if (bSaveSong) {
      pSong->Save();
      SCREENMAN->ZeroNextUpdate();
    }
    SCREENMAN->SendMessageToTopScreen(SM_RefreshSelector);
  } else if (
      SM == SM_Failure &&
      m_Selector.GetSelectedAction() == EditMenuAction_Delete) {
    LOG->Trace("Delete failed; not deleting steps");
  } else if (SM == SM_BackFromEditDescription) {
    if (!ScreenTextEntry::s_bCancelledLast) {
      SOUND->StopMusic();
      StartTransitioningScreen(SM_GoToNextScreen);
    }
  }

  ScreenWithMenuElements::HandleScreenMessage(SM);
}

bool ScreenEditMenu::MenuUp(const InputEventPlus&) {
  if (m_Selector.CanGoUp()) {
    m_Selector.Up();
    RefreshExplanationText();
  }
  return true;
}

bool ScreenEditMenu::MenuDown(const InputEventPlus&) {
  if (m_Selector.CanGoDown()) {
    m_Selector.Down();
    RefreshExplanationText();
  }
  return true;
}

bool ScreenEditMenu::MenuLeft(const InputEventPlus&) {
  if (m_Selector.CanGoLeft()) {
    m_Selector.Left();
  }
  return true;
}

bool ScreenEditMenu::MenuRight(const InputEventPlus&) {
  if (m_Selector.CanGoRight()) {
    m_Selector.Right();
  }
  return true;
}

static std::string GetCopyDescription(const Steps* pSourceSteps) {
  std::string s = pSourceSteps->GetDescription();
  return s;
}

static void SetCurrentStepsDescription(const std::string& s) {
  GAMESTATE->m_pCurSteps[0]->SetDescription(s);
}

static void DeleteCurrentSteps() {
  GAMESTATE->m_pCurSong->DeleteSteps(GAMESTATE->m_pCurSteps[0]);
  GAMESTATE->m_pCurSteps[0].Set(nullptr);
}

static LocalizedString MISSING_MUSIC_FILE(
    "ScreenEditMenu",
    "This song is missing a music file and cannot be edited.");
static LocalizedString SONG_DIR_READ_ONLY(
    "ScreenEditMenu", "The song directory is read-only and cannot be edited.");
static LocalizedString DELETED_AUTOGEN_STEPS(
    "ScreenEditMenu",
    "These steps are produced by autogen.  You do not need to delete them.");
static LocalizedString STEPS_WILL_BE_LOST(
    "ScreenEditMenu", "These steps will be lost permanently.");
static LocalizedString CONTINUE_WITH_DELETE(
    "ScreenEditMenu", "Continue with delete?");
static LocalizedString ENTER_EDIT_DESCRIPTION(
    "ScreenEditMenu", "Enter a description for this edit.");
static LocalizedString INVALID_SELECTION(
    "ScreenEditMenu",
    "One of the selected things is invalid.  Pick something valid instead.");

bool ScreenEditMenu::MenuStart(const InputEventPlus&) {
  if (IsTransitioning()) {
    return false;
  }
  if (!m_Selector.SafeToUse()) {
    return false;
  }

  if (m_Selector.CanGoDown()) {
    m_Selector.Down();
    RefreshExplanationText();
    return true;
  }

  Song* pSong = m_Selector.GetSelectedSong();
  StepsType st = m_Selector.GetSelectedStepsType();
  Difficulty dc = m_Selector.GetSelectedDifficulty();
  Steps* pSteps = m_Selector.GetSelectedSteps();
  //	StepsType soureNT	= m_Selector.GetSelectedSourceStepsType();
  //	Difficulty sourceDiff	= m_Selector.GetSelectedSourceDifficulty();
  Steps* pSourceSteps = m_Selector.GetSelectedSourceSteps();
  EditMenuAction action = m_Selector.GetSelectedAction();
  if (st == StepsType_Invalid) {
    ScreenPrompt::Prompt(SM_None, INVALID_SELECTION);
    return true;
  }

  GAMESTATE->m_pCurSong.Set(pSong);
  GAMESTATE->m_pCurCourse.Set(nullptr);
  GAMESTATE->SetCurrentStyle(
      GAMEMAN->GetEditorStyleForStepsType(st), PLAYER_INVALID);
  GAMESTATE->m_pCurSteps[PLAYER_1].Set(pSteps);

  // handle error cases
  if (!pSong->HasMusic()) {
    ScreenPrompt::Prompt(SM_None, MISSING_MUSIC_FILE);
    return true;
  }

  switch (m_Selector.EDIT_MODE) {
    case EditMode_Full: {
      std::string sDir = pSong->GetSongDir();
      std::string sTempFile = sDir + TEMP_FILE_NAME;
      RageFile file;
      if (!file.Open(sTempFile, RageFile::WRITE)) {
        ScreenPrompt::Prompt(SM_None, SONG_DIR_READ_ONLY);
        return true;
      }

      file.Close();
      FILEMAN->Remove(sTempFile);
      break;
    }
    default:
      break;
  }

  switch (action) {
    case EditMenuAction_Delete: {
      ASSERT(pSteps != nullptr);
      if (pSteps->IsAutogen()) {
        SCREENMAN->PlayInvalidSound();
        SCREENMAN->SystemMessage(DELETED_AUTOGEN_STEPS.GetValue());
        return true;
      }
    }
    default:
      break;
  }

  // Do work
  switch (action) {
    case EditMenuAction_Edit:
    case EditMenuAction_Practice:
      break;
    case EditMenuAction_Delete:
      ASSERT(pSteps != nullptr);
      ScreenPrompt::Prompt(
          SM_None,
          STEPS_WILL_BE_LOST.GetValue() + "\n\n" +
              CONTINUE_WITH_DELETE.GetValue(),
          PROMPT_YES_NO, ANSWER_NO);
      break;
    case EditMenuAction_LoadAutosave:
      if (pSong) {
        FOREACH_PlayerNumber(pn) { GAMESTATE->m_pCurSteps[pn].Set(nullptr); }
        pSong->LoadAutosaveFile();
        SONGMAN->Invalidate(pSong);
        SCREENMAN->SendMessageToTopScreen(SM_RefreshSelector);
      }
      break;
    case EditMenuAction_Create:
      ASSERT(!pSteps);
      {
        pSteps = pSong->CreateSteps();

        EditMode mode = m_Selector.EDIT_MODE;
        switch (mode) {
          default:
            FAIL_M(ssprintf("Invalid EditMode: %i", mode));
          case EditMode_Full:
            break;
          case EditMode_Home:
            pSteps->SetLoadedFromProfile(ProfileSlot_Machine);
            break;
          case EditMode_Practice:
            FAIL_M("Cannot create steps in EditMode_Practice");
        }

        std::string sEditName;
        if (pSourceSteps) {
          pSteps->CopyFrom(pSourceSteps, st, pSong->m_fMusicLengthSeconds);
          sEditName = GetCopyDescription(pSourceSteps);
        } else {
          pSteps->CreateBlank(st);
          pSteps->SetMeter(1);
          sEditName = "";
        }

        pSteps->SetDifficulty(
            dc);  // override difficulty with the user's choice
        SongUtil::MakeUniqueEditDescription(pSong, st, sEditName);
        pSteps->SetDescription(sEditName);
        pSong->AddSteps(pSteps);

        SCREENMAN->PlayStartSound();

        GAMESTATE->m_pCurSong.Set(pSong);
        GAMESTATE->m_pCurSteps[PLAYER_1].Set(pSteps);
        GAMESTATE->m_pCurCourse.Set(nullptr);
      }
      break;
    default:
      FAIL_M(ssprintf("Invalid edit menu action: %i", action));
  }

  // Go to the next screen.
  switch (action) {
    case EditMenuAction_Edit:
    case EditMenuAction_Create:
    case EditMenuAction_Practice: {
      // Prepare for ScreenEdit
      ASSERT(pSteps != nullptr);
      bool bPromptToNameSteps =
          (action == EditMenuAction_Create && dc == Difficulty_Edit);
      if (bPromptToNameSteps) {
        ScreenTextEntry::TextEntry(
            SM_BackFromEditDescription, ENTER_EDIT_DESCRIPTION,
            GAMESTATE->m_pCurSteps[0]->GetDescription(),
            MAX_STEPS_DESCRIPTION_LENGTH,
            SongUtil::ValidateCurrentStepsDescription,
            SetCurrentStepsDescription, DeleteCurrentSteps);
      } else {
        SOUND->StopMusic();
        SCREENMAN->PlayStartSound();
        StartTransitioningScreen(SM_GoToNextScreen);
      }
    }
      return true;
    case EditMenuAction_Delete:
    case EditMenuAction_LoadAutosave:
      return true;
    default:
      FAIL_M(ssprintf("Invalid edit menu action: %i", action));
  }
}

bool ScreenEditMenu::MenuBack(const InputEventPlus& input) {
  Cancel(SM_GoToPrevScreen);
  return true;
}

void ScreenEditMenu::RefreshExplanationText() {
  m_textExplanation.SetText(EXPLANATION_TEXT(m_Selector.GetSelectedRow()));
  m_textExplanation.StopTweening();
  ON_COMMAND(m_textExplanation);
}

void ScreenEditMenu::RefreshNumStepsLoadedFromProfile() {
  std::string s =
      ssprintf("edits used: %d", SONGMAN->GetNumStepsLoadedFromProfile());
  m_textNumStepsLoadedFromProfile.SetText(s);
}

namespace {

enum RowCountChoice {
  RCC_4M,
  RCC_2M,
  RCC_1M,
  RCC_2ND,
  RCC_4TH,
  RCC_8TH,
  RCC_12TH,
  RCC_16TH,
  RCC_24TH,
  RCC_32ND,
  RCC_48TH,
  RCC_64TH,
  RCC_192ND,
};

#define RCC_CHOICES                                                       \
  RCC_4TH, "4m", "2m", "1m", "2nd", "4th", "8th", "12th", "16th",    \
    "24th", "32nd", "48th", "64th", "192nd"

bool EnabledIfClipboardTimingIsSafe();
bool EnabledIfClipboardTimingIsSafe() {
  TimingData* clipboard_full_timing = ScreenEditClipboardTimingSlot();
  return clipboard_full_timing != nullptr &&
     clipboard_full_timing->IsSafeFullTiming();
}

}  // namespace

static bool EnabledIfSet1SongBGAnimation();
static bool EnabledIfSet1SongMovie();
static bool EnabledIfSet1SongBitmap();
static bool EnabledIfSet1GlobalBGAnimation();
static bool EnabledIfSet1GlobalMovie();
static bool EnabledIfSet1GlobalMovieSongGroup();
static bool EnabledIfSet1GlobalMovieSongGroupAndGenre();
static bool EnabledIfSet2SongBGAnimation();
static bool EnabledIfSet2SongMovie();
static bool EnabledIfSet2SongBitmap();
static bool EnabledIfSet2GlobalBGAnimation();
static bool EnabledIfSet2GlobalMovie();
static bool EnabledIfSet2GlobalMovieSongGroup();
static bool EnabledIfSet2GlobalMovieSongGroupAndGenre();

int GetRowsFromAnswers(int choice, const std::vector<int>& answers) {
  if (answers.empty()) {
  return 192 / 8;
  }
  switch (answers[choice]) {
  case RCC_4M:
    return 192 * 4;
  case RCC_2M:
    return 192 * 2;
  case RCC_1M:
    return 192 * 1;
  case RCC_2ND:
    return 192 / 2;
  case RCC_4TH:
  default:
    return 192 / 4;
  case RCC_8TH:
    return 192 / 8;
  case RCC_12TH:
    return 192 / 12;
  case RCC_16TH:
    return 192 / 16;
  case RCC_24TH:
    return 192 / 24;
  case RCC_32ND:
    return 192 / 32;
  case RCC_48TH:
    return 192 / 48;
  case RCC_64TH:
    return 192 / 64;
  case RCC_192ND:
    return 192 / 192;
  }
}

TimingData*& ScreenEditClipboardTimingSlot() {
  static TimingData* clipboard_full_timing = nullptr;
  return clipboard_full_timing;
}

MenuDef g_EditHelp(
  "ScreenMiniMenuEditHelp"
  // fill this in dynamically
);

MenuDef g_AttackAtTimeMenu(
  "ScreenMiniMenuAttackAtTimeMenu"
  // fill this in dynamically
);

MenuDef g_IndividualAttack(
  "ScreenMiniMenuIndividualAttack"
  // fill this in dynamically
);

MenuDef g_KeysoundTrack(
  "ScreenMiniMenuKeysoundTrack");  // fill this in dynamically

MenuDef g_MainMenu(
  "ScreenMiniMenuMainMenu",
  MenuRowDef(
    ScreenEdit::play_whole_song, "Play whole song", true, EditMode_Practice,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::play_current_beat_to_end, "Play current beat to end", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::save, "Save", true, EditMode_Home, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::play_selection, "Play selection", true, EditMode_Practice,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::set_selection_start, "Set selection start", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::set_selection_end, "Set selection end", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::revert_to_last_save, "Revert to last save", true,
    EditMode_Home, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::revert_from_disk, "Revert from disk", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::options, "Editor options", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::edit_song_info, "Edit song info", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::edit_steps_information, "Edit steps information", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::edit_timing_data, "Edit Timing Data", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::view_steps_data, "View steps data", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::play_preview_music, "Play preview music", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::exit, "Exit Edit Mode", true, EditMode_Practice, true, true,
    0, nullptr));

MenuDef g_AlterMenu(
  "ScreenMiniMenuAlterMenu",
  MenuRowDef(
    ScreenEdit::cut, "Cut", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::copy, "Copy", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::clear, "Clear area", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::quantize, "Quantize", true, EditMode_Practice, true, true,
    0, "4th", "8th", "12th", "16th", "24th", "32nd", "48th", "64th",
    "192nd"),
  MenuRowDef(
    ScreenEdit::turn, "Turn", true, EditMode_Practice, true, true, 0,
    "Left", "Right", "Mirror", "LRMirror", "UDMirror", "Backwards",
    "Shuffle", "SuperShuffle", "HyperShuffle"),
  MenuRowDef(
    ScreenEdit::transform, "Transform", true, EditMode_Practice, true, true,
    0, "NoHolds", "NoMines", "Little", "Wide", "Big", "Quick", "Skippy",
    "Mines", "Echo", "Stomp", "Planted", "Floored", "Twister", "NoJumps",
    "NoHands", "NoQuads", "NoStretch"),
  MenuRowDef(
    ScreenEdit::alter, "Alter", true, EditMode_Practice, true, true, 0,
    "Autogen To Fill Width", "Backwards", "Swap Sides",
    "Copy Left To Right", "Copy Right To Left", "Clear Left", "Clear Right",
    "Collapse To One", "Collapse Left", "Shift Left", "Shift Right",
    "Swap Up/Down", "Arbitrary Remap Columns"),
  MenuRowDef(
    ScreenEdit::tempo, "Tempo", true, EditMode_Full, true, true, 0,
    "Compress 2x", "Compress 3->2", "Compress 4->3", "Expand 3->4",
    "Expand 2->3", "Expand 2x"),
  MenuRowDef(
    ScreenEdit::play, "Play selection", true, EditMode_Practice, true, true,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::record, "Record in selection", true, EditMode_Practice,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::preview_designation, "Designate as Music Preview", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_to_pause, "Convert selection to pause", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_to_delay, "Convert selection to delay", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_to_warp, "Convert selection to warp", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_to_fake, "Convert selection to fake", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_to_attack, "Convert selection to attack", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::routine_invert_notes, "Invert notes' player", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::routine_mirror_1_to_2, "Mirror Player 1 to 2", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::routine_mirror_2_to_1, "Mirror Player 2 to 1", true,
    EditMode_Full, true, true, 0, nullptr));

MenuDef g_AreaMenu(
  "ScreenMiniMenuAreaMenu",
  MenuRowDef(
    ScreenEdit::paste_at_current_beat, "Paste at current beat", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::paste_at_begin_marker, "Paste at begin marker", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::insert_and_shift, "Insert beat and shift down", true,
    EditMode_Practice, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::delete_and_shift, "Delete beat and shift up", true,
    EditMode_Practice, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::shift_pauses_forward, "Shift all timing changes down", true,
    EditMode_Full, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::shift_pauses_backward, "Shift all timing changes up", true,
    EditMode_Full, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::convert_pause_to_beat, "Convert pause to beats", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::convert_delay_to_beat, "Convert delay to beats", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::last_second_at_beat,
    "Designate last second at current beat", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::undo, "Undo", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::clear_clipboard, "Clear clipboard", true, EditMode_Practice,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::modify_attacks_at_row, "Modify Attacks at current beat",
    true, EditMode_CourseMods, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::modify_keysounds_at_row, "Modify Keysounds at current beat",
    true, EditMode_Full, true, true, 0, nullptr));

MenuDef g_StepsInformation(
  "ScreenMiniMenuStepsInformation",
  MenuRowDef(
    ScreenEdit::difficulty, "Difficulty", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::meter, "Meter", true, EditMode_Practice, true, false, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::predict_meter, "Predicted Meter", false, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::chartname, "Chart Name", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::description, "Description", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::chartstyle, "Chart Style", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::step_credit, "Step Author", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::step_display_bpm, "Display BPM", true, EditMode_Full, true,
    true, 0, "Actual", "Specified", "Random"),
  MenuRowDef(
    ScreenEdit::step_min_bpm, "Min BPM", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::step_max_bpm, "Max BPM", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::step_music, "Music File", true, EditMode_Full, true, true,
    0, nullptr));

MenuDef g_StepsData(
  "ScreenMiniMenuStepsData",
  MenuRowDef(
    ScreenEdit::tap_notes, "Tap Steps", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::jumps, "Jumps", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::hands, "Hands", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::quads, "Quads", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::holds, "Holds", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::mines, "Mines", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::rolls, "Rolls", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::lifts, "Lifts", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::fakes, "Fakes", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::stream, "Stream", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::voltage, "Voltage", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::air, "Air", false, EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::freeze, "Freeze", false, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::chaos, "Chaos", false, EditMode_Full, true, true, 0,
    nullptr));

MenuDef g_SongInformation(
  "ScreenMiniMenuSongInformation",
  MenuRowDef(
    ScreenEdit::main_title, "Main title", true, EditMode_Practice, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::sub_title, "Sub title", true, EditMode_Practice, true, true,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::artist, "Artist", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::genre, "Genre", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::credit, "Credit", true, EditMode_Practice, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::preview, "Preview", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::main_title_transliteration, "Main title transliteration",
    true, EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::sub_title_transliteration, "Sub title transliteration",
    true, EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::artist_transliteration, "Artist transliteration", true,
    EditMode_Practice, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::last_second_hint, "Last second hint", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::preview_start, "Preview Start", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::preview_length, "Preview Length", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::display_bpm, "Display BPM", true, EditMode_Full, true, true,
    0, "Actual", "Specified", "Random"),
  MenuRowDef(
    ScreenEdit::min_bpm, "Min BPM", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::max_bpm, "Max BPM", true, EditMode_Full, true, true, 0,
    nullptr));

MenuDef g_TimingDataInformation(
  "ScreenMiniMenuTimingDataInformation",
  MenuRowDef(
    ScreenEdit::beat_0_offset, "Beat 0 Offset", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::bpm, "Edit BPM change", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::stop, "Edit stop", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::delay, "Edit delay", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::label, "Edit label", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::tickcount, "Edit tickcount", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::combo, "Edit combo", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::warp, "Edit warp", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::speed_percent, "Edit speed (percent)", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::speed_wait, "Edit speed (wait)", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::speed_mode, "Edit speed (mode)", true, EditMode_Full, true,
    true, 0, "Beats", "Seconds"),
  MenuRowDef(
    ScreenEdit::scroll, "Edit scrolling factor", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::fake, "Edit fake", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::shift_timing_in_region_down, "Shift timing in region down",
    true, EditMode_Full, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::shift_timing_in_region_up, "Shift timing in region up", true,
    EditMode_Full, true, true, RCC_CHOICES),
  MenuRowDef(
    ScreenEdit::copy_timing_in_region, "Copy timing in region", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::clear_timing_in_region, "Clear timing in region", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::paste_timing_from_clip, "Paste timing from clipboard", true,
    EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::copy_full_timing, "Copy timing data", true, EditMode_Full,
    true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::paste_full_timing, "Paste timing data",
    EnabledIfClipboardTimingIsSafe, EditMode_Full, true, true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::erase_step_timing, "Erase step timing", true, EditMode_Full,
    true, true, 0, nullptr));

MenuDef g_TimingDataChangeInformation(
  "ScreenMiniMenuTimingDataChangeInformation",
  MenuRowDef(
    ScreenEdit::timing_all, "All timing", true, EditMode_Full, true, true,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_bpm, "BPM changes", true, EditMode_Full, true, true,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_stop, "Stops", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::timing_delay, "Delays", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::timing_warp, "Warps", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::timing_label, "Labels", true, EditMode_Full, true, true, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::timing_tickcount, "Tickcount", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_combo, "Combo segments", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_speed, "Speed segments", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_scroll, "Scroll segments", true, EditMode_Full, true,
    true, 0, nullptr),
  MenuRowDef(
    ScreenEdit::timing_fake, "Fakes", true, EditMode_Full, true, true, 0,
    nullptr));

MenuDef g_BackgroundChange(
  "ScreenMiniMenuBackgroundChange",
  MenuRowDef(
    ScreenEdit::layer, "Layer", false, EditMode_Full, true, false, 0, ""),
  MenuRowDef(
    ScreenEdit::rate, "Rate", true, EditMode_Full, true, false, 10, "0%",
    "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%",
    "100%", "120%", "140%", "160%", "180%", "200%", "220%", "240%",
    "260%", "280%", "300%", "350%", "400%"),
  MenuRowDef(
    ScreenEdit::transition, "Force Transition", true, EditMode_Full, true,
    false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::effect, "Force Effect", true, EditMode_Full, true, false, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::color1, "Force Color 1", true, EditMode_Full, true, false,
    0, "-", "#FFFFFF", "#808080", "#FFFFFF80", "#000000", "#FF0000",
    "#00FF00", "#0000FF", "#FFFF00", "#00FFFF", "#FF00FF"),
  MenuRowDef(
    ScreenEdit::color2, "Force Color 2", true, EditMode_Full, true, false,
    0, "-", "#FFFFFF", "#808080", "#FFFFFF80", "#000000", "#FF0000",
    "#00FF00", "#0000FF", "#FFFF00", "#00FFFF", "#FF00FF"),
  MenuRowDef(
    ScreenEdit::file1_type, "File1 Type", true, EditMode_Full, true, true,
    0, "Song BGAnimation", "Song Movie", "Song Bitmap",
    "Global BGAnimation", "Global Movie", "Global Movie from Song Group",
    "Global Movie from Song Group and Genre", "Dynamic Random",
    "Baked Random", "None"),
  MenuRowDef(
    ScreenEdit::file1_song_bganimation, "File1 Song BGAnimation",
    EnabledIfSet1SongBGAnimation, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file1_song_movie, "File1 Song Movie",
    EnabledIfSet1SongMovie, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file1_song_still, "File1 Song Still",
    EnabledIfSet1SongBitmap, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file1_global_bganimation, "File1 Global BGAnimation",
    EnabledIfSet1GlobalBGAnimation, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file1_global_movie, "File1 Global Movie",
    EnabledIfSet1GlobalMovie, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file1_global_movie_song_group, "File1 Global Movie (Group)",
    EnabledIfSet1GlobalMovieSongGroup, EditMode_Full, true, false, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::file1_global_movie_song_group_and_genre,
    "File1 Global Movie (Group + Genre)",
    EnabledIfSet1GlobalMovieSongGroupAndGenre, EditMode_Full, true, false,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_type, "File2 Type", true, EditMode_Full, true, true,
    0, "Song BGAnimation", "Song Movie", "Song Bitmap",
    "Global BGAnimation", "Global Movie", "Global Movie from Song Group",
    "Global Movie from Song Group and Genre", "Dynamic Random",
    "Baked Random", "None"),
  MenuRowDef(
    ScreenEdit::file2_song_bganimation, "File2 Song BGAnimation",
    EnabledIfSet2SongBGAnimation, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_song_movie, "File2 Song Movie",
    EnabledIfSet2SongMovie, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_song_still, "File2 Song Still",
    EnabledIfSet2SongBitmap, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_global_bganimation, "File2 Global BGAnimation",
    EnabledIfSet2GlobalBGAnimation, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_global_movie, "File2 Global Movie",
    EnabledIfSet2GlobalMovie, EditMode_Full, true, false, 0, nullptr),
  MenuRowDef(
    ScreenEdit::file2_global_movie_song_group, "File2 Global Movie (Group)",
    EnabledIfSet2GlobalMovieSongGroup, EditMode_Full, true, false, 0,
    nullptr),
  MenuRowDef(
    ScreenEdit::file2_global_movie_song_group_and_genre,
    "File2 Global Movie (Group + Genre)",
    EnabledIfSet2GlobalMovieSongGroupAndGenre, EditMode_Full, true, false,
    0, nullptr),
  MenuRowDef(
    ScreenEdit::delete_change, "Remove Change", true, EditMode_Full, true,
    true, 0, nullptr));

static bool EnabledIfSet1SongBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
       song_bganimation &&
     !g_BackgroundChange.rows[ScreenEdit::file1_song_bganimation]
        .choices.empty();
}
static bool EnabledIfSet1SongMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] == song_movie &&
     !g_BackgroundChange.rows[ScreenEdit::file1_song_movie].choices.empty();
}
static bool EnabledIfSet1SongBitmap() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] == song_bitmap &&
     !g_BackgroundChange.rows[ScreenEdit::file1_song_still].choices.empty();
}
static bool EnabledIfSet1GlobalBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
       global_bganimation &&
     !g_BackgroundChange.rows[ScreenEdit::file1_global_bganimation]
        .choices.empty();
}
static bool EnabledIfSet1GlobalMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
       global_movie &&
     !g_BackgroundChange.rows[ScreenEdit::file1_global_movie]
        .choices.empty();
}
static bool EnabledIfSet1GlobalMovieSongGroup() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
       global_movie_song_group &&
     !g_BackgroundChange.rows[ScreenEdit::file1_global_movie_song_group]
        .choices.empty();
}
static bool EnabledIfSet1GlobalMovieSongGroupAndGenre() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
       global_movie_song_group_and_genre &&
     !g_BackgroundChange
        .rows[ScreenEdit::file1_global_movie_song_group_and_genre]
        .choices.empty();
}
static bool EnabledIfSet2SongBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
       song_bganimation &&
     !g_BackgroundChange.rows[ScreenEdit::file2_song_bganimation]
        .choices.empty();
}
static bool EnabledIfSet2SongMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] == song_movie &&
     !g_BackgroundChange.rows[ScreenEdit::file2_song_movie].choices.empty();
}
static bool EnabledIfSet2SongBitmap() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] == song_bitmap &&
     !g_BackgroundChange.rows[ScreenEdit::file2_song_still].choices.empty();
}
static bool EnabledIfSet2GlobalBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
       global_bganimation &&
     !g_BackgroundChange.rows[ScreenEdit::file2_global_bganimation]
        .choices.empty();
}
static bool EnabledIfSet2GlobalMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] == global_movie &&
     !g_BackgroundChange.rows[ScreenEdit::file2_global_movie]
        .choices.empty();
}
static bool EnabledIfSet2GlobalMovieSongGroup() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
       global_movie_song_group &&
     !g_BackgroundChange.rows[ScreenEdit::file2_global_movie_song_group]
        .choices.empty();
}
static bool EnabledIfSet2GlobalMovieSongGroupAndGenre() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
       global_movie_song_group_and_genre &&
     !g_BackgroundChange
        .rows[ScreenEdit::file2_global_movie_song_group_and_genre]
        .choices.empty();
}

std::string GetOneBakedRandomFile(Song* pSong, bool bTryGenre) {
  std::vector<std::string> vsPaths;
  std::vector<std::string> vsNames;
  BackgroundUtil::GetGlobalRandomMovies(pSong, "", vsPaths, vsNames, bTryGenre);

  return vsNames.empty() ? std::string() : vsNames[RandomInt(vsNames.size())];
}

MenuDef g_InsertTapAttack(
  "ScreenMiniMenuInsertTapAttack",
  MenuRowDef(
    -1, "Duration seconds", true, EditMode_Practice, true, false, 3, "5",
    "10", "15", "20", "25", "30", "35", "40", "45"),
  MenuRowDef(
    -1, "Set modifiers", true, EditMode_Practice, true, true, 0,
    "Press Start"));

MenuDef g_InsertCourseAttack(
  "ScreenMiniMenuInsertCourseAttack",
  MenuRowDef(
    ScreenEdit::duration, "Duration seconds", true, EditMode_Practice, true,
    false, 3, "5", "10", "15", "20", "25", "30", "35", "40", "45"),
  MenuRowDef(
    ScreenEdit::set_mods, "Set modifiers", true, EditMode_Practice, true,
    true, 0, "Press Start"),
  MenuRowDef(
    ScreenEdit::remove, "Remove", true, EditMode_Practice, true, true, 0,
    "Press Start"));

MenuDef g_InsertStepAttack(
  "ScreenMiniMenuInsertCourseAttack",
  MenuRowDef(
    ScreenEdit::sa_duration, "Duration seconds", true, EditMode_Practice,
    true, false, 3, "5", "10", "15", "20", "25", "30", "35", "40",
    "45"),
  MenuRowDef(
    ScreenEdit::sa_set_mods, "Set modifiers", true, EditMode_Practice, true,
    true, 0, "Press Start"),
  MenuRowDef(
    ScreenEdit::sa_remove, "Remove", true, EditMode_Practice, true, true, 0,
    "Press Start"));

MenuDef g_CourseMode(
  "ScreenMiniMenuCourseDisplay",
  MenuRowDef(
    -1, "Play mods from course", true, EditMode_Practice, true, false, 0,
    nullptr));

/*
 * (c) 2002-2004 Chris Danford
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
