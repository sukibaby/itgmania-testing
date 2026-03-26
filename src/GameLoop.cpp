#include "GameLoop.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "GameSoundManager.h"
#include "GameState.h"
#include "Synchronizer.h"
#include "InputFilter.h"
#include "InputMapper.h"
#include "LightsManager.h"
#include "LuaManager.h"
#include "MemoryCardManager.h"
#include "NetworkManager.h"
#include "PeriodicCaller.h"
#include "Preference.h"
#include "PrefsManager.h"
#include "RageDisplay.h"
#include "RageInput.h"
#include "RageLog.h"
#include "RageSoundManager.h"
#include "RageTextureManager.h"
#include "RageThreads.h"
#include "RageTimer.h"
#include "RageUtil.h"
#include "ScreenManager.h"
#include "StepMania.h"
#include "ThemeManager.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "global.h"

static RageTimer g_GameplayTimer;

static Preference<bool> g_bNeverBoostAppPriority(
    "NeverBoostAppPriority", false);

/* experimental: force a specific update rate. This prevents big  animation
 * jumps on frame skips. 0 to disable. */
static Preference<float> g_fConstantUpdateDeltaSeconds(
    "ConstantUpdateDeltaSeconds", 0);

static Preference<bool> g_bEnableFrameBudgeting("EnableFrameBudgeting", true);
static Preference<float> g_fFrameBudgetSeconds("FrameBudgetSeconds", 1.0f / 60);
static Preference<float> g_fFrameBudgetExceedWarnMarginSeconds(
  "FrameBudgetExceedWarnMarginSeconds", 0.0005f);
static Preference<float> g_fFrameBudgetWarningCooldownSeconds(
    "FrameBudgetWarningCooldownSeconds", 0.5f);
static Preference<bool> g_bWarnOnLikelyDrawBoundBudgetExceed(
  "WarnOnLikelyDrawBoundBudgetExceed", false);
static Preference<float> g_fLikelyDrawBoundShareThreshold(
  "LikelyDrawBoundShareThreshold", 0.80f);
static Preference<float> g_fLikelyInputUpdateBoundShareThreshold(
  "LikelyInputUpdateBoundShareThreshold", 0.80f);
static Preference<float> g_fLikelyDelayedScreenLoadBoundShareThreshold(
    "LikelyDelayedScreenLoadBoundShareThreshold", 0.80f);
static Preference<float> g_fLikelyScreenUpdateBoundShareThreshold(
    "LikelyScreenUpdateBoundShareThreshold", 0.80f);
static Preference<float> g_fLikelyPresentBoundEndFrameShareThreshold(
    "LikelyPresentBoundEndFrameShareThreshold", 0.70f);
static Preference<int> g_iMainThreadSyncTaskBudget(
    "MainThreadSyncTaskBudget", 32);
static Preference<float> g_fMainThreadSyncTimeBudgetSeconds(
    "MainThreadSyncTimeBudgetSeconds", 0.001f);

struct UpdateTimingBreakdown {
  float soundManagerUpdate = 0.0f;
  float soundUpdate = 0.0f;
  float textureUpdate = 0.0f;
  float gameStateUpdate = 0.0f;
  float networkUpdate = 0.0f;
  float mainThreadSyncTaskTime = 0.0f;
  int mainThreadSyncTasksRun = 0;
  float screenUpdate = 0.0f;
  float memoryCardUpdate = 0.0f;
  float inputUpdate = 0.0f;
  float lightsUpdate = 0.0f;
  float totalUpdate = 0.0f;
};

static UpdateTimingBreakdown g_LastUpdateTimingBreakdown;

static float g_fUpdateRate = 1;
void GameLoop::SetUpdateRate(float fUpdateRate) { g_fUpdateRate = fUpdateRate; }

float GameLoop::GetUpdateRate() { return g_fUpdateRate; }

static void CheckGameLoopTimerSkips(float fDeltaTime) {
  static int iLastFPS = 0;
  int iThisFPS = DISPLAY->GetFPS();

  /* If vsync is on, and we have a solid framerate (vsync == refresh and we've
   * sustained this for at least one second), we expect the amount of time for
   * the last frame to be 1/FPS. */
  if (iThisFPS != DISPLAY->GetActualVideoModeParams().rate ||
      iThisFPS != iLastFPS) {
    iLastFPS = iThisFPS;
    return;
  }

  const float fExpectedTime = 1.0f / iThisFPS;
  const float fDifference = fDeltaTime - fExpectedTime;
  if (std::abs(fDifference) > 0.002f && std::abs(fDifference) < 0.100f) {
    LOG->Trace(
        "GameLoop timer skip: %i FPS, expected %.3f, got %.3f (%.3f "
        "difference)",
        iThisFPS, fExpectedTime, fDeltaTime, fDifference);
  }
}

