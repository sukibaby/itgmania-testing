/**
 * @file LifeMeterTime.h
 * @brief Header file for the LifeMeterTime class, implementing time-
 *        based life meters for Survival mode.
 *
 * This header defines the LifeMeterTime class, which inherits from
 * LifeMeter and manages life as remaining time with gain/loss mechanics.
 *
 * The corresponding source file (LifeMeterTime.cpp) implements time
 * calculations, theme metrics, and life change handling for survival.
 *
 * Significant dependencies:
 * - LifeMeter.h: Provides the base LifeMeter class interface.
 * - Sprite.h: Supplies Sprite for graphical elements.
 * - BitmapText.h: Offers BitmapText for text rendering.
 * - RageSound.h: Provides RageSound for life change audio.
 * - PercentageDisplay.h: Supplies PercentageDisplay for life display.
 * - AutoActor.h: Offers AutoActor for automatic theme-based loading.
 * - MeterDisplay.h: Provides MeterDisplay for meter visualization.
 * - Quad.h: Supplies Quad for geometric shapes.
 */

#ifndef LifeMeterTime_H
#define LifeMeterTime_H

#include "LifeMeter.h"
#include "Sprite.h"
#include "BitmapText.h"
#include "RageSound.h"
#include "PercentageDisplay.h"
#include "AutoActor.h"
#include "MeterDisplay.h"
#include "Quad.h"
class StreamDisplay;
/** @brief Battery life meter used in Survival. */
class LifeMeterTime : public LifeMeter
{
public:
	LifeMeterTime();

	virtual ~LifeMeterTime();

	virtual void Load( const PlayerState *pPlayerState, PlayerStageStats *pPlayerStageStats );

	virtual void Update( float fDeltaTime );

	virtual void OnLoadSong();
	virtual void ChangeLife( TapNoteScore score );
	virtual void ChangeLife( HoldNoteScore score, TapNoteScore tscore );
	virtual void ChangeLife(float delta);
	virtual void SetLife(float value);
	virtual void HandleTapScoreNone();
	virtual bool IsInDanger() const;
	virtual bool IsHot() const;
	virtual bool IsFailing() const;
	virtual float GetLife() const;

private:
	float GetLifeSeconds() const;
	void SendLifeChangedMessage( float fOldLife, TapNoteScore tns, HoldNoteScore hns );

	AutoActor		m_sprBackground;
	Quad			m_quadDangerGlow;
	StreamDisplay*	m_pStream;
	AutoActor		m_sprFrame;

	float		m_fLifeTotalGainedSeconds;
	float		m_fLifeTotalLostSeconds;
	/** @brief The sound played when time is gained at the start of each Song. */
	RageSound	m_soundGainLife;
};


#endif

/**
 * @file
 * @author Chris Danford (c) 2001-2004
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
