#include "PCH.h"

#include "Start.h"

#include "Settings.h"
#include "Starts.h"
#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

namespace start
{
	namespace
	{
		std::mutex g_lock;
		Report g_report;
		std::atomic<bool> g_installed{ false };
		std::atomic<bool> g_chosen{ false };
		std::atomic<bool> g_queued{ false };
		// Where the move was aimed. Nothing is handed over until the player is actually THERE - the owner,
		// 2026-09-16: "we should only get the items after teleporting".
		RE::TESObjectCELL* g_destination = nullptr;

		// Our quests' real FormIDs, filled in at kDataLoaded. Index matches starts::kSides.
		std::array<RE::FormID, starts::kSideCount> g_questIDs{};

		std::string JsonText(const std::string& a_text)
		{
			std::string out;
			for (char c : a_text)
			{
				if (c == '"' || c == '\\') { out += '\\'; out += c; }
				else if (c == '\n') { out += "\\n"; }
				else { out += c; }
			}
			return out;
		}

		// One of our own quests, looked up by the plugin that owns it rather than by a baked FormID,
		// because the index depends on where the player's load order puts us.
		RE::TESQuest* OurQuest(std::size_t a_side)
		{
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler || a_side >= starts::kSideCount) { return nullptr; }
			return handler->LookupForm<RE::TESQuest>(starts::kSides[a_side].questLocalID, starts::kPluginName);
		}

		// Everything else is a vanilla record, so the FormID is the whole address.
		template <class T>
		T* Vanilla(RE::FormID a_id)
		{
			return RE::TESForm::LookupByID<T>(a_id);
		}

		// A kit item, resolved. A piece from another mod is looked up by PLUGIN and local FormID, never by
		// a baked full FormID - the plugin's index depends on where the player's load order puts it.
		RE::TESBoundObject* KitObject(const starts::Item& a_item)
		{
			if (!a_item.plugin) { return Vanilla<RE::TESBoundObject>(a_item.formID); }
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) { return nullptr; }

