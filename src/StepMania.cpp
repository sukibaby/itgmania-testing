// StepMania.cpp - Game facing utilities, screenshots, input handling, etc.

#include "StepMania.h"

#include <cstdlib>
#include <utility>

#include "DateTime.h"
#include "EnumHelper.h"
#include "GameConstantsAndTypes.h"
#include "GameInput.h"
#include "PlayerNumber.h"
#include "Preference.h"
#include "RageException.h"
#include "RageInputDevice.h"
#include "RageUtil.h"
#include "StdString.h"
#include "ThemeMetric.h"
#include "global.h"

// Rage global classes
#include "CodeDetector.h"
#include "CommandLineActions.h"
#include "CommonMetrics.h"
#include "Game.h"
#include "GameSoundManager.h"
#include "InputEventPlus.h"
#include "LocalizedString.h"
#include "ProductInfo.h"
#include "RageDisplay.h"
#include "RageInput.h"
#include "RageLog.h"
#include "RageSoundManager.h"
#include "RageSurface.h"
#include "RageSurface_Load.h"
#include "RageTextureManager.h"
#include "RageThreads.h"
#include "RageTimer.h"
#include "RageUtil/Regex.h"
#include "Screen.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "arch/Dialog/Dialog.h"
#include "arch/LoadingWindow/LoadingWindow.h"

#if !defined(SUPPORT_OPENGL) && !defined(SUPPORT_D3D)
#define SUPPORT_OPENGL
#endif

// StepMania global classes
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "ActorUtil.h"
#include "AnnouncerManager.h"
#include "Bookkeeper.h"
#include "CharacterManager.h"
#include "CryptManager.h"
#include "FontManager.h"
#include "GameLoop.h"
#include "GameManager.h"
#include "GameState.h"
#include "ImageCache.h"
#include "InputFilter.h"
#include "InputMapper.h"
#include "InputQueue.h"
#include "LightsManager.h"
#include "LuaDebugManager.h"
#include "LuaManager.h"
#include "MemoryCardManager.h"
#include "MessageManager.h"
#include "ModelManager.h"
#include "NetworkManager.h"
#include "NoteSkinManager.h"
#include "PrefsManager.h"
#include "Profile.h"
#include "ProfileManager.h"
#include "RageFileManager.h"
#include "ScreenManager.h"
#include "SongCacheIndex.h"
#include "SongManager.h"
#include "SpecialFiles.h"
#include "StatsManager.h"
#include "ThemeManager.h"
#include "UnlockManager.h"
#include "ver.h"

bool HandleGlobalInputs(const InputEventPlus& input);
void HandleInputEvents(float fDeltaTime);

void StepMania::ResetGame() {
  GAMESTATE->Reset();

  if (!THEME->DoesThemeExist(THEME->GetCurThemeName())) {
    std::string sGameName = GAMESTATE->GetCurrentGame()->m_szName;
    if (!THEME->DoesThemeExist(sGameName)) {
      sGameName = PREFSMAN->m_sDefaultTheme;  // was previously "default" -aj
    }
    THEME->SwitchThemeAndLanguage(
        sGameName, THEME->GetCurLanguage(), PREFSMAN->m_bPseudoLocalize);
    TEXTUREMAN->DoDelayedDelete();
  }

  PREFSMAN->SavePrefsToDisk();
}

ThemeMetric<std::string> INITIAL_SCREEN("Common", "InitialScreen");
std::string StepMania::GetInitialScreen() {
  if (PREFSMAN->m_sTestInitialScreen.Get() != "" &&
      SCREENMAN->IsScreenNameValid(PREFSMAN->m_sTestInitialScreen)) {
    return PREFSMAN->m_sTestInitialScreen;
  }
  std::string screen_name = INITIAL_SCREEN.GetValue();
  if (!SCREENMAN->IsScreenNameValid(screen_name)) {
    screen_name = "ScreenInitialScreenIsInvalid";
  }
  return screen_name;
}
ThemeMetric<std::string> SELECT_MUSIC_SCREEN("Common", "SelectMusicScreen");
std::string StepMania::GetSelectMusicScreen() {
  return SELECT_MUSIC_SCREEN.GetValue();
}