static bool ShouldLogFrameBudgetWarning() {
  static double lastWarningAt = -1000.0;

  const double now = RageTimer::GetTimeSinceStart();
  const float cooldown =
      std::max(0.0f, static_cast<float>(g_fFrameBudgetWarningCooldownSeconds));
  if ((now - lastWarningAt) < cooldown) {
    return false;
  }

  lastWarningAt = now;
  return true;
}

static void LogFrameBudgetWarning(
    float frameSeconds, float budgetSeconds, float updateSeconds,
    float drawSeconds, bool likelyDrawBound) {
  const ScreenManager::DrawTimingBreakdown& drawTiming =
      SCREENMAN->GetLastDrawTimingBreakdown();
  const ScreenManager::UpdateTimingBreakdown& screenUpdateTiming =
      SCREENMAN->GetLastUpdateTimingBreakdown();
  const InputEventHandlingTimingBreakdown& inputTiming =
      GetLastInputEventHandlingTimingBreakdown();
  const RageDisplay::EndFrameTimingBreakdown& endFrameTiming =
      DISPLAY->GetLastEndFrameTimingBreakdown();
  const float drawEndShare =
      (drawSeconds > 0.0f) ? (drawTiming.endFrame / drawSeconds) : 0.0f;
  const float updateInputShare =
      (updateSeconds > 0.0f)
          ? (g_LastUpdateTimingBreakdown.inputUpdate / updateSeconds)
          : 0.0f;
  const float updateScreenShare =
      (updateSeconds > 0.0f)
          ? (g_LastUpdateTimingBreakdown.screenUpdate / updateSeconds)
          : 0.0f;
  const float updateScreenDelayShare =
      (updateSeconds > 0.0f)
          ? (screenUpdateTiming.loadDelayedScreen / updateSeconds)
          : 0.0f;
  const float inputUpdateThreshold = std::clamp(
      static_cast<float>(g_fLikelyInputUpdateBoundShareThreshold.Get()),
      0.0f, 1.0f);
  const bool likelyInputUpdateBound =
      updateSeconds > budgetSeconds && updateInputShare >= inputUpdateThreshold;
  const float delayedScreenLoadThreshold = std::clamp(
      static_cast<float>(g_fLikelyDelayedScreenLoadBoundShareThreshold.Get()),
      0.0f, 1.0f);
  const bool likelyScreenDelayedLoadBound =
      updateSeconds > budgetSeconds &&
      screenUpdateTiming.didLoadDelayedScreen &&
      updateScreenDelayShare >= delayedScreenLoadThreshold;
  const float screenUpdateThreshold = std::clamp(
      static_cast<float>(g_fLikelyScreenUpdateBoundShareThreshold.Get()),
      0.0f, 1.0f);
  const bool likelyScreenUpdateBound =
      updateSeconds > budgetSeconds && updateScreenShare >= screenUpdateThreshold;
  const float presentThreshold = std::clamp(
      static_cast<float>(g_fLikelyPresentBoundEndFrameShareThreshold.Get()),
      0.0f, 1.0f);
  const bool likelyPresentBound =
      likelyDrawBound && drawTiming.drewFrame && drawEndShare >= presentThreshold;
  const char* delayedScreenName =
      screenUpdateTiming.delayedScreenName.empty()
          ? "<none>"
          : screenUpdateTiming.delayedScreenName.c_str();
  const char* delayedBackgroundName =
      screenUpdateTiming.delayedBackgroundName.empty()
          ? "<none>"
          : screenUpdateTiming.delayedBackgroundName.c_str();
  const char* warningClass = "update-or-mixed";
  if (likelyPresentBound) {
    warningClass = "likely-present-bound";
  } else if (likelyInputUpdateBound) {
    warningClass = "likely-input-update-bound";
  } else if (likelyScreenDelayedLoadBound) {
    warningClass = "likely-screen-delayed-load-bound";
  } else if (likelyScreenUpdateBound) {
    warningClass = "likely-screen-update-bound";
  } else if (likelyDrawBound) {
    warningClass = "likely-draw-bound";
  }
  int pendingMainThreadTasks = 0;
  int pendingBackgroundTasks = 0;
  int peakPendingMainThreadTasks = 0;
  int peakPendingBackgroundTasks = 0;
  uint64_t submittedWork = 0;
  uint64_t completedWork = 0;
  uint64_t submittedMainTasks = 0;
  uint64_t completedMainTasks = 0;
  const int fps = DISPLAY->GetFPS();
  const int refresh = DISPLAY->GetActualVideoModeParams().rate;
  const float drawShare =
      (frameSeconds > 0.0f) ? (drawSeconds / frameSeconds) : 0.0f;
  if (SYNCHRONIZER != nullptr) {
    pendingMainThreadTasks =
        static_cast<int>(SYNCHRONIZER->GetPendingMainThreadTaskCount());
    pendingBackgroundTasks =
        static_cast<int>(SYNCHRONIZER->GetPendingWorkCount());
    peakPendingMainThreadTasks =
        static_cast<int>(SYNCHRONIZER->GetPeakPendingMainThreadTaskCount());
    peakPendingBackgroundTasks =
        static_cast<int>(SYNCHRONIZER->GetPeakPendingWorkCount());
    submittedWork = SYNCHRONIZER->GetSubmittedWorkCount();
    completedWork = SYNCHRONIZER->GetCompletedWorkCount();
    submittedMainTasks = SYNCHRONIZER->GetSubmittedMainThreadTaskCount();
    completedMainTasks = SYNCHRONIZER->GetCompletedMainThreadTaskCount();
  }

  LOG->Warn(
      "Frame budget exceeded (%s): frame=%.4fs budget=%.4fs update=%.4fs draw=%.4fs drawShare=%.2f "
      "updateInputShare=%.2f updateScreenShare=%.2f updateScreenDelayShare=%.2f "
      "drawBegin=%.4fs drawScene=%.4fs drawEnd=%.4fs drawEndShare=%.2f drewFrame=%d "
      "endPre=%.4fs endLimitBefore=%.4fs endPresent=%.4fs endLimitAfter=%.4fs "
      "endFinish=%.4fs endWindowUpdate=%.4fs endTotal=%.4fs endValid=%d "
      "scrPop=%.4fs scrStack=%.4fs scrBga=%.4fs scrOverlay=%.4fs "
      "scrFlush=%.4fs scrDelay=%.4fs scrTotal=%.4fs screens=%d overlays=%d "
      "didFlush=%d didDelay=%d "
      "scrDelayPop=%.4fs scrDelayAct0=%.4fs scrDelayPrune=%.4fs scrDelayPrep=%.4fs "
      "scrDelayPrepMake=%.4fs scrDelayPrepBga=%.4fs "
      "scrDelayAct1=%.4fs scrDelayActScreen=%.4fs scrDelayActBga=%.4fs "
      "scrDelayDelete=%.4fs scrDelayMsg=%.4fs "
      "scrDelayActors=%d scrDelayPrepared=%d scrDelayNeedPrep=%d scrDelayDeleteNow=%d "
      "scrDelayScreen=\"%s\" scrDelayBga=\"%s\" "
      "inFilter=%.4fs inCollect=%.4fs inProcess=%.4fs inPre=%.4fs inDispatch=%.4fs "
      "inToggle=%.4fs inTotal=%.4fs inCount=%d inFirstUpdate=%d inNoFocus=%d "
      "fps=%d refresh=%d "
      "[sndman=%.4fs sound=%.4fs tex=%.4fs gs=%.4fs net=%.4fs sync=%d/%.4fs "
      "scr=%.4fs mem=%.4fs input=%.4fs lights=%.4fs pendingMain=%d "
      "pendingWork=%d peakMain=%d peakWork=%d submittedMain=%llu completedMain=%llu "
      "submittedWork=%llu completedWork=%llu]",
      warningClass,
      frameSeconds, budgetSeconds, updateSeconds, drawSeconds, drawShare,
      updateInputShare,
      updateScreenShare,
      updateScreenDelayShare,
      drawTiming.beginFrame, drawTiming.sceneDraw, drawTiming.endFrame,
      drawEndShare, drawTiming.drewFrame ? 1 : 0,
      endFrameTiming.prePresentWork, endFrameTiming.frameLimitBeforeVsync,
      endFrameTiming.presentOrSwap, endFrameTiming.frameLimitAfterVsync,
      endFrameTiming.finishGpu, endFrameTiming.windowUpdate,
      endFrameTiming.totalEndFrame, endFrameTiming.valid ? 1 : 0,
      screenUpdateTiming.popTopScreenAndMessages,
      screenUpdateTiming.screenStackUpdate,
      screenUpdateTiming.sharedBackgroundUpdate,
      screenUpdateTiming.overlayUpdate,
      screenUpdateTiming.firstUpdateSoundFlush,
      screenUpdateTiming.loadDelayedScreen,
      screenUpdateTiming.totalUpdate,
      screenUpdateTiming.stackScreenCountUpdated,
      screenUpdateTiming.overlayScreenCountUpdated,
      screenUpdateTiming.didSoundFlush ? 1 : 0,
      screenUpdateTiming.didLoadDelayedScreen ? 1 : 0,
      screenUpdateTiming.delayedPopTop,
      screenUpdateTiming.delayedActivatePreparedInitial,
      screenUpdateTiming.delayedDeletePreparedOrGrab,
      screenUpdateTiming.delayedPrepareScreen,
      screenUpdateTiming.delayedPrepareMakeScreen,
      screenUpdateTiming.delayedPrepareBackground,
      screenUpdateTiming.delayedActivatePreparedFinal,
      screenUpdateTiming.delayedActivateScreenPush,
      screenUpdateTiming.delayedActivateBackgroundSwap,
      screenUpdateTiming.delayedDeleteGrabbedActors,
      screenUpdateTiming.delayedBroadcastAndMessage,
      screenUpdateTiming.delayedGrabbedActorCount,
      screenUpdateTiming.delayedUsedPreparedScreen ? 1 : 0,
      screenUpdateTiming.delayedRequiredPrepare ? 1 : 0,
      screenUpdateTiming.delayedDeletePreparedImmediately ? 1 : 0,
      delayedScreenName,
      delayedBackgroundName,
      inputTiming.filterUpdate,
      inputTiming.collectEvents,
      inputTiming.processEventsTotal,
      inputTiming.perEventPreDispatch,
      inputTiming.perEventScreenDispatch,
      inputTiming.toggleWindowed,
      inputTiming.total,
      inputTiming.eventCount,
      inputTiming.returnedFirstUpdate ? 1 : 0,
      inputTiming.returnedNoFocus ? 1 : 0,
      fps, refresh,
      g_LastUpdateTimingBreakdown.soundManagerUpdate,
      g_LastUpdateTimingBreakdown.soundUpdate,
      g_LastUpdateTimingBreakdown.textureUpdate,
      g_LastUpdateTimingBreakdown.gameStateUpdate,
      g_LastUpdateTimingBreakdown.networkUpdate,
      g_LastUpdateTimingBreakdown.mainThreadSyncTasksRun,
      g_LastUpdateTimingBreakdown.mainThreadSyncTaskTime,
      g_LastUpdateTimingBreakdown.screenUpdate,
      g_LastUpdateTimingBreakdown.memoryCardUpdate,
      g_LastUpdateTimingBreakdown.inputUpdate,
      g_LastUpdateTimingBreakdown.lightsUpdate, pendingMainThreadTasks,
      pendingBackgroundTasks, peakPendingMainThreadTasks,
      peakPendingBackgroundTasks,
      static_cast<unsigned long long>(submittedMainTasks),
      static_cast<unsigned long long>(completedMainTasks),
      static_cast<unsigned long long>(submittedWork),
      static_cast<unsigned long long>(completedWork));
}

