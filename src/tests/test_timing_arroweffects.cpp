#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <sstream>
#include <string>
#include <thread>

#include "ArrowEffects.h"
#include "GameConstantsAndTypes.h"
#include "GameState.h"
#include "PlayerOptions.h"
#include "RageTimer.h"

// ArrowEffects is the main dependent on accurate time-since-start which is
// -not- using RageTimer directly, because ArrowEffects is using standard float
// values for time. Thus, care has to be taken to ensure that its idea of time
// from start does not deviate from RageTimer's,  and also separately does not
// suffer from inaccuracy at longer runtimes due to loss of precision.

namespace {

constexpr double kFlagS = 0.001;
constexpr double kNoticeableS = 0.005;
constexpr double kExtremeS = 0.010;

const char* Severity(double deviation) {
  const double d = std::fabs(deviation);
  if (d > kExtremeS) {
    return "EXTREME FAULT (>10ms)";
  }
  if (d > kNoticeableS) {
    return "NOTICEABLE (>5ms)";
  }
  if (d > kFlagS) {
    return "FLAGGED (>1ms)";
  }
  return "ok";
}

::testing::AssertionResult WithinSyncBudget(double expected, double actual) {
  const double dev = actual - expected;
  if (std::fabs(dev) <= kFlagS) {
    return ::testing::AssertionSuccess();
  }
  std::ostringstream os;
  os.precision(9);
  os << Severity(dev) << ": expected " << expected << " s, got " << actual
     << " s, deviation " << dev * 1000.0 << " ms";
  return ::testing::AssertionFailure() << os.str();
}

// Half the float spacing at |v|: the best any float result can do.
double HalfUlp(double v) {
  const float f = static_cast<float>(v);
  return 0.5 * (static_cast<double>(std::nextafter(f, INFINITY)) -
                static_cast<double>(f));
}

double SteadyNow() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct Checkpoint {
  const char* name;
  uint64_t seconds;
};
constexpr uint64_t kMin = 60, kHour = 3600, kDay = 24 * kHour;
const Checkpoint kCheckpoints[] = {
    {"immediate(10s)", 10},
    {"5m", 5 * kMin},
    {"15m", 15 * kMin},
    {"30m", 30 * kMin},
    {"45m", 45 * kMin},
    {"1h", kHour},
    {"1h05m", kHour + 5 * kMin},
    {"1h15m", kHour + 15 * kMin},
    {"1h30m", kHour + 30 * kMin},
    {"1h45m", kHour + 45 * kMin},
    {"2h", 2 * kHour},
    {"2h05m", 2 * kHour + 5 * kMin},
    {"2h15m", 2 * kHour + 15 * kMin},
    {"2h30m", 2 * kHour + 30 * kMin},
    {"2h45m", 2 * kHour + 45 * kMin},
    {"3h", 3 * kHour},
    {"4h", 4 * kHour},
    {"5h", 5 * kHour},
    {"6h", 6 * kHour},
    {"24h", 24 * kHour},
    {"48h", 48 * kHour},
    {"256h", 256 * kHour},
    {"7d", 7 * kDay},
    {"30d", 30 * kDay},
    {"90d", 30 * kDay},
};

// We have to hardcode our own player options here since otherwise ArrowEffects
// will try to call PREFSMAN and that will obviously crash.
// Here we will define two player scenarios, a "P1"
// which is using the settings of C800 with no unusual time based modifiers
// (e.g. drunk, dizzy, etc) and another "P2" which is using 1.5X for speed mod
// and all of the time-based visual mods. Zero is a valid value for every field
// here (floats == 0, enums == first value, i.e. ModTimerType_Default).
class ArrowEffectsTimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::memset(storage_, 0, sizeof(storage_));
    ArrowEffects::SetCurrentOptions(options());
  }
  void TearDown() override { ArrowEffects::SetCurrentOptions(nullptr); }

  PlayerOptions* options() {
    return std::launder(reinterpret_cast<PlayerOptions*>(storage_));
  }

  // C800, no time based mods.
  void ApplySettingsWithoutTimeBasedMods() {
    PlayerOptions* o = options();
    o->time_spacing_ = 1;  // C-mods scroll by time
    o->scroll_bpm_ = 800;  // C800
    o->scroll_speed_ = 1;
    o->max_scroll_bpm_ = 0;
  }

  // 1.5x plus every mod that feeds on GetTime(): drunk (x/z and tan variants,
  // with speed), tipsy (and tan, with speed), blink; dizzy
  void ApplySettingsWithTimeBasedMods() {
    PlayerOptions* o = options();
    o->time_spacing_ = 0;
    o->scroll_bpm_ = 0;
    o->scroll_speed_ = 1.5f;
    o->max_scroll_bpm_ = 0;
    for (float* e :
         {&o->effects_[PlayerOptions::EFFECT_DRUNK],
          &o->effects_[PlayerOptions::EFFECT_DRUNK_SPEED],
          &o->effects_[PlayerOptions::EFFECT_DRUNK_Z],
          &o->effects_[PlayerOptions::EFFECT_DRUNK_Z_SPEED],
          &o->effects_[PlayerOptions::EFFECT_TAN_DRUNK],
          &o->effects_[PlayerOptions::EFFECT_TAN_DRUNK_SPEED],
          &o->effects_[PlayerOptions::EFFECT_TAN_DRUNK_Z],
          &o->effects_[PlayerOptions::EFFECT_TAN_DRUNK_Z_SPEED],
          &o->effects_[PlayerOptions::EFFECT_TIPSY],
          &o->effects_[PlayerOptions::EFFECT_TIPSY_SPEED],
          &o->effects_[PlayerOptions::EFFECT_TAN_TIPSY],
          &o->effects_[PlayerOptions::EFFECT_TAN_TIPSY_SPEED],
          &o->effects_[PlayerOptions::EFFECT_DIZZY]}) {
      *e = 1.0f;
    }
    o->appearances_[PlayerOptions::APPEARANCE_BLINK] = 1.0f;
  }

  // GetTime() must be exactly the steady clock (plus offset*mult) no matter
  // what speed or mods are set: the clock cannot depend on them.
  void ExpectClockUnaffected(float offset = 0.0f, float mult_mod = 0.0f) {
    options()->mod_timer_type_ = ModTimerType_Default;
    options()->mod_timer_offset_ = offset;
    options()->mod_timer_mult_ = mult_mod;
    const double add = static_cast<double>(offset) * (1.0 + mult_mod);

    const double before = SteadyNow();
    const float t = ArrowEffects::GetTime();
    const double after = SteadyNow();
    const double slack = HalfUlp(after + add) + 1e-6;
    EXPECT_GE(static_cast<double>(t), before + add - slack);
    EXPECT_LE(static_cast<double>(t), after + add + slack);
  }

  alignas(PlayerOptions) unsigned char storage_[sizeof(PlayerOptions)];
};

