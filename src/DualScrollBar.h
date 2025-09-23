/**
 * @file DualScrollBar.h
 * @brief Header file for the DualScrollBar class, dual-player scrollbar widget.
 *
 * This file defines the DualScrollBar class which provides a scrollbar widget
 * with two independent thumbs for displaying progress of two players. The
 * implementation is in DualScrollBar.cpp.
 *
 * Significant dependencies:
 * - ActorFrame.h: Base class for UI elements and actor management.
 * - AutoActor.h: AutoActor class for automatic sprite resource management.
 * - PlayerNumber.h: Defines PlayerNumber enum for identifying players.
 */

#ifndef DUAL_SCROLLBAR_H
#define DUAL_SCROLLBAR_H

#include "ActorFrame.h"
#include "AutoActor.h"
#include "PlayerNumber.h"
/** @brief A scrollbar with two independent thumbs. */
class DualScrollBar: public ActorFrame
{
public:
	DualScrollBar();

	void Load( const RString &sType );
	void SetBarHeight( float fHeight ) { m_fBarHeight = fHeight; }
	void SetBarTime( float fTime ) { m_fBarTime = fTime; }
	void SetPercentage( PlayerNumber pn, float fPercent );
	void EnablePlayer( PlayerNumber pn, bool on );

private:
	/** @brief The height of the scrollbar. */
	float	m_fBarHeight;
	float	m_fBarTime;

	AutoActor	m_sprScrollThumbOverHalf[NUM_PLAYERS];
	AutoActor	m_sprScrollThumbUnderHalf[NUM_PLAYERS];
};

#endif

/**
 * @file
 * @author Glenn Maynard, Chris Danford (c) 2001-2004
 * @section LICENSE
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
