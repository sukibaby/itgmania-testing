#include "global.h"
#include "ScreenEdit.h"

#include "AdjustSync.h"
#include "ArrowEffects.h"
#include "GameSoundManager.h"
#include "GameState.h"
#include "InputMapper.h"
#include "NoteData.h"
#include "NoteDataUtil.h"
#include "PrefsManager.h"
#include "Preference.h"
#include "RageLog.h"
#include "RageSound.h"
#include "RageTimer.h"
#include "ScreenMiniMenu.h"
#include "ScreenSaveSync.h"
#include "Song.h"
#include "Steps.h"
#include "Style.h"
#include "ThemeManager.h"
#include "ThemeMetric.h"

extern float record_hold_seconds;
extern Preference<bool> g_bEditorShowBGChangesPlay;
extern ThemeMetric<bool> LOOP_ON_CHART_END;

static ThemeMetric<float> FADE_IN_PREVIEW("ScreenEdit", "FadeInPreview");
static ThemeMetric<float> FADE_OUT_PREVIEW("ScreenEdit", "FadeOutPreview");

ScreenEdit::~ScreenEdit() {
  // UGLY: Don't delete the Song's steps.
  m_SongLastSave.DetachSteps();

  LOG->Trace("ScreenEdit::~ScreenEdit()");
  m_pSoundMusic->StopPlaying();

  // Go back to Step Timing on leave.
  GAMESTATE->m_bIsUsingStepTiming = true;
  // DEFINITELY reset the InStepEditor variable.
  GAMESTATE->m_bInStepEditor = false;
}

void ScreenEdit::BeginScreen() {
  ScreenWithMenuElements::BeginScreen();

  /* We do this ourself. */
  SOUND->HandleSongTimer(false);
}

void ScreenEdit::EndScreen() {
  ScreenWithMenuElements::EndScreen();

  SOUND->HandleSongTimer(true);
}

// play assist ticks
void ScreenEdit::PlayTicks() {
  if (m_EditState != STATE_PLAYING) {
    return;
  }
  player_manager_.PlayTicks(m_GameplayAssist);
}

void ScreenEdit::PlayPreviewMusic() {
  SOUND->StopMusic();
  SOUND->PlayMusic(
      m_pSong->GetPreviewMusicPath(), nullptr, false,
      m_pSong->GetPreviewStartSeconds(), m_pSong->m_fMusicSampleLengthSeconds,
      FADE_IN_PREVIEW, FADE_OUT_PREVIEW);
}

void ScreenEdit::MakeFilteredMenuDef(const MenuDef* pDef, MenuDef& menu) {
  menu = *pDef;
  menu.rows.clear();

  std::vector<MenuRowDef> aRows;
  for (const MenuRowDef& r : pDef->rows) {
    // Don't add rows that aren't applicable to this edit mode.
    if (EDIT_MODE >= r.emShowIn) {
      menu.rows.push_back(r);
    }
  }
}

void ScreenEdit::EditMiniMenu(
    const MenuDef* pDef, ScreenMessage SM_SendOnOK,
    ScreenMessage SM_SendOnCancel) {
  // Reload options.
  MenuDef menu("");
  MakeFilteredMenuDef(pDef, menu);
  ScreenMiniMenu::MiniMenu(&menu, SM_SendOnOK, SM_SendOnCancel);
}