// Scenario A: C800, no time-based (or any) mods

TEST_F(ArrowEffectsTimeTest, NoTimeBasedMods_TracksSteadyClock) {
  ApplySettingsWithoutTimeBasedMods();
  ExpectClockUnaffected();
  RecordProperty("half_ulp_ms", std::to_string(HalfUlp(SteadyNow()) * 1000.0));
}

TEST_F(ArrowEffectsTimeTest, NoTimeBasedMods_OffsetAndMultiplier) {
  ApplySettingsWithoutTimeBasedMods();
  ExpectClockUnaffected(2.5f, 0.0f);   // +2.5
  ExpectClockUnaffected(2.5f, 1.0f);   // +5.0
  ExpectClockUnaffected(-4.0f, 0.5f);  // -6.0
  ExpectClockUnaffected(1.0f, -1.0f);  // +0.0
}

TEST_F(ArrowEffectsTimeTest, NoTimeBasedMods_MonotonicAndAdvances) {
  ApplySettingsWithoutTimeBasedMods();
  float prev = ArrowEffects::GetTime();
  for (int i = 0; i < 10000; ++i) {
    const float cur = ArrowEffects::GetTime();
    ASSERT_GE(cur, prev);
    prev = cur;
  }
  const float a = ArrowEffects::GetTime();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const float b = ArrowEffects::GetTime();
  const double tol = 2 * HalfUlp(static_cast<double>(b)) + 0.002;
  EXPECT_NEAR(static_cast<double>(b - a), 0.3, tol + 0.05);
}

// Scenario B: 1.5x with every time-based mod enabled

TEST_F(ArrowEffectsTimeTest, WithTimeBasedMods_TracksSteadyClock) {
  ApplySettingsWithTimeBasedMods();
  ExpectClockUnaffected();
}

