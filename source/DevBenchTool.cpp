#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Settings.h"
#include "Start.h"
#include "Starts.h"
#include "utils/Logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace DevBenchTool
{
	namespace
	{
		std::string EscapeJson(std::string_view a_in)
		{
			std::string out;
			out.reserve(a_in.size() + 8);
			for (const char c : a_in)
			{
				switch (c)
				{
				case '\\': out += "\\\\"; break;
				case '"': out += "\\\""; break;
				case '\n': out += "\\n"; break;
				default: out += c; break;
				}
			}
			return out;
		}

		// The value of a top-level JSON member, as text: a quoted string or a bare word. Empty when absent.
		std::string Get(std::string_view a_json, const char* a_name)
		{
			const std::string key = std::format("\"{}\"", a_name);
			auto pos = a_json.find(key);
			if (pos == std::string_view::npos) { return {}; }
			pos = a_json.find(':', pos + key.size());
			if (pos == std::string_view::npos) { return {}; }
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) { ++pos; }
			if (pos >= a_json.size()) { return {}; }
			if (a_json[pos] == '"')
			{
				std::string out;
				for (++pos; pos < a_json.size() && a_json[pos] != '"'; ++pos)
				{
					if (a_json[pos] == '\\' && pos + 1 < a_json.size()) { ++pos; }
					out += a_json[pos];
				}
				return out;
			}
			std::string out;
			while (pos < a_json.size() && a_json[pos] != ',' && a_json[pos] != '}' && a_json[pos] != ' ') { out += a_json[pos++]; }
			return out;
		}

		// The handler runs on DevBench's own thread; moving the player and starting a quest are main-thread work.
		bool RunOnMainThread(std::function<void()> a_fn, int a_timeoutMs = 8000)
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) { return false; }
			auto done = std::make_shared<std::atomic<bool>>(false);
			auto m = std::make_shared<std::mutex>();
			auto cv = std::make_shared<std::condition_variable>();
			tasks->AddTask([=]() {
				a_fn();
				{
					std::scoped_lock l(*m);
					done->store(true);
				}
				cv->notify_all();
			});
			std::unique_lock l(*m);
			return cv->wait_for(l, std::chrono::milliseconds(a_timeoutMs), [&]() { return done->load(); });
		}

		std::string StateJson()
		{
			return std::format(
				"{{\"ok\":true,\"op\":\"state\",\"mod\":\"{}\",\"plugin\":\"{}\",{},"
				"\"settings\":{{\"enabled\":{},\"startDelaySeconds\":{:.2f},\"moveToWarRoom\":{},"
				"\"giveStarterEquipment\":{},\"equipStarterEquipment\":{},\"startCivilWarQuest\":{},\"logLevel\":{}}}}}",
				EscapeJson(starts::kDisplayName), starts::kPluginName, start::StateJson(),
				settings::general::enabled ? "true" : "false", settings::general::startDelaySeconds,
				settings::start::moveToWarRoom ? "true" : "false",
				settings::start::giveStarterEquipment ? "true" : "false",
				settings::start::equipStarterEquipment ? "true" : "false",
				settings::start::startCivilWarQuest ? "true" : "false", settings::debug::logLevel);
		}

		void StartTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_argsJson ? a_argsJson : "";
			const std::string op = Get(args, "op");

			if (op == "run")
			{
				const std::string sideText = Get(args, "side");
				const std::size_t side = start::SideFromKey(sideText);
				if (side >= starts::kSideCount)
				{
					a_write(a_sink, std::format("{{\"ok\":false,\"op\":\"run\",\"error\":\"need side=\\\"{}\\\" or \\\"{}\\\"\"}}",
												starts::kSides[0].key, starts::kSides[1].key).c_str());
					return;
				}
				bool ok = false;
				if (!RunOnMainThread([&]() { ok = start::Run(side, "devbench"); }))
				{
					a_write(a_sink, R"({"ok":false,"op":"run","error":"main thread did not run the task in time"})");
					return;
				}
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"run\",\"side\":\"{}\",{}}}",
											ok ? "true" : "false", starts::kSides[side].key, start::StateJson()).c_str());
				return;
			}
			if (op == "reload")
			{
				bool ok = false;
				RunOnMainThread([&]() { ok = settings::Reload(); });
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"reload\"}}", ok ? "true" : "false").c_str());
				return;
			}
			if (op == "save")
			{
				bool ok = false;
				RunOnMainThread([&]() { ok = settings::Save(); });
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"save\"}}", ok ? "true" : "false").c_str());
				return;
			}
			if (op == "set")
			{
				const std::string name = Get(args, "name");
				const std::string value = Get(args, "value");
				if (name.empty() || value.empty())
				{
					a_write(a_sink, R"({"ok":false,"op":"set","error":"need name and value"})");
					return;
				}
				const bool on = value == "1" || value == "true" || value == "yes";
				bool known = true;
				if (name == "enabled") { settings::general::enabled = on; }
				else if (name == "moveToWarRoom") { settings::start::moveToWarRoom = on; }
				else if (name == "giveStarterEquipment") { settings::start::giveStarterEquipment = on; }
				else if (name == "equipStarterEquipment") { settings::start::equipStarterEquipment = on; }
				else if (name == "startCivilWarQuest") { settings::start::startCivilWarQuest = on; }
				else if (name == "startDelaySeconds") { try { settings::general::startDelaySeconds = std::stof(value); } catch (...) { known = false; } }
				else { known = false; }
				if (!known)
				{
					a_write(a_sink, std::format("{{\"ok\":false,\"op\":\"set\",\"error\":\"unknown setting \\\"{}\\\"\"}}", EscapeJson(name)).c_str());
					return;
				}
				a_write(a_sink, std::format("{{\"ok\":true,\"op\":\"set\",\"name\":\"{}\",\"value\":\"{}\"}}", EscapeJson(name), EscapeJson(value)).c_str());
				return;
			}

			a_write(a_sink, StateJson().c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"civilwarstarts.start\" tool"); }
			else { logger::debug("DevBench not detected yet; will retry at the next message"); }
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Alternate Perspective Civil War Starts. op=state (default): the settings, and for "
			"each side the marker it moves to, the quest it starts, whether its own start quest is enabled and what "
			"the last run did. op=run with side (stormcloak|imperial): run that whole start sequence now - move the "
			"player to that side's war room, give and wear that side's uniform, and start CW01B or CW01A - without "
			"going through Alternate Perspective's menu. op=set with name (enabled, moveToWarRoom, "
			"giveStarterEquipment, equipStarterEquipment, startCivilWarQuest, startDelaySeconds) and value: change "
			"one setting for this session. op=save writes the settings to the INI; op=reload re-reads it.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"side\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("civilwarstarts.start", descriptor, &StartTool, nullptr))
		{
			logger::info("Registered \"civilwarstarts.start\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
