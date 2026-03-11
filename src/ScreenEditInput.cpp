#include "global.h"
#include "ScreenEdit.h"

#include "BackgroundUtil.h"
#include "Course.h"
#include "Game.h"
#include "GameManager.h"
#include "GameState.h"
#include "InputFilter.h"
#include "LocalizedString.h"
#include "NoteDataUtil.h"
#include "PrefsManager.h"
#include "ScreenEditMenus.h"
#include "ScreenManager.h"
#include "SongUtil.h"
#include "StepsUtil.h"
#include "Style.h"
#include "ThemeManager.h"
#include "ThemeMetric.h"
#include "RageUtil/ConvertValue.h"

extern int g_iLastInsertTapAttackTrack;
extern float g_fLastInsertAttackDurationSeconds;
extern float g_fLastInsertAttackPositionSeconds;
extern BackgroundLayer g_CurrentBGChangeLayer;
extern const float record_hold_default;
extern float record_hold_seconds;

AutoScreenMessage(SM_BackFromMainMenu);
AutoScreenMessage(SM_BackFromAreaMenu);
AutoScreenMessage(SM_BackFromAlterMenu);
AutoScreenMessage(SM_BackFromBGChange);
AutoScreenMessage(SM_BackFromCourseModeMenu);
AutoScreenMessage(SM_BackFromInsertTapAttack);
AutoScreenMessage(SM_BackFromInsertCourseAttack);
AutoScreenMessage(SM_BackFromInsertStepAttack);
AutoScreenMessage(SM_BackFromInsertTapAttackPlayerOptions);
AutoScreenMessage(SM_BackFromInsertCourseAttackPlayerOptions);

bool ScreenEdit::Input(const InputEventPlus& input) {
  //  LOG->Trace( "ScreenEdit::Input()" );

  // invalidate input if cmd/meta is being held.
  if (INPUTFILTER->IsBeingPressed(DeviceInput(DEVICE_KEYBOARD, KEY_LMETA)) ||
      INPUTFILTER->IsBeingPressed(DeviceInput(DEVICE_KEYBOARD, KEY_RMETA))) {
    return false;
  }

  if (m_In.IsTransitioning() || m_Out.IsTransitioning()) {
    return false;
  }

  EditButton EditB = DeviceToEdit(input.DeviceI);
  if (EditB == EditButton_Invalid) {
    EditB = MenuButtonToEditButton(input.MenuI);
  }

  if (EditB == EDIT_BUTTON_REMOVE_NOTE) {
    /* Ugly: we need to know when the button was pressed or released, so we
     * can clamp operations to that time.  Should InputFilter keep track of
     * last release, too? */
    m_bRemoveNoteButtonDown = (input.type != IET_RELEASE);
    m_RemoveNoteButtonLastChanged.Touch();
  }

  switch (m_EditState) {
    DEFAULT_FAIL(m_EditState);
    case STATE_EDITING:
      m_bTextInfoNeedsUpdate = true;
      return InputEdit(input, EditB);
    case STATE_RECORDING:
      return InputRecord(input, EditB);
    case STATE_RECORDING_PAUSED:
      return InputRecordPaused(input, EditB);
    case STATE_PLAYING:
      return InputPlay(input, EditB);
  }
}

static void ShiftToRightSide(int& iCol, int iNumTracks) {
  switch (GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
              ->m_StyleType) {
    DEFAULT_FAIL(GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
                     ->m_StyleType);
    case StyleType_OnePlayerOneSide:
      break;
    case StyleType_TwoPlayersTwoSides:
    case StyleType_OnePlayerTwoSides:
    case StyleType_TwoPlayersSharedSides:
      iCol += iNumTracks / 2;
      break;
  }
}

static LocalizedString BG_CHANGE_STEP_TIMING(
    "ScreenEdit", "You must be in Song Timing Mode to edit BG Changes.");
static LocalizedString ALTER_MENU_NO_SELECTION(
    "ScreenEdit", "You must have an area selected to enter the Alter Menu.");
static LocalizedString SWITCHED_TO("ScreenEdit", "Switched to");
static LocalizedString NO_BACKGROUNDS_AVAILABLE(
    "ScreenEdit", "No backgrounds available");
static ThemeMetric<bool> INVERT_SCROLL_BUTTONS(
    "ScreenEdit", "InvertScrollSpeedButtons");