// This function is meant to only be called during start up.
void StepMania::InitializeCurrentGame(const Game* g) {
  ASSERT(g != nullptr);
  ASSERT(GAMESTATE != nullptr);
  ASSERT(ANNOUNCER != nullptr);
  ASSERT(THEME != nullptr);

  GAMESTATE->SetCurGame(g);

  std::string sAnnouncer = PREFSMAN->m_sAnnouncer;
  std::string sTheme = PREFSMAN->m_sTheme;
  std::string sGametype = GAMESTATE->GetCurrentGame()->m_szName;
  std::string sLanguage = PREFSMAN->m_sLanguage;

  if (sAnnouncer.empty()) {
    sAnnouncer = GAMESTATE->GetCurrentGame()->m_szName;
  }
  std::string argCurGame;
  if (GetCommandlineArgument("game", &argCurGame) && argCurGame != sGametype) {
    const Game* new_game = GAMEMAN->StringToGame(argCurGame);
    if (new_game == nullptr) {
      LOG->Warn("%s is not a known game type, ignoring.", argCurGame.c_str());
    } else {
      PREFSMAN->SetCurrentGame(sGametype);
      GAMESTATE->SetCurGame(new_game);
    }
  }

  // It doesn't matter if sTheme is blank or invalid, THEME->STAL will set
  // a selectable theme for us. -Kyz

  // process gametype, theme and language command line arguments;
  // these change the preferences in order for transparent loading -aj
  std::string argTheme;
  if (GetCommandlineArgument("theme", &argTheme) && argTheme != sTheme) {
    sTheme = argTheme;
    // set theme in preferences too for correct behavior  -aj
    PREFSMAN->m_sTheme.Set(sTheme);
  }

  std::string argLanguage;
  if (GetCommandlineArgument("language", &argLanguage)) {
    sLanguage = argLanguage;
    // set language in preferences too for correct behavior -aj
    PREFSMAN->m_sLanguage.Set(sLanguage);
  }

  // it's OK to call these functions with names that don't exist.
  ANNOUNCER->SwitchAnnouncer(sAnnouncer);
  THEME->SwitchThemeAndLanguage(sTheme, sLanguage, PREFSMAN->m_bPseudoLocalize);

  // Set the input scheme for the new game, and load keymaps.
  if (INPUTMAPPER) {
    INPUTMAPPER->SetInputScheme(&g->m_InputScheme);
    INPUTMAPPER->ReadMappingsFromDisk();
  }
}


std::string StepMania::SaveScreenshot(
    std::string Dir, bool SaveCompressed, bool MakeSignature,
    std::string NamePrefix, std::string NameSuffix) {
  /* As of sm-ssc v1.0 rc2, screenshots are no longer named by an arbitrary
   * index. This was causing naming issues for some unknown reason, so we have
   * changed the screenshot names to a non-blocking format: date and time.
   * As before, we ignore the extension. -aj */
  std::string FileNameNoExtension =
      NamePrefix + DateTime::GetNowDateTime().GetString() + NameSuffix;
  // replace space with underscore.
  Replace(FileNameNoExtension, " ", "_");
  // colons are illegal in filenames.
  Replace(FileNameNoExtension, ":", "");

  // Save the screenshot. If writing lossy to a memcard, use
  // SAVE_LOSSY_LOW_QUAL, so we don't eat up lots of space.
  RageDisplay::GraphicsFileFormat fmt;
  if (SaveCompressed) {
    fmt = RageDisplay::SAVE_LOSSY_HIGH_QUAL;
  } else {
    fmt = RageDisplay::SAVE_LOSSLESS_SENSIBLE;
  }

  std::string FileName =
      FileNameNoExtension + "." + (SaveCompressed ? "jpg" : "png");
  std::string Path = Dir + FileName;

  if (!DISPLAY->SaveScreenshot(Path, fmt)) {
    SCREENMAN->PlayInvalidSound();
    return std::string();
  }

  SCREENMAN->PlayScreenshotSound();

  if (PREFSMAN->m_bSignProfileData && MakeSignature) {
    CryptManager::SignFileToFile(Path);
  }

  return FileName;
}

