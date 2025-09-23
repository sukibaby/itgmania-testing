
/**
 * @file EditModePlayerManager.h
 * @brief Header for managing players during edit mode playback.
 *
 * This header declares the EditModePlayerManager class, which handles
 * player state, input, and rendering during edit mode (practice mode).
 * It manages multiple players, their note data, and gameplay interactions.
 *
 * The corresponding implementation is in EditModePlayerManager.cpp.
 *
 * Key dependencies:
 * - ActorFrame.h: Base class for UI actor frames, used for adding players.
 * - GameplayAssist.h: Provides assist features like ticks during playback.
 * - Player.h: Defines the Player class for individual player management.
 * - NoteData.h: Handles note data structures for gameplay.
 */

#include "global.h"

#include "ActorFrame.h"
#include "GameplayAssist.h"
#include "Player.h"
#include "NoteData.h"

#include <iterator>
#include <map>
#include <unordered_map>
#include <vector>

// This class is intended to manage player state during edit mode playback,
// aka practice mode.
class EditModePlayerManager
{
public:
	// Adds players based on the current gamestate.
	void AddPlayers(const NoteData& note_data);

	// Adds the players (and their notefields) to the desired actor frame.
	void AddPlayersToActorFrame(ActorFrame& frame);

	// Reload the note data for each player. Intended to be called
	// just before playback begins.
	void ReloadNoteData(const NoteData& note_data);

	// Toggles visiblity of the player(s) notefield.
	void SetVisible(bool visible);

	// During playback, if a player hits a button, handle the input. Only looks
	// at inputs related to gameplay, not menuing.
	bool HandleGameplayInput(const InputEventPlus& input, const GameButtonType& gbt);

	// If autoplay is enabled, force the player's state into autoplay mode.
	void SetupAutoplay();

	// Player::CacheAllUsedNoteSkins on each player.
	void CacheAllUsedNoteSkins();

	// Play assist ticks.
	void PlayTicks(GameplayAssist& gameplay_assist);

	// Sets the "center" boolean, for centering the notefield.
	void SetCenter(bool center) { center_ = center;  }

private:
	// All players that the manager is looking at. Indexable by PlayerNumber.
	std::unordered_map<PlayerNumber, std::shared_ptr<PlayerPlus>> players_;
	bool center_;
};