			// The UNTEMPLATED LookupForm, then As<>. TESDataHandler::LookupForm<T> tests
			// form->Is(T::FORMTYPE) - an EXACT type match - so asking it for a TESBoundObject, which is a
			// base class and not a form type anything actually is, returns null for every armour and weapon
			// in the game. It compiles, and it silently finds nothing: the first build using it reported all
			// six Sons of Skyrim pieces "not in the game" while the plugin was plainly loaded (2026-09-16).
			// TESForm::LookupByID<T> does NOT behave this way - it uses As<T>(), which walks the hierarchy -
			// which is why the vanilla half of the same kit worked and hid the fault.
			auto* form = handler->LookupForm(a_item.formID, a_item.plugin);
			return form ? form->As<RE::TESBoundObject>() : nullptr;
		}

		// Which kit this side actually gets. The preferred one needs another mod; without it the fallback
		// is used, so the mod never requires that mod and never hands out an empty set.
		void ChooseKit(const starts::Side& a_side, const starts::Item*& a_kit, std::size_t& a_count)
		{
			a_kit = a_side.fallbackKit;
			a_count = a_side.fallbackKitCount;
			if (!a_side.kitPlugin) { a_kit = a_side.kit; a_count = a_side.kitCount; return; }

			auto* handler = RE::TESDataHandler::GetSingleton();
			const bool present = handler && handler->LookupModByName(a_side.kitPlugin) != nullptr;
			if (present)
			{
				a_kit = a_side.kit;
				a_count = a_side.kitCount;
				logger::info("kit: {} is installed, so the {} start uses its gear", a_side.kitPluginName, a_side.key);
			}
			else
			{
				logger::info("kit: {} is not installed, so the {} start falls back to the game's own gear",
							 a_side.kitPluginName, a_side.key);
			}
		}

		bool MovePlayer(const starts::Side& a_side, RE::PlayerCharacter* a_player, std::string& a_problem)
		{
			auto* marker = Vanilla<RE::TESObjectREFR>(a_side.arrivalMarker);
			if (!marker)
			{
				a_problem = std::format("the arrival marker {:08X} is not in the game", a_side.arrivalMarker);
				logger::error("move: {}", a_problem);
				return false;
			}
			g_destination = marker->GetParentCell();
			a_player->MoveTo(marker);
			logger::info("move: the player was sent to {} (marker {:08X})", a_side.arrivalDescription, a_side.arrivalMarker);

			return true;
		}

		// Handing the set over and wearing it are two separate steps, and they do not share a frame - see
		// the staged sequence in RunStaged below for why. This one only puts the items in the pack.
		std::vector<RE::TESBoundObject*> GiveKit(const starts::Item* a_kit, std::size_t a_count,
												 RE::PlayerCharacter* a_player, int& a_given, std::string& a_problem)
		{
			std::vector<RE::TESBoundObject*> given;
			given.reserve(a_count);

			for (std::size_t i = 0; i < a_count; ++i)
			{
				const auto& item = a_kit[i];
				auto* object = KitObject(item);
				if (!object)
				{
					// One missing record is worth saying out loud but is not worth abandoning the rest of the
					// kit for - the player would rather arrive in three pieces of armour than none.
					logger::error("kit: {} ({:08X}) is not in the game; skipped", item.name, item.formID);
					if (a_problem.empty()) { a_problem = std::format("{} is missing from the game", item.name); }
					given.push_back(nullptr);
					continue;
				}
				a_player->AddObjectToContainer(object, nullptr, 1, nullptr);
				++a_given;
				given.push_back(object);
				logger::debug("kit: gave {} ({:08X})", item.name, item.formID);
			}

			return given;
		}

		// One piece, forced on. Forced because an ordinary equip is a request the actor can decline, and in
		// the first live test that is exactly what happened - cuirass, boots and gauntlets went on and the
		// helmet quietly did not (2026-09-16).
		bool WearOne(RE::PlayerCharacter* a_player, RE::TESBoundObject* a_object, const char* a_name)
		{
			auto* equipManager = RE::ActorEquipManager::GetSingleton();
			if (!equipManager || !a_object) { return false; }
			equipManager->EquipObject(a_player, a_object, nullptr, 1, nullptr,
									  /*queueEquip*/ true, /*forceEquip*/ true, /*playSounds*/ false,
									  /*applyNow*/ false);
			logger::debug("kit: equipped {}", a_name);
			return true;
		}

		// SetStage goes through the Papyrus virtual machine rather than the quest's currentStage member,
		// because a stage is not a number - setting it is what runs that stage's fragment, fills its
		// aliases and puts the objective in the journal. Writing the member would move the number and do
		// none of that.
		bool SetStage(RE::TESQuest* a_quest, std::uint16_t a_stage)
		{
			auto* skyrimVM = RE::SkyrimVM::GetSingleton();
			auto* vm = skyrimVM ? skyrimVM->impl.get() : nullptr;
			if (!vm) { return false; }
			auto* policy = vm->GetObjectHandlePolicy();
			if (!policy) { return false; }
			const RE::VMHandle handle = policy->GetHandleForObject(RE::FormType::Quest, a_quest);
			if (handle == 0) { return false; }

			auto args = RE::MakeFunctionArguments(static_cast<std::uint32_t>(a_stage));
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			return vm->DispatchMethodCall(handle, RE::BSFixedString("Quest"), RE::BSFixedString("SetStage"), args, callback);
		}

		bool StartQuestline(const starts::Side& a_side, std::string& a_problem)
		{
			auto* quest = Vanilla<RE::TESQuest>(a_side.civilWarQuest);
			if (!quest)
			{
				a_problem = std::format("{} ({:08X}) is not in the game", a_side.civilWarDescription, a_side.civilWarQuest);
				logger::error("questline: {}", a_problem);
				return false;
			}
			if (quest->IsCompleted())
			{
				// A save loaded on top of a finished war is not a new start; leave it alone rather than
				// re-running a quest the game considers done.
				logger::warn("questline: {} is already completed; left alone", a_side.civilWarDescription);
				a_problem = "the civil war recruitment quest is already completed";
				return false;
			}
			// IsEnabled(), not IsRunning(). TESQuest::IsRunning() is "not stopping and not mid-promotion",
			// which is TRUE for a quest that has never run at all - reading it as "already going" would skip
			// the Start() that actually gets the quest going. IsEnabled() is the flag the engine sets when a
			// quest is genuinely running.
			if (!quest->IsEnabled() && !quest->Start())
			{
				a_problem = std::format("{} refused to start", a_side.civilWarDescription);
				logger::error("questline: {}", a_problem);
				return false;
			}
			const bool staged = SetStage(quest, a_side.civilWarStage);
			logger::info("questline: {} running, stage {} {}", a_side.civilWarDescription, a_side.civilWarStage,
						 staged ? "set" : "NOT set (the script call was refused)");
			if (!staged && a_problem.empty()) { a_problem = "the civil war quest started but its opening stage was not set"; }
			return true;
		}

		void RunNow(std::size_t a_side, std::string a_reason)
		{
			g_queued.store(false);
			Run(a_side, a_reason.c_str());
		}

		// The wait is a real wait, not a frame count: AP is mid-hand-off when it starts our quest, and the
		// work it is still doing (the fade, freezing the player, watching whether the start moves them out
		// of its own cell) would otherwise land on top of ours. The sleep happens on its own thread and the
		// work itself is handed back to the game thread, because moving the player from anywhere else is
		// not safe.
		void QueueRun(std::size_t a_side, const char* a_reason)
		{
			if (g_queued.exchange(true)) { return; }
			const std::string reason = a_reason;
			const float delay = settings::general::startDelaySeconds;
			std::thread([a_side, reason, delay]() {
				if (delay > 0.0f)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000.0f)));
				}
				if (auto* task = SKSE::GetTaskInterface())
				{
					task->AddTask([a_side, reason]() { RunNow(a_side, reason); });
				}
				else
				{
					RunNow(a_side, reason);
				}
			}).detach();
		}

		// THERE IS NO WATCHDOG, AND THAT IS DELIBERATE.
		//
		// One used to live here: it polled our quests every quarter second after a game began and ran the
		// start if one looked enabled, in case the quest-start event never reached us. It cost more than it
		// was worth. Alternate Perspective's hand-off is a Start() call made when the player walks through
		// the door, and the event for it has arrived on every clean run - while the watchdog, which only
		// ever asks "does this quest look started", fired 13ms AHEAD of the event once and then fired at
		// SELECTION time: the owner picked a side and was teleported on the spot instead of walking through
		// the door (2026-09-16). A guard that can act at the wrong moment is worse than no guard, when the
		// thing it guards has never actually failed.
		//
		// If the event ever does go missing the symptom is clear - the start does nothing, the log says
		// nothing after "listening", and the Status page says the start was never chosen. That is a
		// diagnosable silence, not a teleport nobody asked for.

		void QueueStep(std::size_t a_side, std::string a_reason, std::size_t a_step);

		// Filled by the first step, read by the ones after it. Only ever touched on the main thread.
		std::vector<RE::TESBoundObject*> g_given;
		const starts::Item* g_kit = nullptr;
		std::size_t g_kitCount = 0;

		// How many times the arrival step looks before giving up and carrying on anyway.
		inline constexpr int kArrivalLooks = 40;
		int g_arrivalLooks = 0;

		// The order, and it is deliberate (the owner, 2026-09-16: "we should only get the items after
		// teleporting" and "the quest should happen and then the equipment"):
		//   0        wait until the player has actually arrived
		//   1        start the civil war questline
		//   2        fill the pack
		//   3..n     wear one piece per frame
		//   last     stop our own quests and say what happened
		void RunStep(std::size_t a_side, std::string a_reason, std::size_t a_step)
		{
			const auto& side = starts::kSides[a_side];
			auto* player = RE::PlayerCharacter::GetSingleton();
			const std::size_t wearFirst = 3;
			const std::size_t wearLast = wearFirst + std::max(side.kitCount, side.fallbackKitCount);  // one past the last wear step

			// Step 0: are they there yet? Checked, not timed - a teleport takes as long as it takes, and on
			// a slow load a timer hands somebody a uniform while they are still watching a loading screen.
			if (a_step == 0)
			{
				const auto* here = player ? player->GetParentCell() : nullptr;
				const bool arrived = !g_destination || (here && here == g_destination);
				if (!arrived && ++g_arrivalLooks < kArrivalLooks)
				{
					QueueStep(a_side, a_reason, 0);
					return;
				}
				logger::info("move: the player is now in cell {} (expected: {}){}",
							 here ? here->GetFormEditorID() : "<unknown>", side.arrivalDescription,
							 arrived ? "" : " - gave up waiting and carried on");
				QueueStep(a_side, a_reason, 1);
				return;
			}

			// Step 1: the questline, before anything is handed over.
			if (a_step == 1)
			{
				{
					std::scoped_lock l(g_lock);
					if (settings::start::startCivilWarQuest) { g_report.questStarted = StartQuestline(side, g_report.problem); }
					else { logger::info("questline: skipped (bStartCivilWarQuest=0)"); }
				}
				QueueStep(a_side, a_reason, 2);
				return;
			}

			// Step 2: the pack.
			if (a_step == 2)
			{
				g_given.clear();
				g_kit = nullptr;
				g_kitCount = 0;
				if (player && settings::start::giveStarterEquipment)
				{
					ChooseKit(side, g_kit, g_kitCount);
					std::scoped_lock l(g_lock);
					g_given = GiveKit(g_kit, g_kitCount, player, g_report.itemsGiven, g_report.problem);
				}
				else
				{
					logger::info("kit: skipped (bGiveStarterEquipment=0)");
				}
				QueueStep(a_side, a_reason, settings::start::equipStarterEquipment ? wearFirst : wearLast);
				return;
			}

			if (a_step >= wearFirst && a_step < wearLast)
			{
				const std::size_t i = a_step - wearFirst;
				if (player && g_kit && i < g_kitCount && i < g_given.size() && g_given[i] && g_kit[i].equip &&
					WearOne(player, g_given[i], g_kit[i].name))
				{
					std::scoped_lock l(g_lock);
					++g_report.itemsEquipped;
				}
				QueueStep(a_side, a_reason, a_step + 1);
				return;
			}

			// The last step: tidy up and say what actually happened.
			//
			// Our own quests have nothing left to do once one has run, and a quest left running is a quest
			// that shows up in save inspections forever. Both are stopped, not just the one that ran: the
			// other was never started, and Stop() on a stopped quest is harmless.
			for (std::size_t i = 0; i < starts::kSideCount; ++i)
			{
				if (auto* ours = OurQuest(i); ours && ours->IsEnabled())
				{
					ours->Stop();
					logger::debug("start: our own {} quest stopped", starts::kSides[i].key);
				}
			}

			std::scoped_lock l(g_lock);
			logger::info("start ({}, {}): finished - moved={} given={} worn={} questline={}{}",
						 side.key, a_reason, g_report.moved, g_report.itemsGiven, g_report.itemsEquipped,
						 g_report.questStarted,
						 g_report.problem.empty() ? "" : std::format(" (problem: {})", g_report.problem));
		}

		void QueueStep(std::size_t a_side, std::string a_reason, std::size_t a_step)
		{
			const float gap = std::clamp(settings::general::stageGapSeconds, 0.0f, 5.0f);
			std::thread([a_side, a_reason, a_step, gap]() {
				if (gap > 0.0f)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(gap * 1000.0f)));
				}
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddTask([a_side, a_reason, a_step]() { RunStep(a_side, a_reason, a_step); });
				}
			}).detach();
		}

		void StageTheRest(std::size_t a_side, std::string a_reason)
		{
			g_arrivalLooks = 0;
			QueueStep(a_side, std::move(a_reason), 0);
		}

		class QuestSink : public RE::BSTEventSink<RE::TESQuestStartStopEvent>
		{
		public:
			static QuestSink* GetSingleton()
			{
				static QuestSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStartStopEvent* a_event,
												  RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override
			{
				if (!a_event || !a_event->started) { return RE::BSEventNotifyControl::kContinue; }
				for (std::size_t side = 0; side < starts::kSideCount; ++side)
				{
					if (g_questIDs[side] == 0 || a_event->formID != g_questIDs[side]) { continue; }
					g_chosen.store(true);
					logger::info("chosen: Alternate Perspective started the {} quest {:08X}; it runs in {:.2f}s",
								 starts::kSides[side].key, a_event->formID, settings::general::startDelaySeconds);
					QueueRun(side, "alternate perspective");
					break;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (g_installed.exchange(true)) { return; }

		int found = 0;
		for (std::size_t side = 0; side < starts::kSideCount; ++side)
		{
			auto* quest = OurQuest(side);
			if (!quest)
			{
				// Said plainly, because this is the one failure a player can actually fix: the ESP is what
				// AP names in its menu, and without it the start options cannot do anything.
				logger::error("install: {} has no quest {:03X} for the {} start. The plugin ships with the mod - "
							  "check that it is installed and enabled.",
							  starts::kPluginName, starts::kSides[side].questLocalID, starts::kSides[side].key);
				continue;
			}
			g_questIDs[side] = quest->GetFormID();
			++found;
			logger::info("install: the {} start is quest {:08X} ({})", starts::kSides[side].key,
						 g_questIDs[side], quest->GetFormEditorID());
		}
		if (found == 0) { return; }

		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder)
		{
			logger::error("install: the event source holder is not available; a start cannot be noticed");
			return;
		}
		holder->AddEventSink<RE::TESQuestStartStopEvent>(QuestSink::GetSingleton());
		logger::info("install: listening for {} of {} start quest(s)", found, starts::kSideCount);
	}

	void WatchForStart()
	{
		// Nothing to do: the quest-start event is the only trigger. See the note where the watchdog used
		// to be. Kept as a call site so main.cpp still marks where a game begins.
		logger::debug("a game has begun; waiting for Alternate Perspective to start one of our quests");
	}

	std::size_t SideFromKey(const std::string& a_key)
	{
		for (std::size_t side = 0; side < starts::kSideCount; ++side)
		{
			if (a_key == starts::kSides[side].key || a_key == starts::kSides[side].name) { return side; }
		}
		return starts::kSideCount;
	}

	bool Run(std::size_t a_side, const char* a_reason)
	{
		std::scoped_lock l(g_lock);

		Report report;
		report.reason = a_reason ? a_reason : "";
		report.side = a_side;

		if (a_side >= starts::kSideCount)
		{
			report.problem = "no such start";
			logger::error("start ({}): {}", report.reason, report.problem);
			g_report = report;
			return false;
		}
		const auto& side = starts::kSides[a_side];
		report.sideName = side.name;

		if (!settings::general::enabled)
		{
			report.problem = "the mod is switched off in its settings (bEnabled=0)";
			logger::warn("start ({}, {}): {}", side.key, report.reason, report.problem);
			g_report = report;
			return false;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player)
		{
			report.problem = "there is no player to move";
			logger::error("start ({}, {}): {}", side.key, report.reason, report.problem);
			g_report = report;
			return false;
		}

		logger::info("start ({}, {}): beginning", side.key, report.reason);

		if (settings::start::moveToWarRoom) { report.moved = MovePlayer(side, player, report.problem); }
		else { logger::info("move: skipped (bMoveToWarRoom=0)"); }

		// The rest is spread over the next second or so rather than crammed into this frame. See RunStaged.
		StageTheRest(a_side, report.reason);

		report.ran = true;
		logger::info("start ({}, {}): moved={}; the questline, the pack and the uniform follow once you are there",
					 side.key, report.reason, report.moved);
		g_report = report;
		return true;
	}

	const Report& LastRun()
	{
		return g_report;
	}

	bool WasChosen()
	{
		return g_chosen.load();
	}

	std::string StateJson()
	{
		std::scoped_lock l(g_lock);

		std::string sides;
		for (std::size_t i = 0; i < starts::kSideCount; ++i)
		{
			const auto& s = starts::kSides[i];
			auto* ours = OurQuest(i);
			auto* war = Vanilla<RE::TESQuest>(s.civilWarQuest);
			sides += std::format(
				"{}{{\"key\":\"{}\",\"name\":\"{}\",\"where\":\"{}\",\"marker\":\"{:08X}\","
				"\"ourQuest\":{{\"formID\":\"{:08X}\",\"enabled\":{},\"stopped\":{}}},"
				"\"warQuest\":{{\"name\":\"{}\",\"formID\":\"{:08X}\",\"stage\":{},\"enabled\":{},\"completed\":{},\"currentStage\":{}}}}}",
				i ? "," : "", s.key, JsonText(s.name), JsonText(s.arrivalDescription), s.arrivalMarker,
				g_questIDs[i], ours && ours->IsEnabled() ? "true" : "false", ours && ours->IsStopped() ? "true" : "false",
				JsonText(s.civilWarDescription), s.civilWarQuest, s.civilWarStage,
				war && war->IsEnabled() ? "true" : "false", war && war->IsCompleted() ? "true" : "false",
				war ? war->GetCurrentStageID() : 0);
		}

		return std::format(
			"\"chosen\":{},\"lastRun\":{{\"ran\":{},\"side\":\"{}\",\"reason\":\"{}\",\"moved\":{},"
			"\"itemsGiven\":{},\"itemsEquipped\":{},\"questStarted\":{},\"problem\":\"{}\"}},\"sides\":[{}]",
			g_chosen.load() ? "true" : "false", g_report.ran ? "true" : "false",
			g_report.ran ? starts::kSides[g_report.side].key : "", JsonText(g_report.reason),
			g_report.moved ? "true" : "false", g_report.itemsGiven, g_report.itemsEquipped,
			g_report.questStarted ? "true" : "false", JsonText(g_report.problem), sides);
	}
}