static bool IsLikelyDrawBoundFrame(
    float frameSeconds, float budgetSeconds, float updateSeconds,
    float drawSeconds) {
  if (frameSeconds <= 0.0f || budgetSeconds <= 0.0f) {
    return false;
  }

  const float drawShare = drawSeconds / frameSeconds;
  const float threshold =
      std::clamp(static_cast<float>(g_fLikelyDrawBoundShareThreshold.Get()), 0.0f, 1.0f);
  return updateSeconds <= budgetSeconds && drawShare >= threshold;
}

static bool IsLikelyScreenUpdateBoundFrame(
    float updateSeconds, float budgetSeconds) {
  if (updateSeconds <= 0.0f || budgetSeconds <= 0.0f) {
    return false;
  }

  if (updateSeconds <= budgetSeconds) {
    return false;
  }

  const float screenShare = g_LastUpdateTimingBreakdown.screenUpdate / updateSeconds;
  const float threshold = std::clamp(
      static_cast<float>(g_fLikelyScreenUpdateBoundShareThreshold.Get()),
      0.0f, 1.0f);
  return screenShare >= threshold;
}

static bool IsLikelyDelayedScreenLoadBoundFrame(
    float updateSeconds, float budgetSeconds) {
  if (updateSeconds <= 0.0f || budgetSeconds <= 0.0f) {
    return false;
  }

  if (updateSeconds <= budgetSeconds) {
    return false;
  }

  const ScreenManager::UpdateTimingBreakdown& screenUpdateTiming =
      SCREENMAN->GetLastUpdateTimingBreakdown();
  if (!screenUpdateTiming.didLoadDelayedScreen) {
    return false;
  }

  const float delayShare = screenUpdateTiming.loadDelayedScreen / updateSeconds;
  const float threshold = std::clamp(
      static_cast<float>(g_fLikelyDelayedScreenLoadBoundShareThreshold.Get()),
      0.0f, 1.0f);
  return delayShare >= threshold;
}

