#include "RageSoundPosMap.h"

#include <cmath>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <utility>

#include "RageLog.h"
#include "RageTimer.h"

// Number of frames to retain in the mapping queue.  A larger history makes
// frame conversion resilient to scheduler jitter and delayed position polling,
// reducing extrapolation/clamping artifacts in sync-sensitive play.
static constexpr int64_t pos_map_backlog_frames = 441000;  // ~10s at 44.1kHz

struct pos_map_t {
  int64_t m_iSourceFrame;
  int64_t m_iDestFrame;
  int64_t m_iFrames;
  double m_fSourceToDestRatio;

  pos_map_t() {
    m_iSourceFrame = 0;
    m_iDestFrame = 0;
    m_iFrames = 0;
    m_fSourceToDestRatio = 1.0;
  }
};

struct pos_map_impl {
  std::deque<pos_map_t> m_Queue;
  void Cleanup();
};

namespace {
int64_t ScaleFramesWithRatio(int64_t sourceFrames, double sourceToDestRatio) {
  const long double scaled =
      static_cast<long double>(sourceFrames) * sourceToDestRatio;
  const long double rounded = std::llround(scaled);

  const long double minValue =
      static_cast<long double>(std::numeric_limits<int64_t>::min());
  const long double maxValue =
      static_cast<long double>(std::numeric_limits<int64_t>::max());

  if (rounded < minValue) {
    return std::numeric_limits<int64_t>::min();
  }
  if (rounded > maxValue) {
    return std::numeric_limits<int64_t>::max();
  }

  return static_cast<int64_t>(rounded);
}
}  // namespace

pos_map_queue::pos_map_queue() { m_pImpl = new pos_map_impl; }

pos_map_queue::~pos_map_queue() { delete m_pImpl; }

pos_map_queue::pos_map_queue(const pos_map_queue& cpy) {
  *this = cpy;
  m_pImpl = new pos_map_impl(*cpy.m_pImpl);
}

pos_map_queue& pos_map_queue::operator=(const pos_map_queue& rhs) {
  if (this != &rhs) {
    pos_map_impl* tempImpl = new pos_map_impl(*rhs.m_pImpl);
    std::swap(m_pImpl, tempImpl);
    delete tempImpl;
  }
  return *this;
}

void pos_map_queue::Insert(
    int64_t iSourceFrame, int64_t iFrames, int64_t iDestFrame,
    double fSourceToDestRatio) {
  bool merged = false;
  if (!m_pImpl->m_Queue.empty()) {
    // Check if the last entry can be merged with the new entry
    pos_map_t& last = m_pImpl->m_Queue.back();
    const int64_t expectedNextDestFrame =
      last.m_iDestFrame +
      ScaleFramesWithRatio(last.m_iFrames, last.m_fSourceToDestRatio);
    if (last.m_iSourceFrame + last.m_iFrames == iSourceFrame &&
        last.m_fSourceToDestRatio == fSourceToDestRatio &&

        // llabs() is used instead of abs() because abs() would be susceptible
        // to an integer overflow.
      llabs(expectedNextDestFrame - iDestFrame) <= 1)

    {
      // Merge the frames and set the merged flag to true.
      last.m_iFrames += iFrames;
      merged = true;
    }
  }

  if (!merged) {
    m_pImpl->m_Queue.emplace_back();
    pos_map_t& m = m_pImpl->m_Queue.back();
    m.m_iSourceFrame = iSourceFrame;
    m.m_iDestFrame = iDestFrame;
    m.m_iFrames = iFrames;
    m.m_fSourceToDestRatio = fSourceToDestRatio;
  }

  m_pImpl->Cleanup();
}

void pos_map_impl::Cleanup() {
  auto it = m_Queue.end();
  int64_t iTotalFrames = 0;
  // Scan backwards until we have at least pos_map_backlog_frames.
  while (iTotalFrames < pos_map_backlog_frames) {
    if (it == m_Queue.begin()) {
      break;
    }
    --it;
    iTotalFrames += it->m_iFrames;
  }

  m_Queue.erase(m_Queue.begin(), it);
}

int64_t pos_map_queue::Search(int64_t iSourceFrame) const {
  if (IsEmpty()) {
    return 0;
  }

  const pos_map_t& first = m_pImpl->m_Queue.front();
  const pos_map_t& last = m_pImpl->m_Queue.back();

  const int64_t firstStart = first.m_iSourceFrame;
  const int64_t lastEnd = last.m_iSourceFrame + last.m_iFrames;

  if (iSourceFrame < firstStart) {
    const int64_t sourceOffset = iSourceFrame - firstStart;
    return first.m_iDestFrame +
           ScaleFramesWithRatio(sourceOffset, first.m_fSourceToDestRatio);
  }

  if (iSourceFrame >= lastEnd) {
    const int64_t lastDestEnd =
        last.m_iDestFrame +
        ScaleFramesWithRatio(last.m_iFrames, last.m_fSourceToDestRatio);
    const int64_t sourceOffset = iSourceFrame - lastEnd;
    return lastDestEnd +
           ScaleFramesWithRatio(sourceOffset, last.m_fSourceToDestRatio);
  }

  // iSourceFrame is probably in pos_map.  Search to figure out what position it
  // maps to.
  for (const pos_map_t& pm : m_pImpl->m_Queue) {
    // Loop over the queue until we know generally where iSourceFrame is
    if (iSourceFrame >= pm.m_iSourceFrame &&
        iSourceFrame < pm.m_iSourceFrame + pm.m_iFrames) {
      // If we are in the correct block, calculate its current position
      int64_t iDiff = static_cast<int64_t>(iSourceFrame - pm.m_iSourceFrame);
      iDiff = ScaleFramesWithRatio(iDiff, pm.m_fSourceToDestRatio);
      return pm.m_iDestFrame + iDiff;
    }
  }

  // Should not be reachable due to range checks above.
  static RageTimer lastLog;
  if (lastLog.Ago() >= 1.0f) {
    lastLog.Touch();
    LOG->Trace("Audio position search fell through unexpectedly at frame %" PRId64,
               iSourceFrame);
  }
  return first.m_iDestFrame;
}

void pos_map_queue::Clear() { m_pImpl->m_Queue.clear(); }

bool pos_map_queue::IsEmpty() const { return m_pImpl->m_Queue.empty(); }

/*
 * Copyright (c) 2002-2004 Glenn Maynard
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
