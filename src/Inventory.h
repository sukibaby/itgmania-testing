/**
 * @file Inventory.h
 * @brief Header file for the Inventory class, managing battle mode items.
 *
 * This header defines the Inventory class, which inherits from Actor and
 * handles item acquisition, usage, and effects in battle gameplay mode.
 *
 * The corresponding source file (Inventory.cpp) implements item loading
 * from themes, awarding based on combo, and applying item modifiers.
 *
 * Significant dependencies:
 * - Actor.h: Provides the base Actor class for scene graph integration.
 * - PlayerNumber.h: Defines PlayerNumber for player-specific contexts.
 * - RageSound.h: Supplies RageSound for item-related audio feedback.
 * - ScreenMessage.h: Offers ScreenMessage for battle damage notifications.
 */

#ifndef Inventory_H
#define Inventory_H

#include "Actor.h"
#include "PlayerNumber.h"
#include "RageSound.h"
#include "ScreenMessage.h"

#include <vector>


AutoScreenMessage( SM_BattleDamageLevel1 );
AutoScreenMessage( SM_BattleDamageLevel2 );
AutoScreenMessage( SM_BattleDamageLevel3 );

class PlayerState;
/** @brief Inventory management for PLAY_MODE_BATTLE. */
class Inventory : public Actor
{
public:
	Inventory();
	~Inventory();
	void Load( PlayerState* pPlayerState );

	virtual void Update( float fDelta );
	virtual void DrawPrimitives() {};

	void UseItem( int iSlot );

protected:
	void AwardItem( int iItemIndex );

	PlayerState* m_pPlayerState;
	unsigned int m_iLastSeenCombo;

	/** @brief a sound played when an item has been acquired. */
	RageSound m_soundAcquireItem;
	std::vector<RageSound*> m_vpSoundUseItem;
	RageSound m_soundItemEnding;
};

#endif

/**
 * @file
 * @author Chris Danford (c) 2003
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
