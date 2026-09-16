// Alternate Perspective - Civil War Starts. Own code, GPL-3.0-or-later (2026-09-16).
//
// An add-on for Alternate Perspective (Nexus 50307). It adds two starts to AP's menu in the Resting
// Pilgrim - Stormcloak Recruit and Imperial Recruit. Either puts the player in that side's war room
// (the Palace of the Kings in Windhelm, or Castle Dour in Solitude), in that side's uniform, with the
// game's own recruitment questline running.
//
// Three pieces make that work, and only one of them is code:
//   * ApocryphaCivilWarStarts.esp - two quest records, one per side. They exist so AP has something to
//     name and start; they hold no script and no aliases.
//   * SKSE\AlternatePerspective\ApocryphaCivilWarStarts.json - the registration AP reads. Its format is
//     AP 4.0's own schema, shipped at SKSE\AlternatePerspective\Schema\schema.json.
//   * This DLL - it watches for one of those quests starting and does the work.
//
// No vanilla record is edited and no game file is replaced, so removing the mod removes it entirely.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Settings.h"
#include "Start.h"
#include "Starts.h"
#include "UI.h"

#include "utils/Logger.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kPostPostLoad:
			// The menu framework is another SKSE plugin, so its exports are only reliably there once
			// every plugin has loaded.
			UI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			// Forms exist from here on, which is the earliest our own quests can be looked up.
			start::Install();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kNewGame:
		case SKSE::MessagingInterface::kPostLoadGame:
			// A game has begun, which is the only window in which one of these starts can be chosen.
			start::WatchForStart();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::log::init(starts::kLogName);

	settings::Init(starts::kIniName);
	settings::ApplyLogLevel();
	SKSE::log::describe_level(starts::kIniName);

	logger::info("{} {} loading", starts::kDisplayName,
				 SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	return true;
}