void ScreenEdit::Update(float fDeltaTime) {
  m_PlayerStateEdit.Update(fDeltaTime);

  const float fRate = PREFSMAN->m_bRateModsAffectTweens
                          ? GAMESTATE->m_SongOptions.GetCurrent().m_fMusicRate
                          : 1.0f;

  if (m_pSoundMusic->IsPlaying()) {
    RageTimer tm;
    const float fSeconds = m_pSoundMusic->GetPositionSeconds(&tm);
    GAMESTATE->UpdateSongPosition(
        fSeconds, GAMESTATE->m_pCurSong->m_SongTiming, tm);
  }

  if (m_EditState == STATE_EDITING) {
    if (IsDirty() && m_next_autosave_time > -1.0f &&
        RageTimer::GetTimeSinceStart() > m_next_autosave_time) {
      PerformSave(true);
    }
  }

  if (m_EditState == STATE_RECORDING) {
    // TODO: Find a way to prevent STATE_RECORDING when in Song Timing.

    for (int t = 0;
         t < GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
                 ->m_iColsPerPlayer;
         t++)  // for each track
    {
      std::vector<GameInput> GameI;
      GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
          ->StyleInputToGameInput(t, PLAYER_1, GameI);
      float fSecsHeld = 0.0f;
      for (size_t i = 0; i < GameI.size(); ++i) {
        fSecsHeld = std::max(fSecsHeld, INPUTMAPPER->GetSecsHeld(GameI[i]));
      }
      fSecsHeld = std::min(fSecsHeld, m_RemoveNoteButtonLastChanged.Ago());
      if (fSecsHeld == 0) {
        continue;
      }

      // TODO: Currently, this will ignore a hold note that's
      // held past the end of selection.  Ideally the hold
      // note should be created, ending right at the end of
      // selection (or appropriate quantization).
      float fStartPlayingAtBeat = NoteRowToBeat(m_iStartPlayingAt);
      float fStopPlayingAtBeat = NoteRowToBeat(m_iStopPlayingAt);
      if (GAMESTATE->m_pPlayerState[main_player_]->m_Position.m_fSongBeat <=
              fStartPlayingAtBeat ||
          GAMESTATE->m_pPlayerState[main_player_]->m_Position.m_fSongBeat >=
              fStopPlayingAtBeat) {
        continue;
      }

      float fStartedHoldingSeconds =
          m_pSoundMusic->GetPositionSeconds() - fSecsHeld;
      float fStartBeat = std::max(
          fStartPlayingAtBeat,
          m_pSteps->GetTimingData()->GetBeatFromElapsedTime(
              fStartedHoldingSeconds));
      float fEndBeat = std::max(fStartBeat, GetBeat());
      fEndBeat = std::min(fEndBeat, fStopPlayingAtBeat);

      // Round start and end to the nearest snap interval
      fStartBeat =
          Quantize(fStartBeat, NoteTypeToBeat(m_SnapDisplay.GetNoteType()));
      fEndBeat =
          Quantize(fEndBeat, NoteTypeToBeat(m_SnapDisplay.GetNoteType()));

      if (m_bRemoveNoteButtonDown) {
        m_NoteDataRecord.ClearRangeForTrack(
            BeatToNoteRow(fStartBeat), BeatToNoteRow(fEndBeat), t);
      } else if (fSecsHeld > record_hold_seconds) {
        // create or extend a hold or roll note
        TapNote tn = EditIsBeingPressed(EDIT_BUTTON_LAY_ROLL)
                         ? TAP_ORIGINAL_ROLL_HEAD
                         : TAP_ORIGINAL_HOLD_HEAD;

        tn.pn = m_InputPlayerNumber;
        m_NoteDataRecord.AddHoldNote(
            t, BeatToNoteRow(fStartBeat), BeatToNoteRow(fEndBeat), tn);
      }
    }
  }

  //
  // check for end of playback/record
  //
  if (m_EditState == STATE_RECORDING || m_EditState == STATE_PLAYING) {
    ArrowEffects::Update();
    /*
     * If any arrow is being held, continue for up to half a second after
     * the end marker.  This makes it possible to start a hold note near
     * the end of the range.  We won't allow placing anything outside of the
     * range.
     */
    bool bButtonIsBeingPressed = false;
    for (int t = 0;
         t < GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
                 ->m_iColsPerPlayer;
         t++)  // for each track
    {
      std::vector<GameInput> GameI;
      GAMESTATE->GetCurrentStyle(GAMESTATE->GetMasterPlayerNumber())
          ->StyleInputToGameInput(t, PLAYER_1, GameI);
      if (INPUTMAPPER->IsBeingPressed(GameI)) {
        bButtonIsBeingPressed = true;
      }
    }

    float fLastBeat = NoteRowToBeat(m_iStopPlayingAt);
    if (bButtonIsBeingPressed && m_EditState == STATE_RECORDING) {
      float fSeconds =
          m_pSteps->GetTimingData()->GetElapsedTimeFromBeat(fLastBeat);
      fLastBeat =
          m_pSteps->GetTimingData()->GetBeatFromElapsedTime(fSeconds + 0.5f);
    }

    float fStopAtSeconds = m_pSteps->GetTimingData()->GetElapsedTimeFromBeat(
                               NoteRowToBeat(m_iStopPlayingAt)) +
                           1;
    if (GAMESTATE->m_pPlayerState[main_player_]->m_Position.m_fMusicSeconds >
        fStopAtSeconds) {
      TransitionEditState((LOOP_ON_CHART_END ? STATE_PLAYING : STATE_EDITING));
    }
  }

  // LOG->Trace( "ScreenEdit::Update(%f)", fDeltaTime );
  if (m_EditState == STATE_PLAYING) {
    ScreenWithMenuElements::Update(fDeltaTime * fRate);
  } else {
    ScreenWithMenuElements::Update(fDeltaTime);
  }

  // Update trailing beat
  float fDelta = GetBeat() - m_fTrailingBeat;
  if (std::abs(fDelta) < 10) {
    fapproach(
        m_fTrailingBeat, GetBeat(),
        fDeltaTime * 40 /
            m_NoteFieldEdit.GetPlayerState()
                ->m_PlayerOptions.GetCurrent()
                .m_fScrollSpeed);
  } else {
    fapproach(m_fTrailingBeat, GetBeat(), std::abs(fDelta) * fDeltaTime * 5);
  }

  PlayTicks();
}

