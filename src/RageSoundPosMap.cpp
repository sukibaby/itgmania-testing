#include "global.h"
#include "RageSoundPosMap.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageTimer.h"

#include <cinttypes>
#include <climits>
#include <cmath>
#include <cstdint>
#include <deque>

// NOTE(sukibaby): The number of frames we should keep pos_map data for.
// File bitrate, metadata, etc will factor in here. 80k is a safe value
// to provide a good balance of stability and low latency. It is stable
// up to 200k, but increased latency is the main reason not to increase
// this to a very large number. 
static int pos_map_backlog_frames = 80000;

struct pos_map_t
{
	int64_t m_iSourceFrame;      // Start source frame
	int64_t m_iDestFrame;        // Start dest frame
	int64_t m_iFrames;           // Length in source frames
	double  m_fSourceToDestRatio;
	int64_t m_iDestEndCached;    // Cached: m_iDestFrame + round(m_iFrames * ratio)
	int64_t m_iSourceEndCached;  // Cached: m_iSourceFrame + m_iFrames

	pos_map_t()
		: m_iSourceFrame(0), m_iDestFrame(0), m_iFrames(0),
		  m_fSourceToDestRatio(1.0),
		  m_iDestEndCached(0), m_iSourceEndCached(0) {}

	pos_map_t(int64_t s, int64_t frames, int64_t d, double r)
	{
		m_iSourceFrame = s;
		m_iFrames = frames;
		m_iDestFrame = d;
		m_fSourceToDestRatio = r;
		UpdateCaches();
	}

	inline void UpdateCaches()
	{
		m_iSourceEndCached = m_iSourceFrame + m_iFrames;
		// Use lrint-style rounding: add 0.5 and cast.
		m_iDestEndCached = m_iDestFrame + static_cast<int64_t>( (m_iFrames * m_fSourceToDestRatio) + 0.5 );
	}

	inline bool ContainsSource(int64_t src) const
	{
		// Use unsigned comparison to avoid negative range issues.
		return static_cast<uint64_t>(src - m_iSourceFrame) < static_cast<uint64_t>(m_iFrames);
	}
};

struct pos_map_impl
{
	std::deque<pos_map_t> m_Queue;
	int64_t               m_TotalFrames = 0;      // Total frames across all segments
	mutable size_t        m_LastIndex = 0;        // Cursor for fast Search

	void Cleanup()
	{
		// Remove oldest segments while we still exceed backlog after removal.
		// Keep at least pos_map_backlog_frames.
		while (!m_Queue.empty())
		{
			const pos_map_t& front = m_Queue.front();
			if ( (m_TotalFrames - front.m_iFrames) < pos_map_backlog_frames )
				break;
			m_TotalFrames -= front.m_iFrames;
			m_Queue.pop_front();
			if (m_LastIndex) --m_LastIndex; // Adjust cursor
		}
		if (m_LastIndex >= m_Queue.size())
			m_LastIndex = m_Queue.empty() ? 0 : m_Queue.size() - 1;
	}
};

pos_map_queue::pos_map_queue()
{
	m_pImpl = new pos_map_impl;
}

pos_map_queue::~pos_map_queue()
{
	delete m_pImpl;
}

pos_map_queue::pos_map_queue( const pos_map_queue &cpy )
{
	*this = cpy;
	m_pImpl = new pos_map_impl( *cpy.m_pImpl );
}

pos_map_queue &pos_map_queue::operator=( const pos_map_queue &rhs )
{
	if (this != &rhs)
	{
		pos_map_impl* tempImpl = new pos_map_impl(*rhs.m_pImpl);
		std::swap(m_pImpl, tempImpl);
		delete tempImpl;
	}
	return *this;
}

void pos_map_queue::Insert(int64_t iSourceFrame, int64_t iFrames, int64_t iDestFrame, double fSourceToDestRatio)
{
	pos_map_impl& impl = *m_pImpl;
	if (!impl.m_Queue.empty())
	{
		pos_map_t &last = impl.m_Queue.back();
		if (last.m_iSourceEndCached == iSourceFrame &&
			last.m_fSourceToDestRatio == fSourceToDestRatio)
		{
			// Predict expected next dest start
			const int64_t expectedDestNext =
				last.m_iDestEndCached;
			const int64_t diff = expectedDestNext - iDestFrame;
			if (diff >= -1 && diff <= 1)
			{
				last.m_iFrames += iFrames;
				last.UpdateCaches();
				impl.m_TotalFrames += iFrames;
				impl.Cleanup();
				return;
			}
		}
	}

	impl.m_Queue.emplace_back(iSourceFrame, iFrames, iDestFrame, fSourceToDestRatio);
	impl.m_TotalFrames += iFrames;
	impl.Cleanup();
}

int64_t pos_map_queue::Search( int64_t iSourceFrame ) const
{
	const pos_map_impl& impl = *m_pImpl;
	const auto& q = impl.m_Queue;
	if (q.empty())
	{
		return 0;
	}

	// Cursor-based local search
	size_t idx = impl.m_LastIndex;
	if (idx >= q.size())
	{
		idx = q.size() - 1;
	}

	// Move forward while we're beyond current segment
	while (idx + 1 < q.size() && iSourceFrame >= q[idx].m_iSourceEndCached)
	{
		++idx;
	}

	// Move backward if we've overshot
	while (idx > 0 && iSourceFrame < q[idx].m_iSourceFrame)
	{
		--idx;
	}

	const pos_map_t& seg = q[idx];
	if (seg.ContainsSource(iSourceFrame))
	{
		// Compute dest offset
		int64_t delta = iSourceFrame - seg.m_iSourceFrame;
		int64_t mapped =
			seg.m_iDestFrame +
			static_cast<int64_t>((delta * seg.m_fSourceToDestRatio) + 0.5);
		impl.m_LastIndex = idx;  // Update cursor
		return mapped;
	}

	// Clamp to nearest end if out of range
	if (iSourceFrame < q.front().m_iSourceFrame)
	{
		return q.front().m_iDestFrame;
	}
	const pos_map_t& last = q.back();
	return last.m_iDestEndCached;
}

void pos_map_queue::Clear()
{
	m_pImpl->m_Queue.clear();
	m_pImpl->m_TotalFrames = 0;
	m_pImpl->m_LastIndex = 0;
}

bool pos_map_queue::IsEmpty() const
{
	return m_pImpl->m_Queue.empty();
}

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