bool ScreenEdit::InputEdit(const InputEventPlus& input, EditButton EditB) {
  if (input.type == IET_RELEASE) {
    if (EditPressed(EDIT_BUTTON_SCROLL_SELECT, input.DeviceI)) {
      m_iShiftAnchor = -1;
    }
    // XXX Key releases usually don't count as "handled," but what
    // does it mean in this case?
    return false;
  }
  const TimingData& sTiming = GetAppropriateTiming();
  float playerBeat = GetAppropriatePosition().m_fSongBeat;
  int beatsPerMeasure =
      sTiming.GetTimeSignatureSegmentAtBeat(playerBeat)->GetNum();

  switch (EditB) {
    case EDIT_BUTTON_COLUMN_0:
    case EDIT_BUTTON_COLUMN_1:
    case EDIT_BUTTON_COLUMN_2:
    case EDIT_BUTTON_COLUMN_3:
    case EDIT_BUTTON_COLUMN_4:
    case EDIT_BUTTON_COLUMN_5:
    case EDIT_BUTTON_COLUMN_6:
    case EDIT_BUTTON_COLUMN_7:
    case EDIT_BUTTON_COLUMN_8:
    case EDIT_BUTTON_COLUMN_9: {
      if (input.type != IET_FIRST_PRESS) {
        return false;  // We only care about first presses
      }

      int iCol = EditB - EDIT_BUTTON_COLUMN_0;

      // Alt + number = input to right half
      if (EditIsBeingPressed(EDIT_BUTTON_RIGHT_SIDE)) {
        ShiftToRightSide(iCol, m_NoteDataEdit.GetNumTracks());
      }

      if (iCol >= m_NoteDataEdit.GetNumTracks()) {
        return false;  // this button is not in the range of columns for this
                       // Style
      }

      const float fSongBeat = GetBeat();
      const int iSongIndex = BeatToNoteRow(fSongBeat);

      // check for to see if the user intended to remove a HoldNote
      int iHeadRow;
      if (m_NoteDataEdit.IsHoldNoteAtRow(iCol, iSongIndex, &iHeadRow)) {
        m_soundRemoveNote.Play(true);
        SetDirty(true);
        SaveUndo();
        m_NoteDataEdit.SetTapNote(iCol, iHeadRow, TAP_EMPTY);
        // Don't CheckNumberOfNotesAndUndo.  We don't want to revert any change
        // that removes notes.
      } else if (
          m_NoteDataEdit.GetTapNote(iCol, iSongIndex).type !=
          TapNoteType_Empty) {
        m_soundRemoveNote.Play(true);
        SetDirty(true);
        SaveUndo();
        m_NoteDataEdit.SetTapNote(iCol, iSongIndex, TAP_EMPTY);
        // Don't CheckNumberOfNotesAndUndo.  We don't want to revert any change
        // that removes notes.
      } else if (EditIsBeingPressed(EDIT_BUTTON_LAY_TAP_ATTACK)) {
        g_iLastInsertTapAttackTrack = iCol;
        EditMiniMenu(&g_InsertTapAttack, SM_BackFromInsertTapAttack);
      } else {
        m_soundAddNote.Play(true);
        SetDirty(true);
        SaveUndo();
        TapNote tn = m_selectedTap;
        tn.pn = m_InputPlayerNumber;
        m_NoteDataEdit.SetTapNote(iCol, iSongIndex, tn);
        CheckNumberOfNotesAndUndo();
      }
    }
      return true;

    case EDIT_BUTTON_CYCLE_TAP_LEFT: {
      switch (m_selectedTap.type) {
        case TapNoteType_Tap:
          m_selectedTap = TAP_ORIGINAL_FAKE;
          break;
        case TapNoteType_Mine:
          m_selectedTap = TAP_ORIGINAL_TAP;
          break;
        case TapNoteType_Lift:
          m_selectedTap = TAP_ORIGINAL_MINE;
          break;
        case TapNoteType_Fake:
          m_selectedTap = TAP_ORIGINAL_LIFT;
          break;
          DEFAULT_FAIL(m_selectedTap.type);
      }
      return true;
    }
    case EDIT_BUTTON_CYCLE_TAP_RIGHT: {
      switch (m_selectedTap.type) {
        case TapNoteType_Tap:
          m_selectedTap = TAP_ORIGINAL_MINE;
          break;
        case TapNoteType_Mine:
          m_selectedTap = TAP_ORIGINAL_LIFT;
          break;
        case TapNoteType_Lift:
          m_selectedTap = TAP_ORIGINAL_FAKE;
          break;
        case TapNoteType_Fake:
          m_selectedTap = TAP_ORIGINAL_TAP;
          break;
          DEFAULT_FAIL(m_selectedTap.type);
      }
      return true;
    }
    case EDIT_BUTTON_CYCLE_SEGMENT_LEFT: {
      int tmp = enum_add2(this->currentCycleSegment, -1);
      wrap(*ConvertValue<int>(&tmp), NUM_TimingSegmentType);
      currentCycleSegment = (TimingSegmentType)tmp;
      return true;
    }
    case EDIT_BUTTON_CYCLE_SEGMENT_RIGHT: {
      int tmp = enum_add2(this->currentCycleSegment, +1);
      wrap(*ConvertValue<int>(&tmp), NUM_TimingSegmentType);
      currentCycleSegment = (TimingSegmentType)tmp;
      return true;
    }
    case EDIT_BUTTON_SCROLL_SPEED_UP:
    case EDIT_BUTTON_SCROLL_SPEED_DOWN: {
      PlayerState* pPlayerState =
          const_cast<PlayerState*>(m_NoteFieldEdit.GetPlayerState());
      float fScrollSpeed =
          pPlayerState->m_PlayerOptions.GetSong().m_fScrollSpeed;

      const float fSpeeds[] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
      int iSpeed = 0;
      for (unsigned i = 0; i < ARRAYLEN(fSpeeds); ++i) {
        if (fSpeeds[i] == fScrollSpeed) {
          iSpeed = i;
          break;
        }
      }

      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_SCROLL_SPEED_DOWN:
          INVERT_SCROLL_BUTTONS ? ++iSpeed : --iSpeed;
          break;
        case EDIT_BUTTON_SCROLL_SPEED_UP:
          INVERT_SCROLL_BUTTONS ? --iSpeed : ++iSpeed;
          break;
      }
      iSpeed = std::clamp(iSpeed, 0, (int)ARRAYLEN(fSpeeds) - 1);

      if (fSpeeds[iSpeed] != fScrollSpeed) {
        m_soundMarker.Play(true);
        fScrollSpeed = fSpeeds[iSpeed];
      }

      PO_GROUP_ASSIGN(
          pPlayerState->m_PlayerOptions, ModsLevel_Song, m_fScrollSpeed,
          fScrollSpeed);
    }

      return true;
    case EDIT_BUTTON_SCROLL_HOME:
      ScrollTo(0);
      return true;
    case EDIT_BUTTON_SCROLL_END:
      ScrollTo(m_NoteDataEdit.GetLastBeat());
      return true;
    case EDIT_BUTTON_SCROLL_UP_LINE:
    case EDIT_BUTTON_SCROLL_UP_PAGE:
    case EDIT_BUTTON_SCROLL_UP_TS:
    case EDIT_BUTTON_SCROLL_DOWN_LINE:
    case EDIT_BUTTON_SCROLL_DOWN_PAGE:
    case EDIT_BUTTON_SCROLL_DOWN_TS: {
      float fBeatsToMove = 0.f;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_SCROLL_UP_LINE:
        case EDIT_BUTTON_SCROLL_DOWN_LINE:
          fBeatsToMove = NoteTypeToBeat(m_SnapDisplay.GetNoteType());
          if (EditB == EDIT_BUTTON_SCROLL_UP_LINE) {
            fBeatsToMove *= -1;
          }
          break;
        case EDIT_BUTTON_SCROLL_UP_PAGE:
        case EDIT_BUTTON_SCROLL_DOWN_PAGE:
          fBeatsToMove = 4;
          if (EditB == EDIT_BUTTON_SCROLL_UP_PAGE) {
            fBeatsToMove *= -1;
          }
          break;
        case EDIT_BUTTON_SCROLL_UP_TS:
        case EDIT_BUTTON_SCROLL_DOWN_TS:
          fBeatsToMove = beatsPerMeasure;
          if (EditB == EDIT_BUTTON_SCROLL_UP_TS) {
            fBeatsToMove *= -1;
          }
          break;
      }

      if (m_PlayerStateEdit.m_PlayerOptions.GetSong()
              .m_fScrolls[PlayerOptions::SCROLL_REVERSE] > 0.5) {
        fBeatsToMove *= -1;
      }

      float fDestinationBeat = GetBeat() + fBeatsToMove;
      fDestinationBeat = Quantize(
          fDestinationBeat, NoteTypeToBeat(m_SnapDisplay.GetNoteType()));

      ScrollTo(fDestinationBeat);
    }
      return true;
    case EDIT_BUTTON_SCROLL_NEXT_MEASURE: {
      float fDestinationBeat = GetBeat() + beatsPerMeasure;
      fDestinationBeat = ftruncf(fDestinationBeat, (float)beatsPerMeasure);
      ScrollTo(fDestinationBeat);
      return true;
    }
    case EDIT_BUTTON_SCROLL_PREV_MEASURE: {
      float fDestinationBeat = QuantizeUp(GetBeat(), (float)beatsPerMeasure);
      fDestinationBeat -= (float)beatsPerMeasure;
      ScrollTo(fDestinationBeat);
      return true;
    }
    case EDIT_BUTTON_SCROLL_NEXT: {
      int iRow = BeatToNoteRow(GetBeat());
      NoteDataUtil::GetNextEditorPosition(m_NoteDataEdit, iRow);
      ScrollTo(NoteRowToBeat(iRow));
    }
      return true;
    case EDIT_BUTTON_SCROLL_PREV: {
      int iRow = BeatToNoteRow(GetBeat());
      NoteDataUtil::GetPrevEditorPosition(m_NoteDataEdit, iRow);
      ScrollTo(NoteRowToBeat(iRow));
    }
      return true;
    case EDIT_BUTTON_SEGMENT_NEXT: {
      // TODO: Work around Stops and Delays. We MAY have to separate them.
      const TimingData& timing = GetAppropriateTiming();
      ScrollTo(timing.GetNextSegmentBeatAtBeat(
          this->currentCycleSegment, GetBeat()));
    }
      return true;
    case EDIT_BUTTON_SEGMENT_PREV: {
      // TODO: Work around Stops and Delays. We MAY have to separate them.
      const TimingData& timing = GetAppropriateTiming();
      ScrollTo(timing.GetPreviousSegmentBeatAtBeat(
          this->currentCycleSegment, GetBeat()));
    }
      return true;
    case EDIT_BUTTON_SNAP_NEXT:
      if (m_SnapDisplay.PrevSnapMode()) {
        OnSnapModeChange();
      }
      return true;
    case EDIT_BUTTON_SNAP_PREV:
      if (m_SnapDisplay.NextSnapMode()) {
        OnSnapModeChange();
      }
      return true;
    case EDIT_BUTTON_LAY_SELECT: {
      const int iCurrentRow =
          BeatToNoteRow(GetAppropriatePosition().m_fSongBeat);
      if (m_NoteFieldEdit.m_iBeginMarker == -1 &&
          m_NoteFieldEdit.m_iEndMarker == -1) {
        // lay begin marker
        m_NoteFieldEdit.m_iBeginMarker =
            BeatToNoteRow(GetAppropriatePosition().m_fSongBeat);
      } else if (m_NoteFieldEdit.m_iEndMarker == -1)  // only begin marker is
                                                      // laid
      {
        if (iCurrentRow == m_NoteFieldEdit.m_iBeginMarker) {
          m_NoteFieldEdit.m_iBeginMarker = -1;
        } else {
          m_NoteFieldEdit.m_iEndMarker =
              std::max(m_NoteFieldEdit.m_iBeginMarker, iCurrentRow);
          m_NoteFieldEdit.m_iBeginMarker =
              std::min(m_NoteFieldEdit.m_iBeginMarker, iCurrentRow);
        }
      } else  // both markers are laid
      {
        m_NoteFieldEdit.m_iBeginMarker = iCurrentRow;
        m_NoteFieldEdit.m_iEndMarker = -1;
      }
      m_soundMarker.Play(true);
    }
      return true;
    case EDIT_BUTTON_OPEN_AREA_MENU: {
      // TODO: Improve behavior if timing changes are shifted down on beat 0.
      g_AreaMenu.rows[shift_pauses_forward].bEnabled = (GetBeat() != 0);
      g_AreaMenu.rows[paste_at_current_beat].bEnabled = !m_Clipboard.IsEmpty();
      g_AreaMenu.rows[paste_at_begin_marker].bEnabled =
          !m_Clipboard.IsEmpty() != 0 && m_NoteFieldEdit.m_iBeginMarker != -1;
      g_AreaMenu.rows[undo].bEnabled = m_bHasUndo;
      EditMiniMenu(&g_AreaMenu, SM_BackFromAreaMenu);
    }
      return true;
    case EDIT_BUTTON_OPEN_ALTER_MENU: {
      bool bAreaSelected = m_NoteFieldEdit.m_iBeginMarker != -1 &&
                           m_NoteFieldEdit.m_iEndMarker != -1;
      if (!bAreaSelected) {
        SCREENMAN->SystemMessage(ALTER_MENU_NO_SELECTION);
        SCREENMAN->PlayInvalidSound();
      } else {
        bool isRoutine = (m_InputPlayerNumber != PLAYER_INVALID);
        g_AlterMenu.rows[routine_invert_notes].bEnabled = isRoutine;
        g_AlterMenu.rows[routine_mirror_1_to_2].bEnabled = isRoutine;
        g_AlterMenu.rows[routine_mirror_2_to_1].bEnabled = isRoutine;
        EditMiniMenu(&g_AlterMenu, SM_BackFromAlterMenu);
      }
      return true;
    }
    case EDIT_BUTTON_OPEN_EDIT_MENU:
      EditMiniMenu(&g_MainMenu, SM_BackFromMainMenu);
      return true;
    case EDIT_BUTTON_OPEN_INPUT_HELP:
      DoHelp();
      return true;
    case EDIT_BUTTON_OPEN_TIMING_MENU: {
      DisplayTimingMenu();
      return true;
    }
    case EDIT_BUTTON_OPEN_NEXT_STEPS:
    case EDIT_BUTTON_OPEN_PREV_STEPS: {
      // don't keep undo when changing Steps
      ClearUndo();

      // get the second of the current step.
      float curSecond =
          GetAppropriateTiming().GetElapsedTimeFromBeat(GetBeat());

      // save current steps
      Steps* pSteps = GAMESTATE->m_pCurSteps[PLAYER_1];
      ASSERT(pSteps != nullptr);
      pSteps->SetNoteData(m_NoteDataEdit);

      // Get all Steps of this StepsType
      const StepsType st = pSteps->m_StepsType;
      std::vector<Steps*> vSteps;
      SongUtil::GetSteps(GAMESTATE->m_pCurSong, vSteps, st);

      // Sort them by difficulty.
      StepsUtil::SortStepsByTypeAndDifficulty(vSteps);

      // Find out what index the current Steps are
      std::vector<Steps*>::iterator it =
          find(vSteps.begin(), vSteps.end(), pSteps);
      ASSERT(it != vSteps.end());

      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_OPEN_PREV_STEPS:
          if (it == vSteps.begin()) {
            SCREENMAN->PlayInvalidSound();
            return true;
          }
          it--;
          break;
        case EDIT_BUTTON_OPEN_NEXT_STEPS:
          it++;
          if (it == vSteps.end()) {
            SCREENMAN->PlayInvalidSound();
            return true;
          }
          break;
      }

      pSteps = *it;
      GAMESTATE->m_pCurSteps[PLAYER_1].Set(pSteps);
      m_pSteps = pSteps;
      pSteps->GetNoteData(m_NoteDataEdit);

      std::string s = ssprintf(
          (SWITCHED_TO.GetValue() + " %s %s '%s' (%d of %d)").c_str(),
          GAMEMAN->GetStepsTypeInfo(st).szName,
          DifficultyToString(pSteps->GetDifficulty()).c_str(),
          pSteps->GetChartName().c_str(), it - vSteps.begin() + 1,
          int(vSteps.size()));
      SCREENMAN->SystemMessage(s);
      m_soundSwitchSteps.Play(true);
      // Reload the music because it can be different for every steps. -Kyz
      m_AutoKeysounds.FinishLoading();
      m_pSoundMusic = m_AutoKeysounds.GetSound();

      ScrollTo(GetAppropriateTiming().GetBeatFromElapsedTime(curSecond));
    }
      return true;
    case EDIT_BUTTON_BPM_UP:
    case EDIT_BUTTON_BPM_DOWN: {
      float fBPM = GetAppropriateTiming().GetBPMAtBeat(GetBeat());
      float fDelta;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_BPM_UP:
          fDelta = +0.020f;
          break;
        case EDIT_BUTTON_BPM_DOWN:
          fDelta = -0.020f;
          break;
      }
      if (EditIsBeingPressed(EDIT_BUTTON_ADJUST_FINE)) {
        fDelta /= 20;  // .001 bpm
      } else if (input.type == IET_REPEAT) {
        if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
          fDelta *= 10;
        } else {
          fDelta *= 40;
        }
      }

      float fNewBPM = fBPM + fDelta;
      if (fNewBPM > 0.0f) {
        GetAppropriateTimingForUpdate().AddSegment(
            BPMSegment(GetRow(), fNewBPM));
      }
      (fDelta > 0 ? m_soundValueIncrease : m_soundValueDecrease).Play(true);
      SetDirty(true);
    }
      return true;
    case EDIT_BUTTON_STOP_UP:
    case EDIT_BUTTON_STOP_DOWN: {
      float fDelta;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_STOP_UP:
          fDelta = +0.020f;
          break;
        case EDIT_BUTTON_STOP_DOWN:
          fDelta = -0.020f;
          break;
      }
      if (EditIsBeingPressed(EDIT_BUTTON_ADJUST_FINE)) {
        fDelta /= 20;  // 1ms
      } else if (input.type == IET_REPEAT) {
        if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
          fDelta *= 10;
        } else {
          fDelta *= 40;
        }
      }

      // is there a StopSegment on the current row?
      TimingData& timing = GetAppropriateTimingForUpdate();
      StopSegment* seg = timing.GetStopSegmentAtRow(GetRow());
      int i = timing.GetSegmentIndexAtRow(SEGMENT_STOP, GetRow());
      if (i == -1 || seg->GetRow() != GetRow())  // invalid
      {
        if (fDelta > 0) {
          timing.AddSegment(StopSegment(GetRow(), fDelta));
        } else {
          return false;
        }
      } else {
        std::vector<TimingSegment*>& stops =
            timing.GetTimingSegments(SEGMENT_STOP);
        seg->SetPause(seg->GetPause() + fDelta);
        if (seg->GetPause() <= 0) {
          stops.erase(stops.begin() + i, stops.begin() + i + 1);
        }
      }

      (fDelta > 0 ? m_soundValueIncrease : m_soundValueDecrease).Play(true);
      SetDirty(true);
    }
      return true;
    // TODO: Combine the stop and delay call somehow?
    case EDIT_BUTTON_DELAY_UP:
    case EDIT_BUTTON_DELAY_DOWN: {
      float fDelta;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_DELAY_UP:
          fDelta = +0.020f;
          break;
        case EDIT_BUTTON_DELAY_DOWN:
          fDelta = -0.020f;
          break;
      }
      if (EditIsBeingPressed(EDIT_BUTTON_ADJUST_FINE)) {
        fDelta /= 20;  // 1ms
      } else if (input.type == IET_REPEAT) {
        if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
          fDelta *= 10;
        } else {
          fDelta *= 40;
        }
      }

      // is there a StopSegment on the current row?
      TimingData& timing = GetAppropriateTimingForUpdate();
      DelaySegment* seg = timing.GetDelaySegmentAtRow(GetRow());
      int i = timing.GetSegmentIndexAtRow(SEGMENT_DELAY, GetRow());
      if (i == -1 || seg->GetRow() != GetRow())  // invalid
      {
        if (fDelta > 0) {
          timing.AddSegment(DelaySegment(GetRow(), fDelta));
        } else {
          return false;
        }
      } else {
        std::vector<TimingSegment*>& stops =
            timing.GetTimingSegments(SEGMENT_DELAY);
        seg->SetPause(seg->GetPause() + fDelta);
        if (seg->GetPause() <= 0) {
          stops.erase(stops.begin() + i, stops.begin() + i + 1);
        }
      }

      (fDelta > 0 ? m_soundValueIncrease : m_soundValueDecrease).Play(true);
      SetDirty(true);
    }
      return true;
    case EDIT_BUTTON_OFFSET_UP:
    case EDIT_BUTTON_OFFSET_DOWN: {
      float fDelta;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_OFFSET_DOWN:
          fDelta = -0.02f;
          break;
        case EDIT_BUTTON_OFFSET_UP:
          fDelta = +0.02f;
          break;
      }
      if (EditIsBeingPressed(EDIT_BUTTON_ADJUST_FINE)) {
        fDelta /= 20; /* 1ms */
      } else if (input.type == IET_REPEAT) {
        if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
          fDelta *= 10;
        } else {
          fDelta *= 40;
        }
      }
      GetAppropriateTimingForUpdate().m_fBeat0OffsetInSeconds += fDelta;
      (fDelta > 0 ? m_soundValueIncrease : m_soundValueDecrease).Play(true);
      if (GAMESTATE->m_bIsUsingStepTiming) {
        GAMESTATE->m_pCurSteps[PLAYER_1]->m_Attacks.UpdateStartTimes(fDelta);
      } else {
        GAMESTATE->m_pCurSong->m_Attacks.UpdateStartTimes(fDelta);
        GAMESTATE->m_pCurSong->m_fMusicSampleStartSeconds += fDelta;
      }
      SetDirty(true);
    }
      return true;
    case EDIT_BUTTON_SAMPLE_START_UP:
    case EDIT_BUTTON_SAMPLE_START_DOWN:
    case EDIT_BUTTON_SAMPLE_LENGTH_DOWN:
    case EDIT_BUTTON_SAMPLE_LENGTH_UP: {
      float fDelta;
      switch (EditB) {
        DEFAULT_FAIL(EditB);
        case EDIT_BUTTON_SAMPLE_START_DOWN:
          fDelta = -0.02f;
          break;
        case EDIT_BUTTON_SAMPLE_START_UP:
          fDelta = +0.02f;
          break;
        case EDIT_BUTTON_SAMPLE_LENGTH_DOWN:
          fDelta = -0.02f;
          break;
        case EDIT_BUTTON_SAMPLE_LENGTH_UP:
          fDelta = +0.02f;
          break;
      }

      if (input.type == IET_REPEAT) {
        if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
          fDelta *= 10;
        } else {
          fDelta *= 40;
        }
      }

      if (EditB == EDIT_BUTTON_SAMPLE_LENGTH_DOWN ||
          EditB == EDIT_BUTTON_SAMPLE_LENGTH_UP) {
        m_pSong->m_fMusicSampleLengthSeconds += fDelta;
        m_pSong->m_fMusicSampleLengthSeconds =
            std::max(m_pSong->m_fMusicSampleLengthSeconds, 0.0f);
      } else {
        m_pSong->m_fMusicSampleStartSeconds += fDelta;
        m_pSong->m_fMusicSampleStartSeconds =
            std::max(m_pSong->m_fMusicSampleStartSeconds, 0.0f);
      }
      (fDelta > 0 ? m_soundValueIncrease : m_soundValueDecrease).Play(true);
      SetDirty(true);
    }
      return true;
    case EDIT_BUTTON_PLAY_SAMPLE_MUSIC:
      PlayPreviewMusic();
      return true;
    case EDIT_BUTTON_OPEN_BGCHANGE_LAYER1_MENU:
    case EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU:
      if (!GAMESTATE->m_bIsUsingStepTiming) {
        switch (EditB) {
          DEFAULT_FAIL(EditB);
          case EDIT_BUTTON_OPEN_BGCHANGE_LAYER1_MENU:
            g_CurrentBGChangeLayer = BACKGROUND_LAYER_1;
            break;
          case EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU:
            g_CurrentBGChangeLayer = BACKGROUND_LAYER_2;
            break;
        }

        {
          // Fill in option names
          std::vector<std::string> vThrowAway;

          MenuDef& menu = g_BackgroundChange;

          menu.rows[layer].choices[0] = ssprintf("%d", g_CurrentBGChangeLayer);
          BackgroundUtil::GetBackgroundTransitions(
              "", vThrowAway, menu.rows[transition].choices);
          g_BackgroundChange.rows[transition].choices.insert(
              g_BackgroundChange.rows[transition].choices.begin(),
              "None");  // add "no transition"
          BackgroundUtil::GetBackgroundEffects(
              "", vThrowAway, menu.rows[effect].choices);
          menu.rows[effect].choices.insert(
              menu.rows[effect].choices.begin(),
              "Default");  // add "default effect"

          BackgroundUtil::GetSongBGAnimations(
              m_pSong, "", vThrowAway,
              menu.rows[file1_song_bganimation].choices);
          BackgroundUtil::GetSongMovies(
              m_pSong, "", vThrowAway, menu.rows[file1_song_movie].choices);
          BackgroundUtil::GetSongBitmaps(
              m_pSong, "", vThrowAway, menu.rows[file1_song_still].choices);
          BackgroundUtil::GetGlobalBGAnimations(
              m_pSong, "", vThrowAway,
              menu.rows[file1_global_bganimation]
                  .choices);  // nullptr to get all background files
          BackgroundUtil::GetGlobalRandomMovies(
              m_pSong, "", vThrowAway, menu.rows[file1_global_movie].choices,
              false, false);  // all backgrounds
          BackgroundUtil::GetGlobalRandomMovies(
              m_pSong, "", vThrowAway,
              menu.rows[file1_global_movie_song_group].choices, false,
              true);  // song group's backgrounds
          BackgroundUtil::GetGlobalRandomMovies(
              m_pSong, "", vThrowAway,
              menu.rows[file1_global_movie_song_group_and_genre].choices, true,
              true);  // song group and genre's backgrounds

          menu.rows[file2_type].choices = menu.rows[file1_type].choices;
          menu.rows[file2_song_bganimation].choices =
              menu.rows[file1_song_bganimation].choices;
          menu.rows[file2_song_movie].choices =
              menu.rows[file1_song_movie].choices;
          menu.rows[file2_song_still].choices =
              menu.rows[file1_song_still].choices;
          menu.rows[file2_global_bganimation].choices =
              menu.rows[file1_global_bganimation].choices;
          menu.rows[file2_global_movie].choices =
              menu.rows[file1_global_movie].choices;
          menu.rows[file2_global_movie_song_group].choices =
              menu.rows[file1_global_movie_song_group].choices;
          menu.rows[file2_global_movie_song_group_and_genre].choices =
              menu.rows[file1_global_movie_song_group_and_genre].choices;

          // Fill in lines enabled/disabled
          bool bAlreadyBGChangeHere = false;
          BackgroundChange bgChange;
          for (BackgroundChange& bgc :
               m_pSong->GetBackgroundChanges(g_CurrentBGChangeLayer)) {
            if (bgc.m_fStartBeat == GAMESTATE->m_pPlayerState[main_player_]
                                        ->m_Position.m_fSongBeat) {
              bAlreadyBGChangeHere = true;
              bgChange = bgc;
            }
          }

#define FILL_ENABLED(x) menu.rows[x].bEnabled = menu.rows[x].choices.size() > 0;
          FILL_ENABLED(transition);
          FILL_ENABLED(effect);
          FILL_ENABLED(file1_song_bganimation);
          FILL_ENABLED(file1_song_movie);
          FILL_ENABLED(file1_song_still);
          FILL_ENABLED(file1_global_bganimation);
          FILL_ENABLED(file1_global_movie);
          FILL_ENABLED(file1_global_movie_song_group);
          FILL_ENABLED(file1_global_movie_song_group_and_genre);
          FILL_ENABLED(file2_song_bganimation);
          FILL_ENABLED(file2_song_movie);
          FILL_ENABLED(file2_song_still);
          FILL_ENABLED(file2_global_bganimation);
          FILL_ENABLED(file2_global_movie);
          FILL_ENABLED(file2_global_movie_song_group);
          FILL_ENABLED(file2_global_movie_song_group_and_genre);
#undef FILL_ENABLED
          menu.rows[delete_change].bEnabled = bAlreadyBGChangeHere;

          // set default choices
          menu.rows[rate].SetDefaultChoiceIfPresent(
              ssprintf("%2.0f%%", bgChange.m_fRate * 100));
          menu.rows[transition].SetDefaultChoiceIfPresent(
              bgChange.m_sTransition);
          menu.rows[effect].SetDefaultChoiceIfPresent(bgChange.m_def.m_sEffect);
          menu.rows[file1_type].iDefaultChoice = none;
          if (bgChange.m_def.m_sFile1 == RANDOM_BACKGROUND_FILE) {
            menu.rows[file1_type].iDefaultChoice = dynamic_random;
          }
          if (menu.rows[file1_song_bganimation].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = song_bganimation;
          }
          if (menu.rows[file1_song_movie].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = song_movie;
          }
          if (menu.rows[file1_song_still].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = song_bitmap;
          }
          if (menu.rows[file1_global_bganimation].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = global_bganimation;
          }
          if (menu.rows[file1_global_movie].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = global_movie;
          }
          if (menu.rows[file1_global_movie_song_group].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice = global_movie_song_group;
          }
          if (menu.rows[file1_global_movie_song_group_and_genre]
                  .SetDefaultChoiceIfPresent(bgChange.m_def.m_sFile1)) {
            menu.rows[file1_type].iDefaultChoice =
                global_movie_song_group_and_genre;
          }

          menu.rows[file2_type].iDefaultChoice = none;
          if (bgChange.m_def.m_sFile2 == RANDOM_BACKGROUND_FILE) {
            menu.rows[file2_type].iDefaultChoice = dynamic_random;
          }
          if (menu.rows[file2_song_bganimation].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = song_bganimation;
          }
          if (menu.rows[file2_song_movie].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = song_movie;
          }
          if (menu.rows[file2_song_still].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = song_bitmap;
          }
          if (menu.rows[file2_global_bganimation].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = global_bganimation;
          }
          if (menu.rows[file2_global_movie].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = global_movie;
          }
          if (menu.rows[file2_global_movie_song_group].SetDefaultChoiceIfPresent(
                  bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice = global_movie_song_group;
          }
          if (menu.rows[file2_global_movie_song_group_and_genre]
                  .SetDefaultChoiceIfPresent(bgChange.m_def.m_sFile2)) {
            menu.rows[file2_type].iDefaultChoice =
                global_movie_song_group_and_genre;
          }

        }
        EditMiniMenu(&g_BackgroundChange, SM_BackFromBGChange);
      } else {
        SCREENMAN->SystemMessage(BG_CHANGE_STEP_TIMING);
        SCREENMAN->PlayInvalidSound();
      }
      return true;
    case EDIT_BUTTON_INSERT_ATTACK:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }
      EditMiniMenu(&g_InsertCourseAttack, SM_BackFromInsertCourseAttack);
      return true;
    case EDIT_BUTTON_ADD_COURSE_MODS:
      EditMiniMenu(&g_CourseMode, SM_BackFromCourseModeMenu);
      return true;
    case EDIT_BUTTON_ADD_STEP_MODS:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }
      g_iLastInsertTapAttackTrack = -1;
      g_fLastInsertAttackDurationSeconds = -1;
      g_fLastInsertAttackPositionSeconds = -1;
      EditMiniMenu(&g_InsertStepAttack, SM_BackFromInsertStepAttack);
      return true;
    case EDIT_BUTTON_OPEN_STEP_ATTACK_MENU:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }
      EditMiniMenu(&g_InsertTapAttack, SM_BackFromInsertTapAttack);
      return true;
    case EDIT_BUTTON_OPEN_COURSE_ATTACK_MENU:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }
      EditMiniMenu(&g_InsertCourseAttack, SM_BackFromInsertCourseAttack);
      return true;
    case EDIT_BUTTON_PLAY_FROM_START:
      m_iStartPlayingAt = 0;
      m_iStopPlayingAt = BeatToNoteRow(m_pSong->GetLastBeat());
      TransitionEditState(STATE_PLAYING);
      return true;
    case EDIT_BUTTON_PLAY_FROM_CURSOR:
      m_iStartPlayingAt = BeatToNoteRow(GetBeat());
      m_iStopPlayingAt = BeatToNoteRow(m_pSong->GetLastBeat());
      TransitionEditState(STATE_PLAYING);
      return true;
    case EDIT_BUTTON_PLAY_SELECTION:
      // play selection
      if (m_NoteFieldEdit.m_iBeginMarker == -1 ||
          m_NoteFieldEdit.m_iEndMarker == -1) {
        m_iStartPlayingAt = 0;
        m_iStopPlayingAt = BeatToNoteRow(m_pSong->GetLastBeat());
      } else {
        m_iStartPlayingAt = m_NoteFieldEdit.m_iBeginMarker;
        m_iStopPlayingAt = m_NoteFieldEdit.m_iEndMarker;
      }
      TransitionEditState(STATE_PLAYING);
      return true;
    case EDIT_BUTTON_RECORD_SELECTION:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }

      // Only fill m_NoteDataRecord once per selection.
      // To replace the previous contents, copy m_NoteDataEdit onto
      // m_NoteDataRecord only if they haven't already been set to
      // the desired range.
      if (m_NoteFieldRecord.m_iBeginMarker != m_NoteFieldEdit.m_iBeginMarker ||
          m_NoteFieldRecord.m_iEndMarker != m_NoteFieldEdit.m_iEndMarker) {
        m_NoteDataRecord.CopyAll(m_NoteDataEdit);
      }

      if (m_NoteFieldEdit.m_iBeginMarker == -1 ||
          m_NoteFieldEdit.m_iEndMarker == -1) {
        m_iStartPlayingAt = BeatToNoteRow(GetBeat());
        m_iStopPlayingAt = m_iStartPlayingAt +
                           BeatToNoteRow(
                               NoteTypeToBeat(m_SnapDisplay.GetNoteType()));
      } else {
        m_iStartPlayingAt = m_NoteFieldEdit.m_iBeginMarker;
        m_iStopPlayingAt = m_NoteFieldEdit.m_iEndMarker;
      }

      m_bReturnToRecordMenuAfterPlay = false;
      // don't allow recording on top of what the user already did.
      m_NoteDataRecord.ClearRange(m_iStartPlayingAt, m_iStopPlayingAt);

      TransitionEditState(STATE_RECORDING);
      return true;
    case EDIT_BUTTON_RECORD_FROM_CURSOR:
      if (GAMESTATE->m_bIsUsingStepTiming) {
        return false;  // this is only supported in song timing
      }

      m_iStartPlayingAt = BeatToNoteRow(GetBeat());
      m_iStopPlayingAt = m_iStartPlayingAt + 4 * ROWS_PER_BEAT;

      /* Step to the last beat in the chart.  This still isn't perfect; for a
       * dense chart at the end of the song, we'll stop early.  We should aim
       * for the stop time of the chart. */
      m_iStopPlayingAt =
          std::max(m_iStartPlayingAt, m_NoteDataEdit.GetLastRow());
      TransitionEditState(STATE_RECORDING);
      return true;

    case EDIT_BUTTON_INSERT:
      HandleAreaMenuChoice(insert_and_shift);
      SCREENMAN->PlayInvalidSound();
      return true;

    case EDIT_BUTTON_INSERT_SHIFT_PAUSES:
      HandleAreaMenuChoice(shift_pauses_forward);
      SCREENMAN->PlayInvalidSound();
      return true;

    case EDIT_BUTTON_DELETE:
      HandleAreaMenuChoice(delete_and_shift);
      SCREENMAN->PlayInvalidSound();
      return true;

    case EDIT_BUTTON_DELETE_SHIFT_PAUSES:
      HandleAreaMenuChoice(shift_pauses_backward);
      SCREENMAN->PlayInvalidSound();
      return true;

    case EDIT_BUTTON_RECORD_HOLD_LESS:
      record_hold_seconds -= .01f;
      if (record_hold_seconds <= 0.0f) {
        record_hold_seconds = 0.0f;
      }
      return true;

    case EDIT_BUTTON_RECORD_HOLD_MORE:
      record_hold_seconds += .01f;
      return true;

    case EDIT_BUTTON_RECORD_HOLD_RESET:
      record_hold_seconds = record_hold_default;
      return true;

    case EDIT_BUTTON_RECORD_HOLD_OFF:
      record_hold_seconds = 120.0f;
      return true;

    case EDIT_BUTTON_SAVE:
      HandleMainMenuChoice(ScreenEdit::save);
      return true;

    case EDIT_BUTTON_UNDO:
      Undo();
      return true;

    case EDIT_BUTTON_SWITCH_PLAYERS:
      if (m_InputPlayerNumber == PLAYER_INVALID) {
        return false;
      }
      enum_add(m_InputPlayerNumber, 1);
      if (m_InputPlayerNumber == NUM_PLAYERS) {
        m_InputPlayerNumber = PLAYER_1;
      }
      m_soundSwitchPlayer.Play(true);
      return true;

    case EDIT_BUTTON_SWITCH_TIMINGS:
      GAMESTATE->m_bIsUsingStepTiming = !GAMESTATE->m_bIsUsingStepTiming;
      m_soundSwitchTiming.Play(true);
      return true;
    default:
      return false;
  }
}