void ScreenEdit::TransitionEditState(EditState em) {
  EditState old = m_EditState;

  // If we're going from recording to paused, come back when we're done.
  if (old == STATE_RECORDING_PAUSED && em == STATE_PLAYING) {
    m_bReturnToRecordMenuAfterPlay = true;
  }

  const bool bStateChanging = em != old;

#if 0
	// If switching out of record, open the menu.
	{
		bool bGoToRecordMenu = (old == STATE_RECORDING);
		if( m_bReturnToRecordMenuAfterPlay && old == STATE_PLAYING )
		{
			bGoToRecordMenu = true;
			m_bReturnToRecordMenuAfterPlay = false;
		}

		if( bGoToRecordMenu )
			em = STATE_RECORDING_PAUSED;
	}
#endif

  // If we're playing music or assist ticks when changing modes, stop.
  SOUND->StopMusic();
  if (m_pSoundMusic) {
    m_pSoundMusic->StopPlaying();
  }
  m_GameplayAssist.StopPlaying();
  GAMESTATE->m_bGameplayLeadIn.Set(true);

  if (bStateChanging) {
    switch (old) {
      case STATE_EDITING:
        // If exiting EDIT mode, save the cursor position.
        m_fBeatToReturnTo = GetAppropriatePosition().m_fSongBeat;
        break;

      case STATE_PLAYING:
        AdjustSync::HandleSongEnd();
        if (!GAMESTATE->m_bIsUsingStepTiming) {
          GAMESTATE->m_pCurSteps[main_player_]->m_Timing = backupStepTiming;
        }
        if (AdjustSync::IsSyncDataChanged()) {
          ScreenSaveSync::PromptSaveSync();
        }
        break;

      case STATE_RECORDING:
        SetDirty(true);
        if (!GAMESTATE->m_bIsUsingStepTiming) {
          GAMESTATE->m_pCurSteps[PLAYER_1]->m_Timing = backupStepTiming;
        }
        SaveUndo();

        // delete old TapNotes in the range
        m_NoteDataEdit.ClearRange(m_iStartPlayingAt, m_iStopPlayingAt);
        m_NoteDataEdit.CopyRange(
            m_NoteDataRecord, m_iStartPlayingAt, m_iStopPlayingAt,
            m_iStartPlayingAt);
        m_NoteDataRecord.ClearAll();

        CheckNumberOfNotesAndUndo();
        break;
      default:
        break;
    }
  }

  // Set up player options for this mode. (EDITING uses m_PlayerStateEdit,
  // which we don't need to change.)
  if (em != STATE_EDITING) {
    // Stop displaying course attacks, if any.
    FOREACH_EnabledPlayer(pn) {
      GAMESTATE->m_pPlayerState[pn]->RemoveActiveAttacks();
      // Load the player's default PlayerOptions.
      GAMESTATE->m_pPlayerState[pn]->RebuildPlayerOptionsFromActiveAttacks();

      // Snap to current options.
      GAMESTATE->m_pPlayerState[pn]->m_PlayerOptions.SetCurrentToLevel(
          ModsLevel_Stage);
    }
  }

  switch (em) {
    DEFAULT_FAIL(em);
    case STATE_EDITING:
      // Important: people will stop playing, change the BG and start again;
      // make sure we reload
      m_Background.Unload();
      m_Foreground.Unload();

      // Restore the cursor position + Quantize + Clamp
      SetBeat(
          std::max(
              0.0f, Quantize(
                        m_fBeatToReturnTo,
                        NoteTypeToBeat(m_SnapDisplay.GetNoteType()))));
      GAMESTATE->m_bInStepEditor = true;
      break;

    case STATE_PLAYING:
    case STATE_RECORDING: {
      m_NoteDataEdit.RevalidateATIs(std::vector<int>(), false);
      if (bStateChanging) {
        AdjustSync::ResetOriginalSyncData();
      }

      /* Give a lead-in.  If we're loading Player, this must be done first.
       * Also be sure to get the right timing. */
      float fSeconds = GetAppropriateTiming().GetElapsedTimeFromBeat(
                           NoteRowToBeat(m_iStartPlayingAt)) -
                       PREFSMAN->m_EditRecordModeLeadIn;
      GAMESTATE->UpdateSongPosition(
          fSeconds, GetAppropriateTiming(), RageZeroTimer);

      GAMESTATE->m_bGameplayLeadIn.Set(false);

      if (!GAMESTATE->m_bIsUsingStepTiming) {
        // Substitute the song timing for the step timing during
        // preview if we're in song mode
        backupStepTiming = GAMESTATE->m_pCurSteps[main_player_]->m_Timing;
        GAMESTATE->m_pCurSteps[main_player_]->m_Timing.Clear();
      }

      /* Reset the note skin, in case preferences have changed. */
      // XXX
      // GAMESTATE->ResetNoteSkins();
      // GAMESTATE->res
      GAMESTATE->m_bInStepEditor = false;
      break;
    }
    case STATE_RECORDING_PAUSED:
      GAMESTATE->m_bInStepEditor = false;
      break;
  }

  switch (em) {
    case STATE_PLAYING:
      // If we're in course display mode, set that up.
      SetupCourseAttacks();
      player_manager_.SetupAutoplay();
      player_manager_.ReloadNoteData(m_NoteDataEdit);

      if (g_bEditorShowBGChangesPlay) {
        /* FirstBeat affects backgrounds, so commit changes to memory (not to
         * disk) and recalc it. */
        Steps* pSteps = GAMESTATE->m_pCurSteps[main_player_];
        ASSERT(pSteps != nullptr);
        pSteps->SetNoteData(m_NoteDataEdit);
        m_pSong->ReCalculateStepStatsAndLastSecond();

        // TODO: Background videos don't support seeking, when they do, make
        // sure to load the appropriate part of the video.
        m_Background.Unload();
        m_Background.LoadFromSong(m_pSong);

        m_Foreground.Unload();
        m_Foreground.LoadFromSong(m_pSong);
      }

      break;
    case STATE_RECORDING:
    case STATE_RECORDING_PAUSED:
      // initialize m_NoteFieldRecord
      m_NoteDataRecord.CopyAll(m_NoteDataEdit);

      // highlight the section being recorded
      m_NoteFieldRecord.m_iBeginMarker = m_iStartPlayingAt;
      m_NoteFieldRecord.m_iEndMarker = m_iStopPlayingAt;

      break;
    default:
      break;
  }

  // Show/hide depending on edit state (em)
  m_sprOverlay->PlayCommand(EditStateToString(em));
  m_sprUnderlay->PlayCommand(EditStateToString(em));

  m_Background.SetVisible(g_bEditorShowBGChangesPlay && em != STATE_EDITING);
  m_textInputTips.SetVisible(em == STATE_EDITING);
  m_textInfo.SetVisible(em == STATE_EDITING);
  // Play the OnCommands again so that these will be re-hidden if the OnCommand
  // hides them.
  if (em == STATE_EDITING) {
    m_textInputTips.PlayCommand("On");
    m_textInfo.PlayCommand("On");
  }
  m_textPlayRecordHelp.SetVisible(em != STATE_EDITING);
  m_SnapDisplay.SetVisible(em == STATE_EDITING);
  m_NoteFieldEdit.SetVisible(em == STATE_EDITING);
  m_NoteFieldRecord.SetVisible(
      em == STATE_RECORDING || em == STATE_RECORDING_PAUSED);

  player_manager_.SetVisible(em == STATE_PLAYING);
  m_Foreground.SetVisible(g_bEditorShowBGChangesPlay && em != STATE_EDITING);

  switch (em) {
    case STATE_PLAYING:
    case STATE_RECORDING: {
      const float fStartSeconds =
          GetAppropriateTiming().GetElapsedTimeFromBeat(GetBeat());
      LOG->Trace("Starting playback at %f", fStartSeconds);

      RageSoundParams p;
      p.m_fSpeed = GAMESTATE->m_SongOptions.GetCurrent().m_fMusicRate;
      p.m_StartSecond = fStartSeconds;
      p.StopMode = RageSoundParams::M_CONTINUE;
      m_pSoundMusic->SetProperty("AccurateSync", true);
      m_pSoundMusic->Play(false, &p);
      break;
    }
    default:
      break;
  }

  m_EditState = em;
}

