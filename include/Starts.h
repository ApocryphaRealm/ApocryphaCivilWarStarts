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
	inline constexpr const char* kPluginName = "Alternate Perspective Civil War Starts.esl";

	struct Item
	{
		// When plugin is null this is a whole FormID in Skyrim.esm. When plugin is set it is the LOCAL
		// FormID inside that plugin - the index depends on the player's load order and is resolved at
		// runtime, never baked in here.
		std::uint32_t formID;
		const char* plugin;
		const char* name;  // for the log only - the game supplies the name the player sees
		bool equip;
	};

	// ---- Stormcloak -------------------------------------------------------------------------------
	// The owner, 2026-09-16: "i want to spawn with heavy armor and an axe and shield, all of which you
	// can get from sons of skyrim mod". Sons of Skyrim (Nexus 68656) gives each hold its own kit, so a
	// recruit standing in Windhelm's war room gets Windhelm's: heavy armour, the Windhelm helmet and
	// shield, heavy lamellar boots and gauntlets, and a Nord war axe. Every piece below was confirmed
	// HEAVY out of the plugin itself, with its armour rating, rather than assumed from the name.
	inline constexpr Item kStormcloakKitSonsOfSkyrim[] = {
		{ 0x004A9E, "NW_Sons_of_Skyrim.esp", "Windhelm Heavy Armor", true },
		{ 0x004AC8, "NW_Sons_of_Skyrim.esp", "Lamellar Heavy Boots", true },
		{ 0x004ACE, "NW_Sons_of_Skyrim.esp", "Lamellar Heavy Gauntlets", true },
		{ 0x006117, "NW_Sons_of_Skyrim.esp", "Windhelm Helmet", true },
		{ 0x006122, "NW_Sons_of_Skyrim.esp", "Windhelm Shield", true },
		{ 0x006158, "NW_Sons_of_Skyrim.esp", "Nord Heavy War Axe", true },
	};

	// Without Sons of Skyrim there is no heavy Stormcloak armour in the game, so the fallback is the
	// vanilla Stormcloak uniform - but still with an axe and a shield, because that is the loadout that
	// was asked for and vanilla does have both.
	inline constexpr Item kStormcloakKitVanilla[] = {
		{ 0x000A6D7B, nullptr, "Stormcloak Cuirass", true },
		{ 0x000A6D7F, nullptr, "Stormcloak Boots", true },
		{ 0x000A6D7D, nullptr, "Stormcloak Gauntlets", true },
		{ 0x000A6D79, nullptr, "Stormcloak Helmet", true },
		{ 0x00012EB6, nullptr, "Iron Shield", true },
		{ 0x00013790, nullptr, "Iron War Axe", true },
	};

	// ---- Imperial ---------------------------------------------------------------------------------
	// The Legion's heavy set, so the two sides start on equal footing, with the shield and sword the
	// game's own legionaries carry. Sons of Skyrim is a Stormcloak-and-hold-guard mod and has no Legion
	// kit, so there is nothing to prefer over this.
	inline constexpr Item kImperialKit[] = {
		{ 0x000136D5, nullptr, "Imperial Armor", true },
		{ 0x000136D6, nullptr, "Imperial Boots", true },
		{ 0x000136D4, nullptr, "Imperial Gauntlets", true },
		{ 0x00013EDC, nullptr, "Imperial Helmet", true },
		{ 0x000135BA, nullptr, "Imperial Shield", true },
		{ 0x000135B8, nullptr, "Imperial Sword", true },
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

		// The kit this side would rather have, and the plugin it needs. When that plugin is not loaded
		// the fallback is used instead - so the mod never requires it and never hands out nothing.
		const Item* kit;
		std::size_t kitCount;
		const char* kitPlugin;         // null when the preferred kit needs nothing
		const char* kitPluginName;     // how to say it to a player
		const Item* fallbackKit;
		std::size_t fallbackKitCount;
	};

	inline constexpr Side kSides[] = {
		{
			"stormcloak", "Stormcloak Recruit",
			0x800,
			0x000422BA, "the war room of the Palace of the Kings, Windhelm",  // CWSonsFactionHQMarker
			0x000E2D29, 1, "CW01B (Joining the Stormcloaks)",
			kStormcloakKitSonsOfSkyrim, std::size(kStormcloakKitSonsOfSkyrim),
			"NW_Sons_of_Skyrim.esp", "Sons of Skyrim",
			kStormcloakKitVanilla, std::size(kStormcloakKitVanilla),
		},
		{
			"imperial", "Imperial Recruit",
			0x801,
			0x000422B8, "the war room of Castle Dour, Solitude",  // CWImperialFactionHQMarker
			0x000D517A, 1, "CW01A (Joining the Legion)",
			kImperialKit, std::size(kImperialKit),
			nullptr, nullptr,
			kImperialKit, std::size(kImperialKit),
		},
	};

	inline constexpr std::size_t kSideCount = std::size(kSides);
}
