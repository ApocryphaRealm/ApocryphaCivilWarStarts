#pragma once

// Alternate Perspective Civil War Starts.
//
// THE WHOLE MOD IS THIS TABLE PLUS THE CODE THAT READS IT. Both sides are one mod, one plugin and one
// DLL; a side is a row here, and adding another one would be a row and a quest record, nothing else.
//
// THE UNIFORM LISTS BELOW ARE ONLY THE FALLBACK (1.0.7). The lists the mod actually uses are read from
// Data/SKSE/Plugins/CivilWarStarts_Kits.json (Kits.h): that file is where a supported armour mod is added, removed or
// reordered, with no rebuild. This compiled copy is what the start uses when that file is missing or broken, so it
// carries the sources the mod has always had (Civil War Guard Armor Replacer, Sons of Skyrim, the game's own gear).
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
	inline constexpr const char* kLogName = "CivilWarStarts";
	inline constexpr const char* kDisplayName = "Alternate Perspective Civil War Starts";
	inline constexpr const char* kIniName = "CivilWarStarts.ini";
	inline constexpr const char* kPluginName = "Alternate Perspective Civil War Starts.esp";

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

	// Civil War Guard Armor Replacer (the author's own all-in-one) merges Sons of Skyrim, New Legion, Guards Armor
	// Replacer and Imperial Tribune Armor into ONE light plugin, so the Windhelm kit above exists there under
	// DIFFERENT FormIDs and the Sons of Skyrim check cannot see it. These are the same six pieces, matched to the
	// Sons of Skyrim kit by EditorID (0_Windhelm_Armor_Heavy, 0_Boots_Lamellar_Heavy, 0_Gauntlets_Lamellar_Heavy,
	// 0_Windhelm_Helmet, 0_Windhelm_Shield_1, 0_WarAxe_Nord_Heavy) and read out of that plugin on 2026-10-19 - local
	// FormIDs inside a light plugin, resolved by plugin name at runtime. The replacer is not published yet, so every
	// piece is checked by NAME when the start runs (KitSource::verifyNames): a piece that is missing or called something
	// else means the plugin has moved on, and the start falls through to the next source instead of giving wrong gear.
	// Checked BEFORE Sons of Skyrim because the merge supersedes it.
	inline constexpr Item kStormcloakKitReplacer[] = {
		{ 0x000804, "Civil War Guard Armor Replacer.esp", "Windhelm Heavy Armor", true },
		{ 0x000819, "Civil War Guard Armor Replacer.esp", "Lamellar Heavy Boots", true },
		{ 0x00081C, "Civil War Guard Armor Replacer.esp", "Lamellar Heavy Gauntlets", true },
		{ 0x00085A, "Civil War Guard Armor Replacer.esp", "Windhelm Helmet", true },
		{ 0x00085E, "Civil War Guard Armor Replacer.esp", "Windhelm Shield", true },
		{ 0x000898, "Civil War Guard Armor Replacer.esp", "Nord Heavy War Axe", true },
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

	// One place a side's kit can come from. The first source whose plugin is loaded (and whose pieces all
	// resolve under their expected names) is the kit; a source with no plugin is always available,
	// so the last source is the game's own gear and the mod never hands out nothing and never requires another mod.
	struct KitSource
	{
		const char* plugin;      // null when the kit needs nothing
		const char* pluginName;  // how to say it to a player
		const Item* items;
		std::size_t count;
		bool verifyNames;        // no longer read: every source that names a plugin is name-checked (Kits.cpp)
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

		// The kit sources in order of preference; the last one needs no other mod.
		const KitSource* sources;
		std::size_t sourceCount;
	};

	inline constexpr KitSource kStormcloakSources[] = {
		{ "Civil War Guard Armor Replacer.esp", "Civil War Guard Armor Replacer", kStormcloakKitReplacer, std::size(kStormcloakKitReplacer), true },
		{ "NW_Sons_of_Skyrim.esp", "Sons of Skyrim", kStormcloakKitSonsOfSkyrim, std::size(kStormcloakKitSonsOfSkyrim), false },
		{ nullptr, nullptr, kStormcloakKitVanilla, std::size(kStormcloakKitVanilla), false },
	};

	// The Imperial pieces are vanilla records. Civil War Guard Armor Replacer overrides every one of them with New
	// Legion's Legion art (checked 2026-10-19: ArmorImperialCuirass, Boots, Gauntlets, Helmet, Shield and
	// Imperialsword all carry its armatures), so a player with the replacer already wears its Legion set from this
	// same list - no separate source is needed.
	inline constexpr KitSource kImperialSources[] = {
		{ nullptr, nullptr, kImperialKit, std::size(kImperialKit), false },
	};

	inline constexpr Side kSides[] = {
		{
			"stormcloak", "Stormcloak Recruit",
			0x800,
			0x000422BA, "the war room of the Palace of the Kings, Windhelm",  // CWSonsFactionHQMarker
			0x000E2D29, 1, "CW01B (Joining the Stormcloaks)",
			kStormcloakSources, std::size(kStormcloakSources),
		},
		{
			"imperial", "Imperial Recruit",
			0x801,
			0x000422B8, "the war room of Castle Dour, Solitude",  // CWImperialFactionHQMarker
			0x000D517A, 1, "CW01A (Joining the Legion)",
			kImperialSources, std::size(kImperialSources),
		},
	};

	inline constexpr std::size_t kSideCount = std::size(kSides);
}
