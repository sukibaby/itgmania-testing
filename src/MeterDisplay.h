/**
 * @file MeterDisplay.h
 * @brief Header file for the MeterDisplay class, providing a visual meter
 *        display with stream and tip elements.
 *
 * The MeterDisplay class inherits from ActorFrame and renders a progress
 * meter using stream and tip actors. It supports setting percentage and
 * stream width. The SongMeterDisplay subclass adds update functionality
 * for song-related meters.
 *
 * Dependencies:
 * - ActorFrame.h: Base class for composite actors containing multiple child
 *   actors.
 * - AutoActor.h: Classes for automatic actor loading and management.
 */

#ifndef METER_DISPLAY_H
#define METER_DISPLAY_H

#include "ActorFrame.h"
#include "AutoActor.h"


class MeterDisplay : public ActorFrame
{
public:
	MeterDisplay();
	void Load( RString sStreamPath, float fStreamWidth, RString sTipPath );
	virtual void LoadFromNode( const XNode* pNode );
	virtual MeterDisplay *Copy() const;

	void SetPercent( float fPercent );
	void SetStreamWidth( float fStreamWidth );

	// Lua
	void PushSelf( lua_State *L );

private:
	float	m_fStreamWidth;
	float	m_fPercent;
	AutoActor  m_sprStream;
	AutoActor  m_sprTip;
};

class SongMeterDisplay: public MeterDisplay 
{
public:
	virtual void Update( float fDeltaTime );
	virtual SongMeterDisplay *Copy() const;
};

#endif

/*
 * (c) 2003-2004 Chris Danford
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
