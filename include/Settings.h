#pragma once

// Settings for Alternate Perspective Civil War Starts. Plain-file INI beside the DLL (the project
// standard: redirector-proof, and readable by a player without a mod manager).
//
// Both sides share these settings. Nothing here is per-side: a player who wants the uniform for one
// side and not the other is not a case worth a second set of switches, and the start only ever runs
// once per game anyway.
//
// Every value here is also a control on the mod's Apocrypha Menu Framework page, and every compiled
// default below is the value the shipped INI carries (rule 16) - pre-finalize-check compares them.

#include <cstdint>
#include <string>

namespace settings
{
	namespace debug
	{
		inline std::uint32_t logLevel = 0;  // uLogLevel:Debug - 0 is trace, the shipped default
	}

	namespace general
	{
		// bEnabled:General - with this off the start options still appear in Alternate Perspective's
		// menu (the cards are AP's, read from our JSON before the game runs) but choosing one does
		// nothing, leaving the player in the Resting Pilgrim. It exists so the starts can be switched
		// off without uninstalling mid-playthrough.
		inline bool enabled = true;

		// fStartDelaySeconds:General - how long to wait after Alternate Perspective starts our quest
		// before acting. AP is still finishing its own hand-off in that moment - it fades the screen,
		// freezes the player and then watches to see whether the chosen start moves them out of the
		// start cell. Waiting a moment lets the fade cover the arrival; waiting too long trips AP's own
		// safety net, which says "You seem to be stuck. I'll move you into the Helgen Inn." and puts the
		// player in Helgen instead. One second sits comfortably between the two.
		inline float startDelaySeconds = 1.0f;

		// fStageGapSeconds:General - the beat between the parts of the start: the move, then filling the
		// pack, then each piece of the uniform going on, then the questline. They are deliberately NOT done
		// in one frame. A cross-cell teleport and a full set of armour appearing are both heavy pieces of
		// 3D work, and together in a single frame they make every mod that rebuilds itself from the player's
		// body do so at once - which is when Faster HDT-SMP crashed on the first end-to-end run (2026-09-16).
		// Raising this makes the start gentler and slower; 0 puts everything back in one frame.
		inline float stageGapSeconds = 0.40f;
	}

	namespace start
	{
		inline bool moveToWarRoom = true;         // bMoveToWarRoom:Start
		inline bool giveStarterEquipment = true;  // bGiveStarterEquipment:Start
		inline bool equipStarterEquipment = true; // bEquipStarterEquipment:Start
		inline bool startCivilWarQuest = true;    // bStartCivilWarQuest:Start
	}

	void Init(const std::string& a_iniFileName);
	bool Reload();
	bool Save();
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();
}
