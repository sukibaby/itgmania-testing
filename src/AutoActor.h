/**
 * @file AutoActor.h
 * @brief Header file for the AutoActor class, a smart pointer for Actor objects.
 *
 * @details This file defines the AutoActor class, which manages the automatic creation
 * and deletion of Actor derivatives. It provides methods for loading actors
 * from various sources and ensures proper resource management. The
 * implementation is in AutoActor.cpp.
 *
 * Significant dependencies:
 * - Actor: Base class for graphical elements in the engine.
 * - XNode: Used for parsing XML data when loading actors from nodes.
 * - ThemeManager: Handles loading of themed assets and metrics.
 * - ActorUtil: Provides utility functions for actor creation and manipulation.
 */

#ifndef AutoActor_H
#define AutoActor_H

class Actor;
class XNode;

/**
 * @brief A smart pointer for Actor.
 *
 * This creates the appropriate Actor derivative on load and
 * automatically deletes the Actor on deconstruction. */
class AutoActor
{
public:
	AutoActor(): m_pActor(nullptr) {}
	~AutoActor()			{ Unload(); }
	AutoActor( const AutoActor &cpy );
	AutoActor &operator =( const AutoActor &cpy );
	operator const Actor* () const	{ return m_pActor; }
	operator Actor* ()		{ return m_pActor; }
	const Actor *operator->() const { return m_pActor; }
	Actor *operator->()		{ return m_pActor; }
	void Unload();
	/** 
	 * @brief Determine if this actor is presently loaded.
	 * @return true if it is loaded, or false otherwise. */
	bool IsLoaded() const		{ return m_pActor != nullptr; }
	void Load( Actor *pActor );	// transfer pointer
	void Load( const RString &sPath );
	void LoadB( const RString &sMetricsGroup, const RString &sElement );	// load a background and set up LuaThreadVariables for recursive loading
	void LoadActorFromNode( const XNode *pNode, Actor *pParent );
	void LoadAndSetName( const RString &sScreenName, const RString &sActorName );

protected:
	/** @brief the Actor for which there is a smart pointer to. */
	Actor* m_pActor;
};

#endif

/**
 * @file
 * @author Chris Danford (c) 2003-2004
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