static bool IsLikelyInputUpdateBoundFrame(
    float updateSeconds, float budgetSeconds) {
  if (updateSeconds <= 0.0f || budgetSeconds <= 0.0f) {
    return false;
  }

  if (updateSeconds <= budgetSeconds) {
    return false;
  }

  const float inputShare = g_LastUpdateTimingBreakdown.inputUpdate / updateSeconds;
  const float threshold = std::clamp(
      static_cast<float>(g_fLikelyInputUpdateBoundShareThreshold.Get()),
      0.0f, 1.0f);
  return inputShare >= threshold;
}

static bool IsLikelyPresentBoundFrame(float drawSeconds) {
  if (drawSeconds <= 0.0f) {
    return false;
  }

  const ScreenManager::DrawTimingBreakdown& drawTiming =
      SCREENMAN->GetLastDrawTimingBreakdown();
  if (!drawTiming.drewFrame) {
    return false;
  }

  const float endShare = drawTiming.endFrame / drawSeconds;
  const float threshold = std::clamp(
      static_cast<float>(g_fLikelyPresentBoundEndFrameShareThreshold.Get()),
      0.0f, 1.0f);
  return endShare >= threshold;
}

static bool ChangeAppPri() {
  if (g_bNeverBoostAppPriority.Get()) {
    return false;
  }

  // If this is a debug build, don't. It makes the VC debugger sluggish.
#if defined(WIN32) && defined(DEBUG)
  return false;
#else
  return true;
#endif
}