bool ScreenEdit::InputRecord(const InputEventPlus& input, EditButton EditB) {
  if (input.type == IET_FIRST_PRESS && EditB == EDIT_BUTTON_RETURN_TO_EDIT) {
    TransitionEditState(STATE_EDITING);
    return true;
  }

  if (input.pn != PLAYER_1) {
    return false;  // ignore
  }

  const int iCol =
      GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
          ->GameInputToColumn(input.GameI);

  // Is this actually a column? If not, ignore the input.
  if (iCol == -1) {
    return false;
  }

  switch (input.type) {
    DEFAULT_FAIL(input.type);
    case IET_FIRST_PRESS: {
      if (EditIsBeingPressed(EDIT_BUTTON_REMOVE_NOTE)) {
        // Remove notes in Update.
        return false;
      }

      // Add a tap
      float fBeat = GetBeat();
      fBeat = Quantize(fBeat, NoteTypeToBeat(m_SnapDisplay.GetNoteType()));

      const int iRow = BeatToNoteRow(fBeat);
      if (iRow < 0) {
        return false;
      }

      // Don't add outside of the range.
      if (iRow < m_iStartPlayingAt || iRow >= m_iStopPlayingAt) {
        return false;
      }

      // Remove hold if any so that we don't have taps inside of a hold.
      int iHeadRow;
      if (m_NoteDataRecord.IsHoldNoteAtRow(iCol, iRow, &iHeadRow)) {
        m_NoteDataRecord.SetTapNote(iCol, iHeadRow, TAP_EMPTY);
      }

      TapNote tn = TAP_ORIGINAL_TAP;
      tn.pn = m_InputPlayerNumber;
      m_NoteDataRecord.SetTapNote(iCol, iRow, tn);
      m_NoteFieldRecord.Step(iCol, TNS_W1);
    }
      return true;
    case IET_REPEAT:
    case IET_RELEASE:
      // don't add or extend holds here; we do it in Update()
      return false;
  }
}

