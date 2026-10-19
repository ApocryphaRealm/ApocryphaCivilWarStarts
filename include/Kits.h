#pragma once

// Where each side's uniform comes from, read from ONE data file the player can edit.
//
// The owner, 2026-10-19: "Civil War Starts should just have compatibility for several different Civil War armor
// mods so that it can sense them and just call whatever armor from that mod."
//
// Data/SKSE/Plugins/CivilWarStarts_Kits.json lists, for each side, the armour sources in the order to try them.
// A source names the plugin file (or a few file names the same mod circulates under) and its pieces; a piece is a
// slot, a FormID and the name it must have. A source is used when one of its plugins is loaded and every required
// piece resolves under its expected name; otherwise the log says why and the next source is tried. The last source
// needs no plugin - it is the game's own gear - so a side never ends up with nothing.
//
// The table compiled into the DLL (starts::kSides in Starts.h) is only the fallback for a file that is missing or
// broken. A player reorders the sources, or adds a mod, by editing the one file.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RE
{
	class TESBoundObject;
}

namespace kits
{
	struct Piece
	{
		// The LOCAL FormID inside the source's plugin (the load-order index is resolved at runtime), or the whole
		// FormID of a record in the game's own files when the source names no plugin.
		std::uint32_t formID = 0;
		std::string slot;       // body, boots, gauntlets, helmet, shield, weapon, ... - for the log only
		std::string name;       // the name the record must carry (checked for sources that name a plugin)
		bool optional = false;  // a missing optional piece is left out; a missing required piece sinks the source
	};

	struct Source
	{
		std::string id;
		std::string label;                 // how the log and the player say it
		std::vector<std::string> plugins;  // accepted file names; empty = needs no other mod
		std::vector<Piece> pieces;
	};

	// Read the file. A missing or broken file is not an error for the player: the built-in table is used instead and the
	// log says which. Returns true when the file was used. Safe to call again (the DevBench "reload" op does).
	bool Load();

	// Which table is in use, in words: "file <path>" or "built-in table (<why>)".
	const std::string& Origin();

	const std::vector<Source>& SourcesFor(std::size_t a_side);

	// A piece found in the running game. It carries its own copy of the slot and name, so a later reload of the file
	// cannot leave it pointing into a table that has been replaced.
	struct Resolved
	{
		RE::TESBoundObject* object = nullptr;
		std::string slot;
		std::string name;
		std::uint32_t formID = 0;  // the file's FormID (local to the plugin for a plugin source)
	};

	struct Verdict
	{
		std::string id;
		std::string label;
		bool installed = false;
		bool usable = false;
		std::string why;  // "ok", or the reason this source was passed over
	};

	struct Choice
	{
		bool found = false;              // false only if even the game's own gear is missing
		std::string sourceId;
		std::string sourceLabel;
		std::string pluginFile;          // the accepted file name that was found, empty for the game's own gear
		std::vector<Resolved> pieces;    // required and optional pieces that resolved, in file order
		std::vector<Verdict> verdicts;   // every source tried, in order, up to and including the chosen one
	};

	// Walk the side's sources in order and return the first usable one. Game thread only. a_quiet = no log lines (the
	// DevBench "kits" op asks "what would be chosen" without writing to the log).
	Choice Choose(std::size_t a_side, bool a_quiet);

	// The DevBench "kits" op: the origin, and for each side what Choose would pick and why each earlier source lost.
	std::string DescribeJson();
}