static void CheckFocus() {
  if (!HOOKS->AppFocusChanged()) {
    return;
  }

  // If we lose focus, we may lose input events, especially key releases.
  INPUTFILTER->Reset();
}

static void CheckInputDevices() {
  if (INPUTMAN->DevicesChanged()) {
    INPUTFILTER->Reset();  // fix "buttons stuck" if button held while unplugged
    INPUTMAN->LoadDrivers();
    std::string sMessage;
    if (INPUTMAPPER->CheckForChangedInputDevicesAndRemap(sMessage)) {
      SCREENMAN->SystemMessage(sMessage);
    }
  }
}

// On the next update, change themes, and load sNewScreen.
static std::string g_NewTheme;
static std::string g_NewGame;
void GameLoop::ChangeTheme(const std::string& sNewTheme) {
  g_NewTheme = sNewTheme;
}

void GameLoop::ChangeGame(
    const std::string& new_game, const std::string& new_theme) {
  g_NewGame = new_game;
  g_NewTheme = new_theme;
}

#include "Game.h"
#include "GameManager.h"
#include "StepMania.h"  // XXX
namespace {
std::string GetNewScreenName() {
  if (THEME->HasMetric("Common", "AfterThemeChangeScreen")) {
    std::string after_screen =
        THEME->GetMetric("Common", "AfterThemeChangeScreen");
    if (SCREENMAN->IsScreenNameValid(after_screen)) {
      return after_screen;
    }
  }

  std::string new_screen = THEME->GetMetric("Common", "InitialScreen");
  if (!SCREENMAN->IsScreenNameValid(new_screen)) {
    return "ScreenInitialScreenIsInvalid";
  }
  return new_screen;
}

void DoChangeTheme() {
  RageUtil::SafeDelete(SCREENMAN);
  TEXTUREMAN->DoDelayedDelete();

  // In case the previous theme overloaded class bindings, reinitialize them.
  LUA->RegisterTypes();

  // We always need to force the theme to reload because we cleared the lua
  // state by calling RegisterTypes so the scripts in Scripts/ need to run.
  THEME->SwitchThemeAndLanguage(
      g_NewTheme, THEME->GetCurLanguage(), PREFSMAN->m_bPseudoLocalize, true);
  PREFSMAN->m_sTheme.Set(g_NewTheme);

  // Apply the new window title, icon and aspect ratio.
  StepMania::ApplyGraphicOptions();

  SCREENMAN = new ScreenManager();

  StepMania::ResetGame();
  SCREENMAN->ThemeChanged();
  // The previous system for changing the theme fetched the "NextScreen"
  // metric from the current theme, then changed the theme, then tried to
  // set the new screen to the name that had been fetched.
  // If the new screen didn't exist in the new theme, there would be a
  // crash.
  // So now the correct thing to do is for a theme to specify its entry
  // point after a theme change, ensuring that we are going to a valid
  // screen and not crashing. -Kyz
  std::string newScreenName = GetNewScreenName();
  SCREENMAN->SetNewScreen(newScreenName);

  // Indicate no further theme change is needed
  g_NewTheme = std::string();
}

void DoChangeGame() {
  const Game* g = GAMEMAN->StringToGame(g_NewGame);
  ASSERT(g != nullptr);
  GAMESTATE->SetCurGame(g);

  bool theme_changing = false;
  // The prefs allow specifying a different default theme to use for each
  // game type.  So if a theme name isn't passed in, fetch from the prefs.
  if (g_NewTheme.empty()) {
    g_NewTheme = PREFSMAN->m_sTheme;
  }
  if (g_NewTheme != THEME->GetCurThemeName() &&
      THEME->IsThemeSelectable(g_NewTheme)) {
    theme_changing = true;
  }

  if (theme_changing) {
    RageUtil::SafeDelete(SCREENMAN);
    TEXTUREMAN->DoDelayedDelete();
    LUA->RegisterTypes();
    THEME->SwitchThemeAndLanguage(
        g_NewTheme, THEME->GetCurLanguage(), PREFSMAN->m_bPseudoLocalize);
    PREFSMAN->m_sTheme.Set(g_NewTheme);
    StepMania::ApplyGraphicOptions();
    SCREENMAN = new ScreenManager();
  }
  StepMania::ResetGame();
  std::string new_screen = THEME->GetMetric("Common", "InitialScreen");
  std::string after_screen;
  if (theme_changing) {
    SCREENMAN->ThemeChanged();
    if (THEME->HasMetric("Common", "AfterGameAndThemeChangeScreen")) {
      after_screen =
          THEME->GetMetric("Common", "AfterGameAndThemeChangeScreen");
    }
  } else {
    if (THEME->HasMetric("Common", "AfterGameChangeScreen")) {
      after_screen = THEME->GetMetric("Common", "AfterGameChangeScreen");
    }
  }
  if (SCREENMAN->IsScreenNameValid(after_screen)) {
    new_screen = after_screen;
  }
  SCREENMAN->SetNewScreen(new_screen);

  // Set the input scheme for the new game, and load keymaps.
  if (INPUTMAPPER) {
    INPUTMAPPER->SetInputScheme(&g->m_InputScheme);
    INPUTMAPPER->ReadMappingsFromDisk();
  }
  // aj's comment transplanted from ScreenOptionsMasterPrefs.cpp:GameSel. -Kyz
  /* Reload metrics to force a refresh of CommonMetrics::DIFFICULTIES_TO_SHOW,
   * mainly if we're not switching themes. I'm not sure if this was the
   * case going from theme to theme, but if it was, it should be fixed
   * now. There's probably be a better way to do it, but I'm not sure
   * what it'd be. -aj */
  THEME->UpdateLuaGlobals();
  THEME->ReloadMetrics();
  g_NewGame = std::string();
  g_NewTheme = std::string();
}
}  // namespace

