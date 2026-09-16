#include "PCH.h"

#include "Start.h"

#include "Settings.h"
#include "Starts.h"
#include "utils/Logger.h"

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

		bool MovePlayer(const starts::Side& a_side, RE::PlayerCharacter* a_player, std::string& a_problem)
		{
			auto* marker = Vanilla<RE::TESObjectREFR>(a_side.arrivalMarker);
			if (!marker)
			{
				a_problem = std::format("the arrival marker {:08X} is not in the game", a_side.arrivalMarker);
				logger::error("move: {}", a_problem);
				return false;
			}
			a_player->MoveTo(marker);
			logger::info("move: the player was sent to {} (marker {:08X})", a_side.arrivalDescription, a_side.arrivalMarker);

			// Where they ACTUALLY ended up. "We asked the engine to move them" and "they are there" are
			// different claims, and only the second is worth having in a bug report.
			//
			// The check waits half a second on its own thread before asking. Asking in this frame answers
			// with the cell they are LEAVING, and so does a task queued from here - a task added while the
			// task queue is being drained runs in the same drain, not the next frame. Both were tried and
			// both reported "APStartCell" straight after a move into Windhelm (2026-09-16).
			{
				const char* where = a_side.arrivalDescription;
				std::thread([where]() {
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					if (auto* task = SKSE::GetTaskInterface())
					{
						task->AddTask([where]() {
							auto* player = RE::PlayerCharacter::GetSingleton();
							const auto* cell = player ? player->GetParentCell() : nullptr;
							logger::info("move: the player is now in cell {} (expected: {})",
										 cell ? cell->GetFormEditorID() : "<unknown>", where);
						});
					}
				}).detach();
			}
			return true;
		}

		// Two passes on purpose. Handing the whole set over before anything is worn means the equips are
		// not competing with items still arriving, and every piece is forced on rather than offered: an
		// ordinary equip is a request the actor can decline, and in the first live test that is exactly
		// what happened - cuirass, boots and gauntlets went on and the helmet quietly did not (2026-09-16).
		void GiveKit(const starts::Side& a_side, RE::PlayerCharacter* a_player, int& a_given, int& a_equipped,
					 std::string& a_problem)
		{
			std::vector<RE::TESBoundObject*> given;
			given.reserve(a_side.kitCount);

			for (std::size_t i = 0; i < a_side.kitCount; ++i)
			{
				const auto& item = a_side.kit[i];
				auto* object = Vanilla<RE::TESBoundObject>(item.formID);
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

			if (!settings::start::equipStarterEquipment) { return; }
			auto* equipManager = RE::ActorEquipManager::GetSingleton();
			if (!equipManager)
			{
				logger::error("kit: the equip manager is not available; the uniform is in the pack but not worn");
				if (a_problem.empty()) { a_problem = "the uniform could not be worn"; }
				return;
			}
			for (std::size_t i = 0; i < given.size(); ++i)
			{
				if (!given[i] || !a_side.kit[i].equip) { continue; }
				equipManager->EquipObject(a_player, given[i], nullptr, 1, nullptr,
										  /*queueEquip*/ true, /*forceEquip*/ true, /*playSounds*/ false,
										  /*applyNow*/ false);
				++a_equipped;
				logger::debug("kit: equipped {}", a_side.kit[i].name);
			}
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

		// A watchdog, not a second mechanism.
		//
		// The start hangs on one event firing, and an event that does not fire looks exactly like a player
		// who chose a different start - the mod would sit there silently and Alternate Perspective would
		// eventually decide the start was broken and drop the player in the Helgen inn. So the quests are
		// also asked directly, a few times a second for a short while after the game begins. Whichever
		// notices first runs the sequence; QueueRun only lets one of them through.
		void StartWatchdogThread()
		{
			std::thread([]() {
				for (int i = 0; i < 120; ++i)  // 30 seconds at 250ms
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
					if (g_chosen.load()) { return; }
					auto* task = SKSE::GetTaskInterface();
					if (!task) { continue; }
					task->AddTask([]() {
						if (g_chosen.load()) { return; }
						for (std::size_t side = 0; side < starts::kSideCount; ++side)
						{
							auto* quest = OurQuest(side);
							// IsEnabled() is the only one of these that means what it sounds like.
							// IsRunning() is "not stopping and not mid-promotion" and is TRUE for a quest
							// that has never run, so a watchdog built on it fires on every ordinary save
							// load - which is exactly what happened on the first live test: loading a save
							// teleported the player to Windhelm and handed them a uniform they never asked
							// for (2026-09-16).
							if (!quest || !quest->IsEnabled()) { continue; }
							g_chosen.store(true);
							logger::info("watchdog: the {} quest is enabled but no start event was seen; running it",
										 starts::kSides[side].key);
							QueueRun(side, "watchdog");
							return;
						}
					});
				}
			}).detach();
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
		StartWatchdogThread();
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

		if (settings::start::giveStarterEquipment) { GiveKit(side, player, report.itemsGiven, report.itemsEquipped, report.problem); }
		else { logger::info("kit: skipped (bGiveStarterEquipment=0)"); }

		if (settings::start::startCivilWarQuest) { report.questStarted = StartQuestline(side, report.problem); }
		else { logger::info("questline: skipped (bStartCivilWarQuest=0)"); }

		// Our own quests have nothing left to do once one has run, and a quest left running is a quest that
		// shows up in save inspections forever. Both are stopped, not just the one that ran: the other was
		// never started, and Stop() on a stopped quest is harmless.
		for (std::size_t i = 0; i < starts::kSideCount; ++i)
		{
			if (auto* ours = OurQuest(i); ours && ours->IsEnabled())
			{
				ours->Stop();
				logger::debug("start: our own {} quest stopped", starts::kSides[i].key);
			}
		}

		report.ran = true;
		logger::info("start ({}, {}): done - moved={} given={} equipped={} questline={}{}",
					 side.key, report.reason, report.moved, report.itemsGiven, report.itemsEquipped,
					 report.questStarted, report.problem.empty() ? "" : std::format(" (problem: {})", report.problem));
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
