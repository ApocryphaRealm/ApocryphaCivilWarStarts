#pragma once

// Alternate Perspective Civil War Starts.
//
// THE WHOLE MOD IS THIS TABLE PLUS THE CODE THAT READS IT. Both sides are one mod, one plugin and one
// DLL; a side is a row here, and adding another one would be a row and a quest record, nothing else.
//
// Every FormID below was read out of the game's own Skyrim.esm on 2026-09-16 rather than recalled.
// The arrival markers were confirmed to be persistent XMarkers standing on the floor of the cell named
// beside them - deliberately the FACTION HQ markers rather than the map-table floor markers, because
// the table marker sits UNDER the war map and the engine puts anyone moved there on top of the table
// (seen in the first live test: the player arrived standing on the map, 2026-09-16).

#include <cstddef>
#include <cstdint>

namespace starts
{
	inline constexpr const char* kLogName = "ApocryphaCivilWarStarts";
	inline constexpr const char* kDisplayName = "Alternate Perspective Civil War Starts";
	inline constexpr const char* kIniName = "ApocryphaCivilWarStarts.ini";
	inline constexpr const char* kPluginName = "Alternate Perspective Civil War Starts.esp";

	struct Item
	{
		std::uint32_t formID;
		const char* name;  // for the log only - the game supplies the name the player sees
		bool equip;
	};

	// The Stormcloak uniform: what the game's own ArmorStormcloakOutfit puts on a Stormcloak soldier.
	inline constexpr Item kStormcloakKit[] = {
		{ 0x000A6D7B, "Stormcloak Cuirass", true },
		{ 0x000A6D7F, "Stormcloak Boots", true },
		{ 0x000A6D7D, "Stormcloak Gauntlets", true },
		{ 0x000A6D79, "Stormcloak Helmet", true },
		{ 0x00012EB7, "Iron Sword", true },
	};

	// The legionary's kit: what CWSoldierImperialSoldierOutfitLight puts on an Imperial soldier.
	inline constexpr Item kImperialKit[] = {
		{ 0x00013ED9, "Imperial Light Armor", true },
		{ 0x00013ED7, "Imperial Light Boots", true },
		{ 0x00013EDA, "Imperial Light Bracers", true },
		{ 0x00013EDB, "Imperial Light Helmet", true },
		{ 0x00013AB2, "Imperial Light Shield", true },
		{ 0x000135B8, "Imperial Sword", true },
	};

	struct Side
	{
		const char* key;   // what the DevBench tool and the log call it
		const char* name;  // what the player sees in Alternate Perspective's menu

		// Our own quest, the one Alternate Perspective starts. Local FormID inside our plugin.
		std::uint32_t questLocalID;

		// Where the player wakes up.
		std::uint32_t arrivalMarker;
		const char* arrivalDescription;

		// The game's own recruitment quest, started at its own opening stage. The mod never skips ahead
		// to the swearing-in at stage 200, so no vanilla content is bypassed.
		std::uint32_t civilWarQuest;
		std::uint16_t civilWarStage;
		const char* civilWarDescription;

		const Item* kit;
		std::size_t kitCount;
	};

	inline constexpr Side kSides[] = {
		{
			"stormcloak", "Stormcloak Recruit",
			0x800,
			0x000422BA, "the war room of the Palace of the Kings, Windhelm",  // CWSonsFactionHQMarker
			0x000E2D29, 1, "CW01B (Joining the Stormcloaks)",
			kStormcloakKit, std::size(kStormcloakKit),
		},
		{
			"imperial", "Imperial Recruit",
			0x801,
			0x000422B8, "the war room of Castle Dour, Solitude",  // CWImperialFactionHQMarker
			0x000D517A, 1, "CW01A (Joining the Legion)",
			kImperialKit, std::size(kImperialKit),
		},
	};

	inline constexpr std::size_t kSideCount = std::size(kSides);
}