void GameLoop::UpdateAllButDraw(bool bRunningFromVBLANK) {
  RageTimer updateTimer;
  g_LastUpdateTimingBreakdown = UpdateTimingBreakdown();

  // Flag to indicate whether an update has been processed during the VBLANK
  // period.
  static bool m_bUpdatedDuringVBLANK = false;

  // If we're running from VBLANK, and we've already updated during the VBLANK
  // period, don't update again. This is to prevent multiple updates during the
  // same VBLANK period.
  if (!bRunningFromVBLANK && m_bUpdatedDuringVBLANK) {
    m_bUpdatedDuringVBLANK = false;
    return;
  }

  // If we're running from VBLANK, indicate we've updated during the VBLANK
  // period. Otherwise, make sure the flag is cleared.
  if (bRunningFromVBLANK) {
    m_bUpdatedDuringVBLANK = true;
  } else {
    m_bUpdatedDuringVBLANK = false;
  }

  // If the constant update delta is set, use that value. Otherwise, use the
  // delta time from the gameplay timer.
  float fDeltaTime = (g_fConstantUpdateDeltaSeconds > 0)
                         ? g_fConstantUpdateDeltaSeconds
                         : g_GameplayTimer.GetDeltaTime();

  // Use a static boolean to check the preference once per game launch.
  // This is a rarely used debug feature, so we try to skip it if possible.
  static bool bLogSkips = PREFSMAN->m_bLogSkips;
  if (bLogSkips) {
    CheckGameLoopTimerSkips(fDeltaTime);
  }

  fDeltaTime *= g_fUpdateRate;

  RageTimer stageTimer;

  // Update SOUNDMAN early (before any RageSound::GetPosition calls), to flush
  // position data.
  SOUNDMAN->Update();
  g_LastUpdateTimingBreakdown.soundManagerUpdate = stageTimer.GetDeltaTime();

  /* Update song beat information -before- calling update on all the classes
   * that depend on it. If you don't do this first, the classes are all acting
   * on old information and will lag. (but no longer fatally, due to
   * timestamping -glenn) */
  stageTimer.Touch();
  SOUND->Update(fDeltaTime);
  g_LastUpdateTimingBreakdown.soundUpdate = stageTimer.GetDeltaTime();

  stageTimer.Touch();
  TEXTUREMAN->Update(fDeltaTime);
  g_LastUpdateTimingBreakdown.textureUpdate = stageTimer.GetDeltaTime();

  stageTimer.Touch();
  GAMESTATE->Update(fDeltaTime);
  g_LastUpdateTimingBreakdown.gameStateUpdate = stageTimer.GetDeltaTime();

  stageTimer.Touch();
  NETWORK->Update();
  g_LastUpdateTimingBreakdown.networkUpdate = stageTimer.GetDeltaTime();

  stageTimer.Touch();
  if (SYNCHRONIZER != nullptr) {
    g_LastUpdateTimingBreakdown.mainThreadSyncTasksRun =
        SYNCHRONIZER->RunMainThreadTasks(
            std::max(1, static_cast<int>(g_iMainThreadSyncTaskBudget.Get())),
            std::max(
                0.0f,
                static_cast<float>(g_fMainThreadSyncTimeBudgetSeconds.Get())));
  }
  g_LastUpdateTimingBreakdown.mainThreadSyncTaskTime =
      stageTimer.GetDeltaTime();

  stageTimer.Touch();
  SCREENMAN->Update(fDeltaTime);
  g_LastUpdateTimingBreakdown.screenUpdate = stageTimer.GetDeltaTime();

  stageTimer.Touch();
  MEMCARDMAN->Update();
  g_LastUpdateTimingBreakdown.memoryCardUpdate = stageTimer.GetDeltaTime();

  /* Important: Process input AFTER updating game logic, or input will be
   * acting on song beat from last frame */
  stageTimer.Touch();
  HandleInputEvents(fDeltaTime);
  g_LastUpdateTimingBreakdown.inputUpdate = stageTimer.GetDeltaTime();

  // Update the lights
  stageTimer.Touch();
  LIGHTSMAN->Update(fDeltaTime);
  g_LastUpdateTimingBreakdown.lightsUpdate = stageTimer.GetDeltaTime();

  g_LastUpdateTimingBreakdown.totalUpdate = updateTimer.GetDeltaTime();
}