void StepMania::InsertCoin(int iNum, bool bCountInBookkeeping) {
  if (bCountInBookkeeping) {
    LIGHTSMAN->PulseCoinCounter();
    BOOKKEEPER->CoinInserted();
  }
  int iNumCoinsOld = GAMESTATE->m_iCoins;

  // Don't allow GAMESTATE's coin count to become negative.
  if (GAMESTATE->m_iCoins + iNum >= 0) {
    GAMESTATE->m_iCoins.Set(GAMESTATE->m_iCoins + iNum);
  }

  int iCredits = GAMESTATE->m_iCoins / PREFSMAN->m_iCoinsPerCredit;
  bool bMaxCredits = iCredits >= PREFSMAN->m_iMaxNumCredits;
  if (bMaxCredits) {
    GAMESTATE->m_iCoins.Set(
        PREFSMAN->m_iMaxNumCredits * PREFSMAN->m_iCoinsPerCredit);
  }

  LOG->Trace(
      "%i coins inserted, %i needed to play", GAMESTATE->m_iCoins.Get(),
      PREFSMAN->m_iCoinsPerCredit.Get());

  // On InsertCoin, make sure to update Coins file
  BOOKKEEPER->WriteCoinsFile(GAMESTATE->m_iCoins.Get());

  // If inserting coins, play an appropriate sound; if deducting coins, don't
  // play anything.
  if (iNum > 0) {
    if (iNumCoinsOld != GAMESTATE->m_iCoins) {
      SCREENMAN->PlayCoinSound();
    } else {
      SCREENMAN->PlayInvalidSound();
    }
  }

  /* If AutoJoin and a player is already joined, then try to join a player.
   * (If no players are joined, they'll join on the first JoinInput.) */
  if (GAMESTATE->m_bAutoJoin.Get() && GAMESTATE->GetNumSidesJoined() > 0) {
    if (GAMESTATE->GetNumSidesJoined() > 0 && GAMESTATE->JoinPlayers()) {
      SCREENMAN->PlayStartSound();
    }
  }

  // TODO: remove this redundant message and things that depend on it
  Message msg("CoinInserted");
  // below params are unused
  // msg.SetParam( "Coins", GAMESTATE->m_iCoins );
  // msg.SetParam( "Inserted", iNum );
  // msg.SetParam( "MaxCredits", bMaxCredits );
  MESSAGEMAN->Broadcast(msg);
}

void StepMania::InsertCredit() {
  InsertCoin(PREFSMAN->m_iCoinsPerCredit, false);
}

void StepMania::ClearCredits() {
  LOG->Trace("%i coins cleared", GAMESTATE->m_iCoins.Get());
  GAMESTATE->m_iCoins.Set(0);
  SCREENMAN->PlayInvalidSound();

  // Update Coins file to make sure credits are cleared.
  BOOKKEEPER->WriteCoinsFile(GAMESTATE->m_iCoins.Get());

  // TODO: remove this redundant message and things that depend on it
  Message msg("CoinInserted");
  // below params are unused
  // msg.SetParam( "Coins", GAMESTATE->m_iCoins );
  // msg.SetParam( "Clear", true );
  MESSAGEMAN->Broadcast(msg);
}

/* Returns true if the key has been handled and should be discarded, false if
 * the key should be sent on to screens. */
static LocalizedString SERVICE_SWITCH_PRESSED(
    "StepMania", "Service switch pressed");
static LocalizedString RELOADED_METRICS("ThemeManager", "Reloaded metrics");
static LocalizedString RELOADED_METRICS_AND_TEXTURES(
    "ThemeManager", "Reloaded metrics and textures");
static LocalizedString RELOADED_SCRIPTS("ThemeManager", "Reloaded scripts");
static LocalizedString RELOADED_OVERLAY_SCREENS(
    "ThemeManager", "Reloaded overlay screens");
