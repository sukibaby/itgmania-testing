

/**
 * @file FadingBanner.h
 * @brief Header for fading banner display functionality.
 *
 * This header declares the FadingBanner class, which manages smooth
 * transitions between multiple banner images, supporting low-to-high
 * resolution loading and various banner sources.
 *
 * The corresponding implementation is in FadingBanner.cpp.
 *
 * Key dependencies:
 * - Banner.h: Base banner class for individual banner handling.
 * - ActorFrame.h: Base class for actor frame management.
 * - RageTimer.h: Provides timing functionality for fade effects.
 */

#ifndef FADING_BANNER_H
#define FADING_BANNER_H

#include "Banner.h"
#include "ActorFrame.h"
#include "RageTimer.h"

class FadingBanner : public ActorFrame
{
public:
	FadingBanner();
	virtual FadingBanner *Copy() const;

	void ScaleToClipped( float fWidth, float fHeight );

	/* If you previously loaded a cached banner, and are now loading the full-
	 * resolution banner, set bLowResToHighRes to true. */
	void Load( RageTextureID ID, bool bLowResToHighRes=false );
	void LoadFromSong( const Song* pSong );		// nullptr means no song
	void LoadMode();
	void LoadFromSongGroup( RString sSongGroup );
	void LoadFromCourse( const Course* pCourse );
	void LoadIconFromCharacter( Character* pCharacter );
	void LoadBannerFromUnlockEntry( const UnlockEntry* pUE );
	void LoadRoulette();
	void LoadRandom();
	void LoadFromSortOrder( SortOrder so );
	void LoadFallback();
	void LoadCourseFallback();
	void LoadCustom( RString sBanner );

	bool LoadFromCachedBanner( const RString &path );

	void SetMovingFast( bool fast ) { m_bMovingFast=fast; }
	virtual void UpdateInternal( float fDeltaTime );
	virtual void DrawPrimitives();

	int GetLatestIndex(){ return m_iIndexLatest; }
	Banner GetBanner(int i){ return m_Banner[i]; }

	// Lua
	void PushSelf( lua_State *L );

protected:
	void BeforeChange( bool bLowResToHighRes=false );

	static const int NUM_BANNERS = 5;
	Banner	m_Banner[NUM_BANNERS];
	int		m_iIndexLatest;

	bool	m_bMovingFast;
	bool	m_bSkipNextBannerUpdate;
};

#endif

/*
 * (c) 2001-2004 Chris Danford
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