bool ScreenEdit::InputRecordPaused(
    const InputEventPlus& input, EditButton EditB) {
  if (input.type != IET_FIRST_PRESS) {
    return false;  // don't care
  }

  switch (EditB) {
    case EDIT_BUTTON_UNDO:
      /* We've already actually committed changes to m_NoteDataEdit, so all we
       * have to do to undo is Undo() as usual, and copy the note data back in.
       */
      Undo();
      m_NoteDataRecord.CopyAll(m_NoteDataEdit);
      return true;

    case EDIT_BUTTON_PLAY_SELECTION:
      TransitionEditState(STATE_PLAYING);
      return true;

    case EDIT_BUTTON_RECORD_SELECTION:
      TransitionEditState(STATE_RECORDING);
      return true;

    case EDIT_BUTTON_RECORD_FROM_CURSOR:
      if (GAMESTATE->m_pCurSteps[0]->IsAnEdit()) {
        float fMaximumBeat = GetMaximumBeatForNewNote();
        if (NoteRowToBeat(m_iStopPlayingAt) >= fMaximumBeat) {
          // We're already at the end.
          SCREENMAN->PlayInvalidSound();
          return true;
        }
      }

      // Pick up where we left off.
      {
        int iSize = m_iStopPlayingAt - m_iStartPlayingAt;
        m_iStartPlayingAt = m_iStopPlayingAt;
        m_iStopPlayingAt += iSize;

        if (GAMESTATE->m_pCurSteps[0]->IsAnEdit()) {
          m_iStopPlayingAt = std::min(
              m_iStopPlayingAt, BeatToNoteRow(GetMaximumBeatForNewNote()));
        }
      }

      TransitionEditState(STATE_RECORDING);
      return true;

    case EDIT_BUTTON_RETURN_TO_EDIT:
      TransitionEditState(STATE_EDITING);
      return true;
    default:
      return false;
  }
}

