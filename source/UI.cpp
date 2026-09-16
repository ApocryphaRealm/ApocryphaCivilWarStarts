#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Settings.h"
#include "Start.h"
#include "Starts.h"
#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <format>
#include <functional>
#include <string>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		constexpr const char* kLogLevelNames[]{ "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount{ 7 };

		// The framework draws from the renderer's present hook. Anything that touches the game rather than
		// this page's own variables is handed to the main thread first.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface()) { taskInterface->AddTask(std::move(a_task)); }
		}

		// A page that calls an export the installed framework does not have crashes on its first draw rather
		// than failing to register, so every export this page resolves is probed by its resolved name first.
		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextWrappedV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igSliderFloat",
				"igIsItemHovered",
				"igSetTooltip",
				// The hand-drawn switch (rule 32: a boolean is a switch, never a tick-box).
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};
			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("the menu framework does not export \"{}\"", name);
					return false;
				}
			}
			return true;
		}
	}

	void Register() noexcept
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("the Apocrypha Menu Framework is not installed; settings are read from the INI only");
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("the installed menu framework is older than this mod's settings page needs; "
						 "update it to configure {} in game", starts::kDisplayName);
			return;
		}

		SKSEMenuFramework::SetSection(starts::kDisplayName);
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);
		SKSEMenuFramework::AddSectionItem("Status", StatusPanel::Render);

		logger::info("settings page registered with the menu framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		ImGuiMCP::TextWrapped(
			"Two starts in Alternate Perspective's menu. \"Stormcloak Recruit\" begins in %s; "
			"\"Imperial Recruit\" begins in %s. Either way you wake in uniform with your side's recruitment "
			"questline already running. Everything below is read when the start runs, so a change here "
			"applies to the next new game, and it applies to both sides.",
			starts::kSides[0].arrivalDescription, starts::kSides[1].arrivalDescription);
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText("The start");

		ImGuiMCP::Toggle("Enabled", &settings::general::enabled);
		if (ImGuiMCP::IsItemHovered())
		{
			ImGuiMCP::SetTooltip("Off leaves both start options in Alternate Perspective's menu but makes choosing "
								 "one do nothing, so they can be switched off without uninstalling mid-playthrough.");
		}

		ImGuiMCP::Toggle("Move to the war room", &settings::start::moveToWarRoom);
		ImGuiMCP::Toggle("Give the uniform", &settings::start::giveStarterEquipment);
		ImGuiMCP::Toggle("Wear it straight away", &settings::start::equipStarterEquipment);
		ImGuiMCP::Toggle("Start the civil war questline", &settings::start::startCivilWarQuest);
		if (ImGuiMCP::IsItemHovered())
		{
			ImGuiMCP::SetTooltip("Starts the game's own recruitment quest at its own opening stage. Nothing is "
								 "skipped ahead - the oath and everything after it happen in game as they always do.");
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Timing");

		float delay = settings::general::startDelaySeconds;
		if (ImGuiMCP::SliderFloat("Wait before starting (seconds)", &delay, 0.0f, 10.0f, "%.2f"))
		{
			settings::general::startDelaySeconds = std::clamp(delay, 0.0f, 30.0f);
		}
		if (ImGuiMCP::IsItemHovered())
		{
			ImGuiMCP::SetTooltip("Alternate Perspective is still finishing its own hand-off when it starts this "
								 "mod's quest. Waiting lets it finish first; raise this if you ever arrive back in "
								 "the Resting Pilgrim, lower it if you ever land in Helgen.");
		}

		float gap = settings::general::stageGapSeconds;
		if (ImGuiMCP::SliderFloat("Beat between the steps (seconds)", &gap, 0.0f, 2.0f, "%.2f"))
		{
			settings::general::stageGapSeconds = std::clamp(gap, 0.0f, 5.0f);
		}
		if (ImGuiMCP::IsItemHovered())
		{
			ImGuiMCP::SetTooltip("The start is spread over several frames on purpose - arrive, then pack, then each "
								 "piece of the uniform, then the questline. It is also how often the mod checks "
								 "whether you have finished arriving: nothing is handed over until you are actually "
								 "standing in the war room, however long the load takes. 0 puts it all in one frame.");
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Logging");

		int logLevel{ static_cast<int>(settings::debug::logLevel) };
		if (ImGuiMCP::Combo("Log level", &logLevel, kLogLevelNames, kLogLevelCount))
		{
			settings::debug::logLevel = static_cast<std::uint32_t>(std::clamp(logLevel, 0, kLogLevelCount - 1));
			OnMainThread([]() { settings::ApplyLogLevel(); });
		}
		if (ImGuiMCP::IsItemHovered())
		{
			ImGuiMCP::SetTooltip("How much detail reaches the log. It ships at Trace, so a bug report arrives with "
								 "the log already complete.");
		}

		ImGuiMCP::Spacing();

		if (ImGuiMCP::Button("Save"))
		{
			OnMainThread([]() {
				statusMessage = settings::Save() ? "Settings saved." : "Could not save the INI. See the log for why.";
			});
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Reload from INI"))
		{
			OnMainThread([]() {
				statusMessage = settings::Reload() ? "Settings reloaded from the INI."
												   : "Could not read the INI. See the log for why.";
			});
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Restore defaults"))
		{
			OnMainThread([]() { settings::RestoreDefaults(); });
			statusMessage = "Defaults restored. Press Save to keep them.";
		}

		if (!statusMessage.empty()) { ImGuiMCP::TextWrapped("%s", statusMessage.c_str()); }

		ImGuiMCP::Spacing();
		ImGuiMCP::TextWrapped(".\\Data\\SKSE\\Plugins\\%s", starts::kIniName);
	}

	void __stdcall StatusPanel::Render()
	{
		// "Nothing happened" and "you did not pick one of these starts" look identical from the player's
		// side, so the page says which of the two it was rather than leaving them to guess.
		const auto& run = start::LastRun();

		ImGuiMCP::SeparatorText("This session");
		if (!start::WasChosen() && !run.ran)
		{
			ImGuiMCP::TextWrapped("Neither start has been chosen in this session. One runs once, when Alternate "
								  "Perspective starts its quest - which happens after you pick \"Stormcloak Recruit\" "
								  "or \"Imperial Recruit\" from the Messenger in the Resting Pilgrim.");
		}
		else if (!run.ran)
		{
			ImGuiMCP::TextWrapped("A start was chosen and is waiting out its delay of %.2f seconds.",
								  settings::general::startDelaySeconds);
		}
		else
		{
			ImGuiMCP::TextWrapped("%s ran (%s).", run.sideName.c_str(), run.reason.c_str());
			ImGuiMCP::Text("Moved to the war room: %s", run.moved ? "yes" : "no");
			ImGuiMCP::Text("Equipment given: %d (worn: %d)", run.itemsGiven, run.itemsEquipped);
			ImGuiMCP::Text("Civil war questline: %s", run.questStarted ? "running" : "not started");
			if (!run.problem.empty()) { ImGuiMCP::TextWrapped("Problem: %s", run.problem.c_str()); }
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Run one now");
		ImGuiMCP::TextWrapped("For testing, and for a game where the start was interrupted. This does exactly what "
							  "choosing the start does - it moves you, hands over the uniform and starts the "
							  "questline - so do not press it in a playthrough you care about.");
		for (std::size_t i = 0; i < starts::kSideCount; ++i)
		{
			const auto label = std::format("Run the {} start now", starts::kSides[i].name);
			if (ImGuiMCP::Button(label.c_str()))
			{
				const std::size_t side = i;
				OnMainThread([side]() { start::Run(side, "settings page"); });
				statusMessage = std::format("{} was run from the settings page.", starts::kSides[i].name);
			}
		}
	}
}