void GameLoop::RunGameLoop() {
  /* People may want to do something else while songs are loading, so do
   * this after loading songs. */
  if (ChangeAppPri()) {
    HOOKS->BoostPriority();
  }

  while (!ArchHooks::UserQuit()) {
    if (!g_NewGame.empty()) {
      DoChangeGame();
    }
    if (!g_NewTheme.empty()) {
      DoChangeTheme();
    }

    CheckFocus();

    RageTimer frameTimer;
    UpdateAllButDraw(false);
    const float updateSeconds = frameTimer.GetDeltaTime();

    CallEveryNFrames(500, CheckInputDevices);

    RageTimer drawTimer;
    SCREENMAN->Draw();
    const float drawSeconds = drawTimer.GetDeltaTime();

    if (g_bEnableFrameBudgeting.Get()) {
      const float budgetSeconds =
          std::max(0.0f, static_cast<float>(g_fFrameBudgetSeconds.Get()));
      const float exceedWarnMarginSeconds =
          std::max(0.0f, static_cast<float>(
                             g_fFrameBudgetExceedWarnMarginSeconds.Get()));
      if (budgetSeconds > 0.0f) {
        const float frameSeconds = updateSeconds + drawSeconds;
        if (frameSeconds > (budgetSeconds + exceedWarnMarginSeconds) &&
            ShouldLogFrameBudgetWarning()) {
          const bool likelyDrawBound = IsLikelyDrawBoundFrame(
              frameSeconds, budgetSeconds, updateSeconds, drawSeconds);

          if (!likelyDrawBound || g_bWarnOnLikelyDrawBoundBudgetExceed.Get()) {
            LogFrameBudgetWarning(
                frameSeconds, budgetSeconds, updateSeconds, drawSeconds,
                likelyDrawBound);
          } else {
            const ScreenManager::DrawTimingBreakdown& drawTiming =
                SCREENMAN->GetLastDrawTimingBreakdown();
            const ScreenManager::UpdateTimingBreakdown& screenUpdateTiming =
              SCREENMAN->GetLastUpdateTimingBreakdown();
            const RageDisplay::EndFrameTimingBreakdown& endFrameTiming =
                DISPLAY->GetLastEndFrameTimingBreakdown();
            const InputEventHandlingTimingBreakdown& inputTiming =
                GetLastInputEventHandlingTimingBreakdown();
            const float updateInputShare =
                (updateSeconds > 0.0f)
                    ? (g_LastUpdateTimingBreakdown.inputUpdate / updateSeconds)
                    : 0.0f;
            const float updateScreenShare =
                (updateSeconds > 0.0f)
                    ? (g_LastUpdateTimingBreakdown.screenUpdate / updateSeconds)
                    : 0.0f;
            const float updateScreenDelayShare =
                (updateSeconds > 0.0f)
                    ? (screenUpdateTiming.loadDelayedScreen / updateSeconds)
                    : 0.0f;
            LOG->Trace(
                "Frame near/over budget but likely draw-bound: frame=%.4fs "
                "budget=%.4fs update=%.4fs draw=%.4fs drawBegin=%.4fs "
                "drawScene=%.4fs drawEnd=%.4fs drawEndShare=%.2f updateInputShare=%.2f "
                "updateScreenShare=%.2f updateScreenDelayShare=%.2f inTotal=%.4fs inCount=%d class=%s "
                "endPresent=%.4fs endLimitBefore=%.4fs endLimitAfter=%.4fs "
                "endFinish=%.4fs endWindowUpdate=%.4fs endTotal=%.4fs "
                "endValid=%d scrStack=%.4fs scrOverlay=%.4fs scrDelay=%.4fs scrTotal=%.4fs (set "
                "WarnOnLikelyDrawBoundBudgetExceed=1 to warn)",
                frameSeconds, budgetSeconds, updateSeconds, drawSeconds,
                drawTiming.beginFrame, drawTiming.sceneDraw,
                drawTiming.endFrame,
                (drawSeconds > 0.0f) ? (drawTiming.endFrame / drawSeconds)
                                     : 0.0f,
                updateInputShare,
                updateScreenShare,
                updateScreenDelayShare,
                inputTiming.total,
                inputTiming.eventCount,
                IsLikelyPresentBoundFrame(drawSeconds)
                    ? "likely-present-bound"
                    : (IsLikelyInputUpdateBoundFrame(updateSeconds, budgetSeconds)
                           ? "likely-input-update-bound"
                           : (IsLikelyDelayedScreenLoadBoundFrame(
                                  updateSeconds, budgetSeconds)
                                  ? "likely-screen-delayed-load-bound"
                                  : (IsLikelyScreenUpdateBoundFrame(
                                         updateSeconds, budgetSeconds)
                                         ? "likely-screen-update-bound"
                                         : "likely-draw-bound"))),
                endFrameTiming.presentOrSwap,
                endFrameTiming.frameLimitBeforeVsync,
                endFrameTiming.frameLimitAfterVsync,
                endFrameTiming.finishGpu, endFrameTiming.windowUpdate,
                endFrameTiming.totalEndFrame, endFrameTiming.valid ? 1 : 0,
                screenUpdateTiming.screenStackUpdate,
                screenUpdateTiming.overlayUpdate,
                screenUpdateTiming.loadDelayedScreen,
                screenUpdateTiming.totalUpdate);
          }
        }
      }
    }
  }

  // If we ended mid-game, finish up.
  GAMESTATE->SaveLocalData();

  if (ChangeAppPri()) {
    HOOKS->UnBoostPriority();
  }
}