TEST_F(ArrowEffectsTimeTest, WithTimeBasedMods_OffsetAndMultiplier) {
  ApplySettingsWithTimeBasedMods();
  ExpectClockUnaffected(2.5f, 0.0f);
  ExpectClockUnaffected(2.5f, 1.0f);
  ExpectClockUnaffected(-4.0f, 0.5f);
}

TEST_F(ArrowEffectsTimeTest, WithTimeBasedMods_MonotonicAndAdvances) {
  ApplySettingsWithTimeBasedMods();
  float prev = ArrowEffects::GetTime();
  for (int i = 0; i < 10000; ++i) {
    const float cur = ArrowEffects::GetTime();
    ASSERT_GE(cur, prev);
    prev = cur;
  }
  const float a = ArrowEffects::GetTime();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const float b = ArrowEffects::GetTime();
  const double tol = 2 * HalfUlp(static_cast<double>(b)) + 0.002;
  EXPECT_NEAR(static_cast<double>(b - a), 0.3, tol + 0.05);
}

TEST_F(ArrowEffectsTimeTest, DoBothPlayersTimeValuesAgree) {
  // Same clock regardless of speed/mods, and same epoch as RageTimer.
  ApplySettingsWithoutTimeBasedMods();
  const RageTimer ref_a;
  const float a = ArrowEffects::GetTime();
  ApplySettingsWithTimeBasedMods();
  const float b = ArrowEffects::GetTime();
  const RageTimer ref_b;
  const double ra = static_cast<double>(ref_a.secs_) + ref_a.us_ / 1e6;
  const double rb = static_cast<double>(ref_b.secs_) + ref_b.us_ / 1e6;
  EXPECT_NEAR(static_cast<double>(a), ra, HalfUlp(ra) + 0.002);
  EXPECT_NEAR(static_cast<double>(b), rb, HalfUlp(rb) + 0.002);
  EXPECT_GE(b, a);
}

// Timer types

TEST_F(ArrowEffectsTimeTest, DefaultAndGameTimerTypesMatch) {
  ApplySettingsWithoutTimeBasedMods();
  options()->mod_timer_type_ = ModTimerType_Default;
  const float d = ArrowEffects::GetTime();
  options()->mod_timer_type_ = ModTimerType_Game;
  const float g = ArrowEffects::GetTime();
  EXPECT_NEAR(
      static_cast<double>(d), static_cast<double>(g),
      2 * HalfUlp(static_cast<double>(d)) + 0.002);
}

// Float precision budget at long uptimes (synthetic clock values)

// Worst deviation of float(t) from t over a sweep of fractional seconds, using
// the same narrowing GetTime() applies to its double result.
double WorstNarrowingDeviation(uint64_t whole_seconds) {
  double worst = 0.0;
  for (int i = 0; i < 1000; ++i) {
    const double t = static_cast<double>(whole_seconds) + i / 1000.0;
    const double dev = static_cast<double>(static_cast<float>(t)) - t;
    worst = std::max(worst, std::fabs(dev));
  }
  return worst;
}

TEST(ArrowEffectsTimePrecision, CheckpointsStayWithinSyncBudget) {
  for (const Checkpoint& c : kCheckpoints) {
    SCOPED_TRACE(c.name);
    const double worst = WorstNarrowingDeviation(c.seconds);
    // WithinSyncBudget(expected, actual): pass the worst-case error as actual.
    EXPECT_TRUE(WithinSyncBudget(0.0, worst))
        << "clock value " << c.seconds << " s";
  }
}

TEST(ArrowEffectsTimePrecision, FrameToFrameStepKeepsItsLength) {
  // A 60 Hz frame advances the clock by 1/60 s. Drunk/tipsy/blink are driven
  // by this value, so quantised steps show up as visible stutter.
  const double frame = 1.0 / 60.0;
  for (const Checkpoint& c : kCheckpoints) {
    SCOPED_TRACE(c.name);
    double worst = 0.0;
    for (int k = 0; k < 600; ++k) {
      const double t0 = static_cast<double>(c.seconds) + k * frame;
      const double step = static_cast<double>(static_cast<float>(t0 + frame)) -
                          static_cast<double>(static_cast<float>(t0));
      worst = std::max(worst, std::fabs(step - frame));
    }
    EXPECT_TRUE(WithinSyncBudget(0.0, worst));
  }
}

}  // namespace