void ScreenEdit::DrawPrimitives() {
  if (m_pSoundMusic->IsPlaying()) {
    ScreenWithMenuElements::DrawPrimitives();
    return;
  }

  // HACK:  Draw using the trailing beat
  PlayerState* pPlayerState =
      const_cast<PlayerState*>(m_NoteFieldEdit.GetPlayerState());

  float fPlayerSongBeat =
      pPlayerState->m_Position.m_fSongBeat;  // save song beat
  float fPlayerSongBeatNoOffset = pPlayerState->m_Position.m_fSongBeatNoOffset;
  float fPlayerSongBeatVisible = pPlayerState->m_Position.m_fSongBeatVisible;

  float fGameSongBeat = GAMESTATE->m_Position.m_fSongBeat;  // save song beat
  float fGameSongBeatNoOffset = GAMESTATE->m_Position.m_fSongBeatNoOffset;
  float fGameSongBeatVisible = GAMESTATE->m_Position.m_fSongBeatVisible;

  pPlayerState->m_Position.m_fSongBeat =
      m_fTrailingBeat;  // put trailing beat in effect
  pPlayerState->m_Position.m_fSongBeatNoOffset =
      m_fTrailingBeat;  // put trailing beat in effect
  pPlayerState->m_Position.m_fSongBeatVisible =
      m_fTrailingBeat;  // put trailing beat in effect

  GAMESTATE->m_Position.m_fSongBeat =
      m_fTrailingBeat;  // put trailing beat in effect
  GAMESTATE->m_Position.m_fSongBeatNoOffset =
      m_fTrailingBeat;  // put trailing beat in effect
  GAMESTATE->m_Position.m_fSongBeatVisible =
      m_fTrailingBeat;  // put trailing beat in effect

  ScreenWithMenuElements::DrawPrimitives();

  pPlayerState->m_Position.m_fSongBeat =
      fPlayerSongBeat;  // restore real song beat
  pPlayerState->m_Position.m_fSongBeatNoOffset = fPlayerSongBeatNoOffset;
  pPlayerState->m_Position.m_fSongBeatVisible = fPlayerSongBeatVisible;

  GAMESTATE->m_Position.m_fSongBeat = fGameSongBeat;  // restore real song beat
  GAMESTATE->m_Position.m_fSongBeatNoOffset = fGameSongBeatNoOffset;
  GAMESTATE->m_Position.m_fSongBeatVisible = fGameSongBeatVisible;
}
