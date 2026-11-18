#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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

class ArrowEffectsTimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    options_.mod_timer_type_ = ModTimerType_Default;
    options_.mod_timer_mult_ = 0.0f;
    options_.mod_timer_offset_ = 0.0f;
    ArrowEffects::SetCurrentOptions(&options_);
  }
  void TearDown() override { ArrowEffects::SetCurrentOptions(nullptr); }
  PlayerOptions options_;
};

// GetTime() tests based on ArrowEffects' version of the function

TEST_F(ArrowEffectsTimeTest, TracksSteadyClockWithinFloatQuantisation) {
  const double before = SteadyNow();
  const float t = ArrowEffects::GetTime();
  const double after = SteadyNow();
  const double slack = HalfUlp(after) + 1e-9;
  EXPECT_GE(static_cast<double>(t), before - slack);
  EXPECT_LE(static_cast<double>(t), after + slack);
  // Informational: how much float error this machine's uptime costs.
  RecordProperty("clock_epoch_seconds", static_cast<int>(after));
  RecordProperty("half_ulp_ms", std::to_string(HalfUlp(after) * 1000.0));
}

TEST_F(ArrowEffectsTimeTest, SharesEpochWithRageTimer) {
  const RageTimer ref;
  const float t = ArrowEffects::GetTime();
  const double ref_s =
      static_cast<double>(ref.secs_) + static_cast<double>(ref.us_) / 1e6;
  EXPECT_NEAR(static_cast<double>(t), ref_s, HalfUlp(ref_s) + 0.002);
}

TEST_F(ArrowEffectsTimeTest, IsMonotonic) {
  float prev = ArrowEffects::GetTime();
  for (int i = 0; i < 10000; ++i) {
    const float cur = ArrowEffects::GetTime();
    ASSERT_GE(cur, prev);
    prev = cur;
  }
}

TEST_F(ArrowEffectsTimeTest, AdvancesWithSleep) {
  const float a = ArrowEffects::GetTime();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const float b = ArrowEffects::GetTime();
  // Quantisation can eat up to one float step at each end.
  const double tol = 2 * HalfUlp(static_cast<double>(b)) + 0.002;
  EXPECT_NEAR(static_cast<double>(b - a), 0.3, tol + 0.05);
  EXPECT_GE(b - a, 0.29f - static_cast<float>(tol));
}

TEST_F(ArrowEffectsTimeTest, OffsetAndMultiplierApply) {
  // GetTime() = clock + offset * (1 + mult).
  struct Case {
    float offset, mult;
    double expected_add;
  };
  const Case cases[] = {
      {0.0f, 0.0f, 0.0},   {2.5f, 0.0f, 2.5},  {2.5f, 1.0f, 5.0},
      {-4.0f, 0.5f, -6.0}, {1.0f, -1.0f, 0.0},
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(
        ::testing::Message() << "offset=" << c.offset << " mult=" << c.mult);
    options_.mod_timer_offset_ = c.offset;
    options_.mod_timer_mult_ = c.mult;
    const double before = SteadyNow();
    const float t = ArrowEffects::GetTime();
    const double after = SteadyNow();
    const double slack = HalfUlp(after) + 1e-6;
    EXPECT_GE(static_cast<double>(t), before + c.expected_add - slack);
    EXPECT_LE(static_cast<double>(t), after + c.expected_add + slack);
  }
}

TEST_F(ArrowEffectsTimeTest, DefaultAndGameTimerTypesMatch) {
  options_.mod_timer_type_ = ModTimerType_Default;
  const float d = ArrowEffects::GetTime();
  options_.mod_timer_type_ = ModTimerType_Game;
  const float g = ArrowEffects::GetTime();
  EXPECT_NEAR(
      static_cast<double>(d), static_cast<double>(g),
      2 * HalfUlp(static_cast<double>(d)) + 0.002);
}

// Needs a live GAMESTATE, which test_main.cpp does not create.
// NOTE: as written, GetTime() contains `modtimer == ModTimerType_Default ||
// ModTimerType_Game`, which is always true, so the Beat and Song branches are
// unreachable; with a GAMESTATE present these tests catch that.
TEST_F(ArrowEffectsTimeTest, BeatTimerUsesSongBeatVisible) {
  if (GAMESTATE == nullptr) {
    GTEST_SKIP() << "GAMESTATE not initialised";
  }
  GAMESTATE->position_.song_beat_visible_ = 8.0f;
  options_.mod_timer_type_ = ModTimerType_Beat;
  options_.mod_timer_offset_ = 2.0f;
  options_.mod_timer_mult_ = 1.0f;  // mult = 2
  EXPECT_FLOAT_EQ(ArrowEffects::GetTime(), (8.0f + 2.0f) * 2.0f);
}

TEST_F(ArrowEffectsTimeTest, SongTimerUsesMusicSeconds) {
  if (GAMESTATE == nullptr) {
    GTEST_SKIP() << "GAMESTATE not initialised";
  }
  GAMESTATE->position_.music_seconds_ = 90.0f;
  options_.mod_timer_type_ = ModTimerType_Song;
  options_.mod_timer_offset_ = 1.0f;
  options_.mod_timer_mult_ = 0.0f;  // mult = 1
  EXPECT_FLOAT_EQ(ArrowEffects::GetTime(), 91.0f);
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