bool HandleGlobalInputs(const InputEventPlus& input) {
  // None of the globals keys act on types other than FIRST_PRESS
  if (input.type != IET_FIRST_PRESS) {
    return false;
  }

  switch (input.MenuI) {
    case GAME_BUTTON_OPERATOR:
      /* Global operator key, to get quick access to the options menu. Don't
       * do this if we're on a "system menu", which includes the editor
       * (to prevent quitting without storing changes). */
      if (SCREENMAN->AllowOperatorMenuButton()) {
        SCREENMAN->SystemMessage(SERVICE_SWITCH_PRESSED);
        SCREENMAN->PopAllScreens();
        GAMESTATE->Reset();
        SCREENMAN->SetNewScreen(CommonMetrics::OPERATOR_MENU_SCREEN);
      }
      return true;

    case GAME_BUTTON_COIN:
      // Handle a coin insertion.
      if (GAMESTATE->IsEditing())  // no coins while editing
      {
        LOG->Trace("Ignored coin insertion (editing)");
        break;
      }
      StepMania::InsertCoin();
      return false;  // Attract needs to know because it goes to TitleMenu on >
                     // 1 credit
    default:
      break;
  }

  /* Re-added for StepMania 3.9 theming veterans, plus it's just faster than
   * the debug menu. The Shift button only reloads the metrics, unlike in 3.9
   * (where it saved bookkeeping and machine profile). -aj */
  bool bIsShiftHeld =
      INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT), &input.InputList) ||
      INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT), &input.InputList);
  bool bIsCtrlHeld =
      INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL), &input.InputList) ||
      INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL), &input.InputList);
  if (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_F2)) {
    if (bIsShiftHeld && !bIsCtrlHeld) {
      // Shift+F2: refresh metrics,noteskin cache and CodeDetector cache only
      THEME->ReloadMetrics();
      NOTESKIN->RefreshNoteSkinData(GAMESTATE->m_pCurGame);
      CodeDetector::RefreshCacheItems();
      SCREENMAN->SystemMessage(RELOADED_METRICS);
    } else if (bIsCtrlHeld && !bIsShiftHeld) {
      // Ctrl+F2: reload scripts only
      if (NETWORK != nullptr) {
        NETWORK->CloseAllWebSockets();
      }
      THEME->UpdateLuaGlobals();
      SCREENMAN->SystemMessage(RELOADED_SCRIPTS);
    } else if (bIsCtrlHeld && bIsShiftHeld) {
      // Shift+Ctrl+F2: reload overlay screens (and metrics, since themers
      // are likely going to do this after changing metrics.)
      THEME->ReloadMetrics();
      SCREENMAN->ReloadOverlayScreens();
      SCREENMAN->SystemMessage(RELOADED_OVERLAY_SCREENS);
    } else {
      // F2 alone: refresh metrics, textures, noteskins, codedetector cache
      THEME->ReloadMetrics();
      TEXTUREMAN->ReloadAll();
      NOTESKIN->RefreshNoteSkinData(GAMESTATE->m_pCurGame);
      CodeDetector::RefreshCacheItems();
      SCREENMAN->SystemMessage(RELOADED_METRICS_AND_TEXTURES);
    }

    return true;
  }

  if (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_PAUSE)) {
    Message msg("ToggleConsoleDisplay");
    MESSAGEMAN->Broadcast(msg);
    return true;
  }

#if !defined(MACOSX)
  if (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_F4)) {
    if (INPUTFILTER->IsBeingPressed(
            DeviceInput(DEVICE_KEYBOARD, KEY_RALT), &input.InputList) ||
        INPUTFILTER->IsBeingPressed(
            DeviceInput(DEVICE_KEYBOARD, KEY_LALT), &input.InputList)) {
      // pressed Alt+F4
      ArchHooks::SetUserQuit();
      return true;
    }
  }
#else
  if (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_Cq) &&
      (INPUTFILTER->IsBeingPressed(
           DeviceInput(DEVICE_KEYBOARD, KEY_LMETA), &input.InputList) ||
       INPUTFILTER->IsBeingPressed(
           DeviceInput(DEVICE_KEYBOARD, KEY_RMETA), &input.InputList))) {
    /* The user quit is handled by the menu item so we don't need to set it
     * here; however, we do want to return that it has been handled since
     * this will happen first. */
    return true;
  }
#endif

  bool bDoScreenshot =
#if defined(MACOSX)
      // Notebooks don't have F13. Use cmd-F12 as well.
      input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_PRTSC) ||
      input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_F13) ||
      (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_F12) &&
       (INPUTFILTER->IsBeingPressed(
            DeviceInput(DEVICE_KEYBOARD, KEY_LMETA), &input.InputList) ||
        INPUTFILTER->IsBeingPressed(
            DeviceInput(DEVICE_KEYBOARD, KEY_RMETA), &input.InputList)));