class ConcurrentRenderer {
 public:
  ConcurrentRenderer();
  ~ConcurrentRenderer();

  void Start();
  void Stop();

 private:
  RageThread m_Thread;
  RageEvent m_Event;
  bool m_bShutdown;
  void RenderThread();
  static int StartRenderThread(void* p);

  enum State {
    RENDERING_IDLE,
    RENDERING_START,
    RENDERING_ACTIVE,
    RENDERING_END
  };
  State m_State;
};
static ConcurrentRenderer* g_pConcurrentRenderer = nullptr;

ConcurrentRenderer::ConcurrentRenderer() : m_Event("ConcurrentRenderer") {
  m_bShutdown = false;
  m_State = RENDERING_IDLE;

  m_Thread.SetName("ConcurrentRenderer");
  m_Thread.Create(StartRenderThread, this);
}

ConcurrentRenderer::~ConcurrentRenderer() {
  ASSERT(m_State == RENDERING_IDLE);
  m_bShutdown = true;
  m_Thread.Wait();
}

void ConcurrentRenderer::Start() {
  DISPLAY->BeginConcurrentRenderingMainThread();

  m_Event.Lock();
  ASSERT(m_State == RENDERING_IDLE);
  m_State = RENDERING_START;
  m_Event.Signal();
  while (m_State != RENDERING_ACTIVE) {
    m_Event.Wait();
  }
  m_Event.Unlock();
}

void ConcurrentRenderer::Stop() {
  m_Event.Lock();
  ASSERT(m_State == RENDERING_ACTIVE);
  m_State = RENDERING_END;
  m_Event.Signal();
  while (m_State != RENDERING_IDLE) {
    m_Event.Wait();
  }
  m_Event.Unlock();

  DISPLAY->EndConcurrentRenderingMainThread();
}

void ConcurrentRenderer::RenderThread() {
  ASSERT(SCREENMAN != nullptr);

  while (!m_bShutdown) {
    m_Event.Lock();
    while (m_State == RENDERING_IDLE && !m_bShutdown) {
      m_Event.Wait();
    }
    m_Event.Unlock();

    if (m_State == RENDERING_START) {
      /* We're starting to render. Set up, and then kick the event to wake
       * up the calling thread. */
      DISPLAY->BeginConcurrentRendering();
      HOOKS->SetupConcurrentRenderingThread();

      LOG->Trace("ConcurrentRenderer::RenderThread start");

      m_Event.Lock();
      m_State = RENDERING_ACTIVE;
      m_Event.Signal();
      m_Event.Unlock();
    }

    /* This is started during Update(). The next thing the game loop
     * will do is Draw, so shift operations around to put Draw at the
     * top. This makes sure updates are seamless. */
    if (m_State == RENDERING_ACTIVE) {
      SCREENMAN->Draw();

      float fDeltaTime = g_GameplayTimer.GetDeltaTime();
      SCREENMAN->Update(fDeltaTime);
    }

    if (m_State == RENDERING_END) {
      LOG->Trace("ConcurrentRenderer::RenderThread done");

      DISPLAY->EndConcurrentRendering();

      m_Event.Lock();
      m_State = RENDERING_IDLE;
      m_Event.Signal();
      m_Event.Unlock();
    }
  }
}

int ConcurrentRenderer::StartRenderThread(void* p) {
  ((ConcurrentRenderer*)p)->RenderThread();
  return 0;
}

void GameLoop::StartConcurrentRendering() {
  if (g_pConcurrentRenderer == nullptr) {
    g_pConcurrentRenderer = new ConcurrentRenderer;
  }
  g_pConcurrentRenderer->Start();
}

void GameLoop::FinishConcurrentRendering() { g_pConcurrentRenderer->Stop(); }

/*
 * (c) 2001-2005 Chris Danford, Glenn Maynard
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