bool ScreenEdit::InputPlay(const InputEventPlus& input, EditButton EditB) {
  switch (input.type) {
    case IET_RELEASE:
    case IET_FIRST_PRESS:
      break;
    default:
      return false;
  }

  GameButtonType gbt =
      GAMESTATE->m_pCurGame->GetPerButtonInfo(input.GameI.button)->m_gbt;
  if (player_manager_.HandleGameplayInput(input, gbt)) {
    return true;
  };

  if (gbt == GameButtonType_Menu && input.type == IET_FIRST_PRESS) {
    switch (EditB) {
      case EDIT_BUTTON_RETURN_TO_EDIT:
        /* When exiting play mode manually, leave the cursor where it is. */
        m_fBeatToReturnTo = GetAppropriatePosition().m_fSongBeat;
        TransitionEditState(STATE_EDITING);
        return true;
      case EDIT_BUTTON_OFFSET_UP:
      case EDIT_BUTTON_OFFSET_DOWN: {
        float fOffsetDelta;
        switch (EditB) {
          DEFAULT_FAIL(EditB);
          case EDIT_BUTTON_OFFSET_DOWN:
            fOffsetDelta = -0.020f;
            break;
          case EDIT_BUTTON_OFFSET_UP:
            fOffsetDelta = +0.020f;
            break;
        }

        if (EditIsBeingPressed(EDIT_BUTTON_ADJUST_FINE)) {
          fOffsetDelta /= 20; /* 1ms */
        } else if (input.type == IET_REPEAT) {
          if (INPUTFILTER->GetSecsHeld(input.DeviceI) < 1.0f) {
            fOffsetDelta *= 10;
          } else {
            fOffsetDelta *= 40;
          }
        }

        GetAppropriateTimingForUpdate().m_fBeat0OffsetInSeconds += fOffsetDelta;
        if (!GAMESTATE->m_bIsUsingStepTiming) {
          GAMESTATE->m_pCurSong->m_fMusicSampleStartSeconds += fOffsetDelta;
        }
      }
        return true;
      default:
        break;
    }
  }
  return false;
}