#else
      /* The default Windows message handler will capture the desktop window
       * upon pressing PrntScrn, or will capture the foreground with focus upon
       * pressing Alt+PrntScrn. Windows will do this whether or not we save a
       * screenshot ourself by dumping the frame buffer. */
      // "if pressing PrintScreen and not pressing Alt"
      input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_PRTSC) &&
      !INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_LALT), &input.InputList) &&
      !INPUTFILTER->IsBeingPressed(
          DeviceInput(DEVICE_KEYBOARD, KEY_RALT), &input.InputList);
#endif
  if (bDoScreenshot) {
    // If holding Shift save uncompressed, else save compressed
    bool bHoldingShift =
        (INPUTFILTER->IsBeingPressed(
             DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT)) ||
         INPUTFILTER->IsBeingPressed(DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT)));
    bool bSaveCompressed = !bHoldingShift;
    RageTimer timer;
    StepMania::SaveScreenshot("Screenshots/", bSaveCompressed, false, "", "");
    LOG->Trace("Screenshot took %f seconds.", timer.GetDeltaTime());
    return true;  // handled
  }

  if (input.DeviceI == DeviceInput(DEVICE_KEYBOARD, KEY_ENTER) &&
      (INPUTFILTER->IsBeingPressed(
           DeviceInput(DEVICE_KEYBOARD, KEY_RALT), &input.InputList) ||
       INPUTFILTER->IsBeingPressed(
           DeviceInput(DEVICE_KEYBOARD, KEY_LALT), &input.InputList))) {
    // alt-enter
    /* In macOS, this is a menu item and will be handled as such. This will
     * happen first and then the lower priority GUI thread will happen second,
     * causing the window to toggle twice. Another solution would be to put
     * a timer in ArchHooks::SetToggleWindowed() and just not set the bool
     * it if it's been less than, say, half a second. */
#if !defined(MACOSX)
    ArchHooks::SetToggleWindowed();
#endif
    return true;
  }

  return false;
}

