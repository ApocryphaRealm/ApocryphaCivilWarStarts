#pragma once

// The start sequence itself.
//
// Alternate Perspective owns the menu and the choosing; this mod owns exactly one moment - the one
// where AP starts one of the quests our JSON named. From there the mod puts the player in that side's
// war room, hands over that side's uniform, and sets that side's recruitment questline running.
// Nothing else in the game is touched.

#include <cstddef>
#include <string>

namespace start
{
	// Resolve our quests and start listening for one of them starting. Called once at kDataLoaded;
	// safe to call again (it will not register twice).
	void Install();

	// Called when a game begins (new or loaded). Starts the short watchdog described in Start.cpp,
	// which notices one of our quests running even if the quest-start event never reaches us.
	void WatchForStart();

	// Run one side's sequence now. a_side indexes starts::kSides. Called by the quest-start listener
	// after the configured delay, and by the DevBench tool and the settings page so the whole thing can
	// be exercised without going through the menu. Returns false when it could not run at all (no
	// player, unknown side, disabled in the settings); a step that is switched off is not a failure.
	bool Run(std::size_t a_side, const char* a_reason);

	// The side whose key or display name this is, or starts::kSideCount when there is no such side.
	std::size_t SideFromKey(const std::string& a_key);

	// What the last run did, for the settings page and the DevBench tool.
	struct Report
	{
		bool ran = false;
		std::size_t side = 0;
		std::string sideName;
		std::string reason;
		bool moved = false;
		int itemsGiven = 0;
		int itemsEquipped = 0;
		bool questStarted = false;
		std::string problem;  // empty when everything asked for actually happened
	};

	const Report& LastRun();

	// True once one of our quests has been seen to start in this session - i.e. the player really did
	// pick one of these starts. The settings page says so, because "nothing happened" and "you did not
	// choose this start" look identical from the player's side otherwise.
	bool WasChosen();

	std::string StateJson();
}