void HandleInputEvents(float fDeltaTime) {
  INPUTFILTER->Update(fDeltaTime);

  /* Hack: If the topmost screen hasn't been updated yet, don't process input,
   * since we must not send inputs to a screen that hasn't at least had one
   * update yet. (The first Update should be the very first thing a screen
   * gets.) We'll process it next time. Call Update above, so the inputs are
   * read and timestamped. */
  if (SCREENMAN->GetTopScreen()->IsFirstUpdate()) {
    return;
  }

  std::vector<InputEvent> ieArray;
  INPUTFILTER->GetInputEvents(ieArray);

  // If we don't have focus, discard input.
  if (!HOOKS->AppHasFocus()) {
    return;
  }

  for (unsigned i = 0; i < ieArray.size(); i++) {
    InputEventPlus input;
    input.DeviceI = ieArray[i].di;
    input.type = ieArray[i].type;
    swap(input.InputList, ieArray[i].m_ButtonState);

    // hack for testing (MultiPlayer) with only one joystick
    /*
    if( input.DeviceI.IsJoystick() )
    {
            if( INPUTFILTER->IsBeingPressed(
    DeviceInput(DEVICE_KEYBOARD,KEY_LSHIFT) ) ) input.DeviceI.device =
    (InputDevice)(input.DeviceI.device + 1); if( INPUTFILTER->IsBeingPressed(
    DeviceInput(DEVICE_KEYBOARD,KEY_LCTRL) ) ) input.DeviceI.device =
    (InputDevice)(input.DeviceI.device + 2); if( INPUTFILTER->IsBeingPressed(
    DeviceInput(DEVICE_KEYBOARD,KEY_LALT) ) ) input.DeviceI.device =
    (InputDevice)(input.DeviceI.device + 4); if( INPUTFILTER->IsBeingPressed(
    DeviceInput(DEVICE_KEYBOARD,KEY_RALT) ) ) input.DeviceI.device =
    (InputDevice)(input.DeviceI.device + 8); if( INPUTFILTER->IsBeingPressed(
    DeviceInput(DEVICE_KEYBOARD,KEY_RCTRL) ) ) input.DeviceI.device =
    (InputDevice)(input.DeviceI.device + 16);
    }
    */

    INPUTMAPPER->DeviceToGame(input.DeviceI, input.GameI);

    input.mp = MultiPlayer_Invalid;

    {
      // Translate input to the appropriate MultiPlayer. Assume that all
      // joystick devices are mapped the same as the master player.
      if (input.DeviceI.IsJoystick()) {
        DeviceInput diTemp = input.DeviceI;
        diTemp.device = DEVICE_JOY1;
        GameInput gi;

        // LOG->Trace( "device %d, %d", diTemp.device, diTemp.button );
        if (INPUTMAPPER->DeviceToGame(diTemp, gi)) {
          if (GAMESTATE->m_bMultiplayer) {
            input.GameI = gi;
            // LOG->Trace( "game %d %d", input.GameI.controller,
            // input.GameI.button );
          }

          input.mp =
              InputMapper::InputDeviceToMultiPlayer(input.DeviceI.device);
          // LOG->Trace( "multiplayer %d", input.mp );
          ASSERT(input.mp >= 0 && input.mp < NUM_MultiPlayer);
        }
      }
    }

    if (input.GameI.IsValid()) {
      input.MenuI = INPUTMAPPER->GameButtonToMenuButton(input.GameI.button);
      input.pn = INPUTMAPPER->ControllerToPlayerNumber(input.GameI.controller);
    }

    INPUTQUEUE->RememberInput(input);

    // When a GameButton is pressed, stop repeating other keys on the same
    // controller.
    if (input.type == IET_FIRST_PRESS && input.MenuI != GameButton_Invalid) {
      FOREACH_ENUM(GameButton, m) {
        if (input.MenuI != m) {
          INPUTMAPPER->RepeatStopKey(m, input.pn);
        }
      }
    }

    if (HandleGlobalInputs(input)) {
      continue;  // skip
    }

    // check back in event mode
    if (GAMESTATE->IsEventMode() &&
        CodeDetector::EnteredCode(
            input.GameI.controller, CODE_BACK_IN_EVENT_MODE)) {
      input.MenuI = GAME_BUTTON_BACK;
    }

    SCREENMAN->Input(input);
  }

  if (ArchHooks::GetAndClearToggleWindowed()) {
    PREFSMAN->m_bWindowed.Set(!PREFSMAN->m_bWindowed);
    StepMania::ApplyGraphicOptions();
  }
}

#include "LuaManager.h"
int LuaFunc_SaveScreenshot(lua_State* L);
int LuaFunc_SaveScreenshot(lua_State* L) {
  // If pn is provided, save to that player's profile.
  // Otherwise, save to the machine.
  PlayerNumber pn = Enum::Check<PlayerNumber>(L, 1, true);
  bool compress = lua_toboolean(L, 2) > 0;
  bool sign = lua_toboolean(L, 3) > 0;
  std::string prefix = luaL_optstring(L, 4, "");
  std::string suffix = luaL_optstring(L, 5, "");
  std::string dir;
  if (pn == PlayerNumber_Invalid) {
    dir = "Screenshots/";
  } else {
    dir = PROFILEMAN->GetProfileDir((ProfileSlot)pn) + "Screenshots/";
    if (PROFILEMAN->ProfileWasLoadedFromMemoryCard(pn)) {
      MEMCARDMAN->MountCard(pn);
    }
  }
  std::string filename =
      StepMania::SaveScreenshot(dir, compress, sign, prefix, suffix);
  if (pn != PlayerNumber_Invalid) {
    if (PROFILEMAN->ProfileWasLoadedFromMemoryCard(pn)) {
      MEMCARDMAN->UnmountCard(pn);
    }
  }
  std::string path = dir + filename;
  lua_pushboolean(L, !filename.empty());
  lua_pushstring(L, path.c_str());
  return 2;
}
void LuaFunc_Register_SaveScreenshot(lua_State* L);
void LuaFunc_Register_SaveScreenshot(lua_State* L) {
  lua_register(L, "SaveScreenshot", LuaFunc_SaveScreenshot);
}
REGISTER_WITH_LUA_FUNCTION(LuaFunc_Register_SaveScreenshot);

/*
 * (c) 2001-2004 Chris Danford, Glenn Maynard
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
