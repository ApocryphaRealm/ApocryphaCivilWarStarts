#include "PCH.h"

#include "Kits.h"

#include "Starts.h"
#include "utils/Logger.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

namespace kits
{
	namespace
	{
		inline constexpr const char* kFileName = "CivilWarStarts_Kits.json";

		// ---- a small JSON reader --------------------------------------------------------------------------------
		// Just enough for this one file: objects, arrays, strings, numbers, true/false/null. No dependency, so a
		// malformed file is an error message with a line and column rather than a crash.
		struct Json
		{
			enum class Type { Null, Bool, Number, String, Array, Object };
			Type type = Type::Null;
			bool boolean = false;
			double number = 0.0;
			std::string text;
			std::vector<Json> items;
			std::vector<std::pair<std::string, Json>> members;

			const Json* Find(std::string_view a_key) const
			{
				if (type != Type::Object) { return nullptr; }
				for (const auto& m : members) { if (m.first == a_key) { return &m.second; } }
				return nullptr;
			}
		};

		class Parser
		{
		public:
			explicit Parser(std::string_view a_text) : _s(a_text) {}

			bool Parse(Json& a_out, std::string& a_error)
			{
				SkipSpace();
				if (!Value(a_out, 0)) { a_error = _error; return false; }
				SkipSpace();
				if (_i < _s.size()) { Fail("unexpected text after the end of the file's JSON"); a_error = _error; return false; }
				return true;
			}

		private:
			std::string_view _s;
			std::size_t _i = 0;
			std::string _error;

			bool Fail(const std::string& a_what)
			{
				if (_error.empty())
				{
					std::size_t line = 1, col = 1;
					for (std::size_t k = 0; k < _i && k < _s.size(); ++k)
					{
						if (_s[k] == '\n') { ++line; col = 1; } else { ++col; }
					}
					_error = std::format("{} (line {}, column {})", a_what, line, col);
				}
				return false;
			}

			void SkipSpace()
			{
				while (_i < _s.size() && (_s[_i] == ' ' || _s[_i] == '\t' || _s[_i] == '\r' || _s[_i] == '\n')) { ++_i; }
			}

			bool Literal(std::string_view a_word)
			{
				if (_s.substr(_i, a_word.size()) == a_word) { _i += a_word.size(); return true; }
				return false;
			}

			static void AppendUtf8(std::string& a_out, std::uint32_t a_cp)
			{
				if (a_cp < 0x80) { a_out += static_cast<char>(a_cp); }
				else if (a_cp < 0x800) { a_out += static_cast<char>(0xC0 | (a_cp >> 6)); a_out += static_cast<char>(0x80 | (a_cp & 0x3F)); }
				else if (a_cp < 0x10000)
				{
					a_out += static_cast<char>(0xE0 | (a_cp >> 12));
					a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
					a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
				}
				else
				{
					a_out += static_cast<char>(0xF0 | (a_cp >> 18));
					a_out += static_cast<char>(0x80 | ((a_cp >> 12) & 0x3F));
					a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
					a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
				}
			}

			bool Hex4(std::uint32_t& a_out)
			{
				if (_i + 4 > _s.size()) { return Fail("a \\u escape is cut short"); }
				std::uint32_t v = 0;
				for (int k = 0; k < 4; ++k)
				{
					const char c = _s[_i++];
					v <<= 4;
					if (c >= '0' && c <= '9') { v |= static_cast<std::uint32_t>(c - '0'); }
					else if (c >= 'a' && c <= 'f') { v |= static_cast<std::uint32_t>(c - 'a' + 10); }
					else if (c >= 'A' && c <= 'F') { v |= static_cast<std::uint32_t>(c - 'A' + 10); }
					else { return Fail("a \\u escape has a character that is not a hex digit"); }
				}
				a_out = v;
				return true;
			}

			bool String(std::string& a_out)
			{
				// the opening quote is at _i
				++_i;
				while (_i < _s.size())
				{
					const char c = _s[_i++];
					if (c == '"') { return true; }
					if (c == '\n') { --_i; return Fail("a string runs onto the next line (a quote is missing)"); }
					if (c != '\\') { a_out += c; continue; }
					if (_i >= _s.size()) { break; }
					const char e = _s[_i++];
					switch (e)
					{
					case '"': a_out += '"'; break;
					case '\\': a_out += '\\'; break;
					case '/': a_out += '/'; break;
					case 'b': a_out += '\b'; break;
					case 'f': a_out += '\f'; break;
					case 'n': a_out += '\n'; break;
					case 'r': a_out += '\r'; break;
					case 't': a_out += '\t'; break;
					case 'u':
					{
						std::uint32_t cp = 0;
						if (!Hex4(cp)) { return false; }
						if (cp >= 0xD800 && cp <= 0xDBFF && _s.substr(_i, 2) == "\\u")
						{
							_i += 2;
							std::uint32_t low = 0;
							if (!Hex4(low)) { return false; }
							cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
						}
						AppendUtf8(a_out, cp);
						break;
					}
					default: --_i; return Fail("a string has a backslash followed by a character JSON does not allow (write a Windows path with \\\\)");
					}
				}
				return Fail("a string is never closed");
			}

			bool Value(Json& a_out, int a_depth)
			{
				if (a_depth > 24) { return Fail("the JSON is nested too deeply"); }
				if (_i >= _s.size()) { return Fail("the file ends where a value was expected"); }
				const char c = _s[_i];
				if (c == '{') { return Object(a_out, a_depth); }
				if (c == '[') { return Array(a_out, a_depth); }
				if (c == '"') { a_out.type = Json::Type::String; return String(a_out.text); }
				if (Literal("true")) { a_out.type = Json::Type::Bool; a_out.boolean = true; return true; }
				if (Literal("false")) { a_out.type = Json::Type::Bool; a_out.boolean = false; return true; }
				if (Literal("null")) { a_out.type = Json::Type::Null; return true; }
				if (c == '-' || (c >= '0' && c <= '9'))
				{
					const std::size_t start = _i;
					while (_i < _s.size() && (std::isdigit(static_cast<unsigned char>(_s[_i])) || _s[_i] == '-' || _s[_i] == '+' ||
											   _s[_i] == '.' || _s[_i] == 'e' || _s[_i] == 'E')) { ++_i; }
					const std::string num(_s.substr(start, _i - start));
					char* end = nullptr;
					a_out.number = std::strtod(num.c_str(), &end);
					if (!end || *end != '\0') { _i = start; return Fail("a number is not valid"); }
					a_out.type = Json::Type::Number;
					return true;
				}
				return Fail("a value was expected here (JSON allows no comments - use a \"comment\" entry instead)");
			}

			bool Array(Json& a_out, int a_depth)
			{
				a_out.type = Json::Type::Array;
				++_i;
				SkipSpace();
				if (_i < _s.size() && _s[_i] == ']') { ++_i; return true; }
				while (true)
				{
					SkipSpace();
					Json item;
					if (!Value(item, a_depth + 1)) { return false; }
					a_out.items.push_back(std::move(item));
					SkipSpace();
					if (_i >= _s.size()) { return Fail("a list is never closed (a ] is missing)"); }
					if (_s[_i] == ',') { ++_i; continue; }
					if (_s[_i] == ']') { ++_i; return true; }
					return Fail("a comma or ] was expected here (is a comma missing between two entries?)");
				}
			}

			bool Object(Json& a_out, int a_depth)
			{
				a_out.type = Json::Type::Object;
				++_i;
				SkipSpace();
				if (_i < _s.size() && _s[_i] == '}') { ++_i; return true; }
				while (true)
				{
					SkipSpace();
					if (_i >= _s.size() || _s[_i] != '"') { return Fail("a quoted name was expected here (a trailing comma, or a missing quote?)"); }
					std::string key;
					if (!String(key)) { return false; }
					SkipSpace();
					if (_i >= _s.size() || _s[_i] != ':') { return Fail("a colon was expected after the name"); }
					++_i;
					SkipSpace();
					Json value;
					if (!Value(value, a_depth + 1)) { return false; }
					a_out.members.emplace_back(std::move(key), std::move(value));
					SkipSpace();
					if (_i >= _s.size()) { return Fail("an object is never closed (a } is missing)"); }
					if (_s[_i] == ',') { ++_i; continue; }
					if (_s[_i] == '}') { ++_i; return true; }
					return Fail("a comma or } was expected here (is a comma missing between two entries?)");
				}
			}
		};

		// ---- the table in use -------------------------------------------------------------------------------------
		std::vector<Source> g_sides[starts::kSideCount];
		std::string g_origin = "not loaded yet";

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
				case '\r': break;
				case '\t': out += ' '; break;
				default: out += c; break;
				}
			}
			return out;
		}

		std::string PluginList(const Source& a_source)
		{
			std::string out;
			for (const auto& p : a_source.plugins) { if (!out.empty()) { out += ", "; } out += p; }
			return out;
		}

		// The compiled table (Starts.h), as the same runtime shape the file produces.
		std::vector<Source> BuiltIn(std::size_t a_side)
		{
			static constexpr const char* kSlots[] = { "body", "boots", "gauntlets", "helmet", "shield", "weapon" };
			std::vector<Source> out;
			const auto& side = starts::kSides[a_side];
			for (std::size_t s = 0; s < side.sourceCount; ++s)
			{
				const auto& src = side.sources[s];
				Source source;
				source.id = src.plugin ? src.plugin : "vanilla";
				source.label = src.pluginName ? src.pluginName : "the game's own gear";
				if (src.plugin) { source.plugins.emplace_back(src.plugin); }
				for (std::size_t i = 0; i < src.count; ++i)
				{
					Piece piece;
					piece.formID = src.items[i].formID;
					piece.slot = i < std::size(kSlots) ? kSlots[i] : "";
					piece.name = src.items[i].name;
					piece.optional = false;
					source.pieces.push_back(std::move(piece));
				}
				out.push_back(std::move(source));
			}
			return out;
		}

		bool ParseFormID(const std::string& a_text, std::uint32_t& a_out)
		{
			std::string_view v = a_text;
			if (v.size() > 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) { v.remove_prefix(2); }
			if (v.empty() || v.size() > 8) { return false; }
			std::uint32_t value = 0;
			for (const char c : v)
			{
				value <<= 4;
				if (c >= '0' && c <= '9') { value |= static_cast<std::uint32_t>(c - '0'); }
				else if (c >= 'a' && c <= 'f') { value |= static_cast<std::uint32_t>(c - 'a' + 10); }
				else if (c >= 'A' && c <= 'F') { value |= static_cast<std::uint32_t>(c - 'A' + 10); }
				else { return false; }
			}
			a_out = value;
			return value != 0;
		}

		std::string StringOf(const Json* a_json)
		{
			return a_json && a_json->type == Json::Type::String ? a_json->text : std::string();
		}

		// One source from the file. On a mistake: false, and a_problem says what, so the log can name it.
		bool ReadSource(const Json& a_json, std::size_t a_index, Source& a_out, std::string& a_problem)
		{
			if (a_json.type != Json::Type::Object) { a_problem = "it is not an object ({ ... })"; return false; }
			a_out.id = StringOf(a_json.Find("id"));
			a_out.label = StringOf(a_json.Find("label"));
			if (a_out.id.empty()) { a_out.id = !a_out.label.empty() ? a_out.label : std::format("source{}", a_index + 1); }
			if (a_out.label.empty()) { a_out.label = a_out.id; }

			if (const auto* plugins = a_json.Find("plugins"))
			{
				if (plugins->type != Json::Type::Array) { a_problem = "\"plugins\" must be a list of file names"; return false; }
				for (const auto& p : plugins->items)
				{
					if (p.type != Json::Type::String || p.text.empty()) { a_problem = "\"plugins\" holds something that is not a file name"; return false; }
					a_out.plugins.push_back(p.text);
				}
			}

			const auto* pieces = a_json.Find("pieces");
			if (!pieces || pieces->type != Json::Type::Array || pieces->items.empty()) { a_problem = "it has no \"pieces\" list"; return false; }
			std::size_t n = 0;
			for (const auto& pj : pieces->items)
			{
				++n;
				if (pj.type != Json::Type::Object) { a_problem = std::format("piece {} is not an object", n); return false; }
				Piece piece;
				piece.slot = StringOf(pj.Find("slot"));
				piece.name = StringOf(pj.Find("name"));
				if (!ParseFormID(StringOf(pj.Find("form")), piece.formID))
				{
					a_problem = std::format("piece {} (\"{}\") needs a \"form\" written as hex text, like \"0x000804\"", n, piece.name);
					return false;
				}
				if (piece.name.empty()) { a_problem = std::format("piece {} ({:06X}) needs a \"name\"", n, piece.formID); return false; }
				if (const auto* opt = pj.Find("optional"))
				{
					if (opt->type != Json::Type::Bool) { a_problem = std::format("piece {} (\"{}\"): \"optional\" must be true or false", n, piece.name); return false; }
					piece.optional = opt->boolean;
				}
				a_out.pieces.push_back(std::move(piece));
			}
			return true;
		}

		// One side's list from the file; false when the file gives this side nothing usable.
		bool ReadSide(const Json& a_root, const char* a_key, std::vector<Source>& a_out)
		{
			const auto* list = a_root.Find(a_key);
			if (!list)
			{
				logger::warn("kits: the file has no \"{}\" list; that side uses the built-in table", a_key);
				return false;
			}
			if (list->type != Json::Type::Array)
			{
				logger::warn("kits: \"{}\" is not a list; that side uses the built-in table", a_key);
				return false;
			}
			std::size_t index = 0;
			for (const auto& sj : list->items)
			{
				Source source;
				std::string problem;
				if (ReadSource(sj, index, source, problem)) { a_out.push_back(std::move(source)); }
				else { logger::warn("kits: {} source {} ({}) is skipped: {}", a_key, index + 1, StringOf(sj.Find("id")), problem); }
				++index;
			}
			if (a_out.empty())
			{
				logger::warn("kits: \"{}\" has no usable source; that side uses the built-in table", a_key);
				return false;
			}
			return true;
		}

		void UseBuiltIn(const std::string& a_why)
		{
			for (std::size_t i = 0; i < starts::kSideCount; ++i) { g_sides[i] = BuiltIn(i); }
			g_origin = "built-in table (" + a_why + ")";
		}

		// ---- resolving a piece ------------------------------------------------------------------------------------
		RE::TESBoundObject* Lookup(const Source& a_source, const std::string& a_file, const Piece& a_piece)
		{
			if (a_source.plugins.empty()) { return RE::TESForm::LookupByID<RE::TESBoundObject>(a_piece.formID); }
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) { return nullptr; }
			// The UNTEMPLATED LookupForm, then As<>. TESDataHandler::LookupForm<T> tests form->Is(T::FORMTYPE), an
			// EXACT type match, so asking it for a TESBoundObject (a base class no form actually is) returns null for
			// every armour and weapon in the game. It compiles and silently finds nothing (2026-09-16: all six Sons of
			// Skyrim pieces reported "not in the game" while the plugin was plainly loaded). TESForm::LookupByID<T> uses
			// As<T>(), which walks the hierarchy, which is why the vanilla half of the same kit worked and hid the fault.
			auto* form = handler->LookupForm(a_piece.formID, a_file);
			return form ? form->As<RE::TESBoundObject>() : nullptr;
		}

		struct Say
		{
			bool quiet;
			template <class... Args>
			void Info(std::format_string<Args...> a_fmt, Args&&... a_args) const
			{
				if (!quiet) { logger::info("{}", std::format(a_fmt, std::forward<Args>(a_args)...)); }
			}
			template <class... Args>
			void Warn(std::format_string<Args...> a_fmt, Args&&... a_args) const
			{
				if (!quiet) { logger::warn("{}", std::format(a_fmt, std::forward<Args>(a_args)...)); }
			}
		};

		// Try one source. Fills a_choice and returns true when it is usable; a_why is the reason when it is not.
		bool TrySource(const Source& a_source, Choice& a_choice, Verdict& a_verdict, const Say& a_say, const char* a_side)
		{
			std::string file;
			if (!a_source.plugins.empty())
			{
				auto* handler = RE::TESDataHandler::GetSingleton();
				for (const auto& p : a_source.plugins)
				{
					if (handler && handler->LookupModByName(p) != nullptr) { file = p; break; }
				}
				if (file.empty())
				{
					a_verdict.installed = false;
					a_verdict.why = std::format("not installed (looked for: {})", PluginList(a_source));
					a_say.Info("kit: {} is not installed (looked for: {})", a_source.label, PluginList(a_source));
					return false;
				}
			}
			a_verdict.installed = true;

			std::vector<Resolved> resolved;
			for (const auto& piece : a_source.pieces)
			{
				auto* object = Lookup(a_source, file, piece);
				const char* actual = object ? object->GetName() : nullptr;
				// Names are checked for a source that names a plugin: a plugin that has moved on would otherwise hand out
				// whatever record now sits at that FormID. The game's own gear is not name-checked, because a replacer
				// mod renames the vanilla records it overrides and that is the look being asked for.
				const bool nameOk = a_source.plugins.empty() || (actual && std::string_view(actual) == piece.name);
				if (object && nameOk)
				{
					resolved.push_back({ object, piece.slot, piece.name, piece.formID });
					continue;
				}
				const std::string found = object ? (actual && *actual ? std::format("\"{}\"", actual) : std::string("a record with no name")) : std::string("nothing");
				if (piece.optional)
				{
					a_say.Info("kit: {}: the optional {} \"{}\" ({:06X}) is not there (found {}); the kit goes without it",
							   a_source.label, piece.slot, piece.name, piece.formID, found);
					continue;
				}
				a_verdict.why = std::format("{} {:06X} should be \"{}\" but is {}", piece.slot.empty() ? "piece" : piece.slot, piece.formID, piece.name, found);
				a_say.Warn("kit: {} - the {} at {:06X} should be \"{}\" but the game has {}; the plugin has changed since this list was read, so this source is passed over for the {} start",
						   a_source.label, piece.slot.empty() ? "piece" : piece.slot, piece.formID, piece.name, found, a_side);
				return false;
			}
			if (resolved.empty())
			{
				a_verdict.why = "none of its pieces is in the game";
				a_say.Warn("kit: {} - none of its pieces is in the game; passed over", a_source.label);
				return false;
			}
			a_choice.found = true;
			a_choice.sourceId = a_source.id;
			a_choice.sourceLabel = a_source.label;
			a_choice.pluginFile = file;
			a_choice.pieces = std::move(resolved);
			a_verdict.usable = true;
			a_verdict.why = "ok";
			return true;
		}
	}

	bool Load()
	{
		const auto path = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / kFileName;
		const std::string shown = path.string();

		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			logger::warn("kits: {} was not found; the built-in table is used (the Sons of Skyrim and Civil War Guard Armor Replacer lists only)", shown);
			UseBuiltIn("file missing");
			return false;
		}
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		std::string_view view = text;
		if (view.size() >= 3 && static_cast<unsigned char>(view[0]) == 0xEF && static_cast<unsigned char>(view[1]) == 0xBB && static_cast<unsigned char>(view[2]) == 0xBF)
		{
			view.remove_prefix(3);
		}

		Json root;
		std::string error;
		Parser parser(view);
		if (!parser.Parse(root, error))
		{
			logger::error("kits: {} is not valid JSON - {}; the built-in table is used", shown, error);
			UseBuiltIn("file broken");
			return false;
		}
		if (root.type != Json::Type::Object)
		{
			logger::error("kits: {} must hold one object ({{ ... }}) with a list per side; the built-in table is used", shown);
			UseBuiltIn("file has the wrong shape");
			return false;
		}

		std::string summary;
		for (std::size_t i = 0; i < starts::kSideCount; ++i)
		{
			std::vector<Source> read;
			if (!ReadSide(root, starts::kSides[i].key, read)) { read = BuiltIn(i); }
			g_sides[i] = std::move(read);
			summary += std::format("{}{}: ", i ? "; " : "", starts::kSides[i].key);
			for (std::size_t s = 0; s < g_sides[i].size(); ++s) { summary += std::format("{}{}", s ? ", " : "", g_sides[i][s].id); }
		}
		g_origin = "file " + shown;
		logger::info("kits: read {} - {}", shown, summary);
		return true;
	}

	const std::string& Origin()
	{
		return g_origin;
	}

	const std::vector<Source>& SourcesFor(std::size_t a_side)
	{
		static const std::vector<Source> none;
		return a_side < starts::kSideCount ? g_sides[a_side] : none;
	}

	Choice Choose(std::size_t a_side, bool a_quiet)
	{
		Choice choice;
		if (a_side >= starts::kSideCount) { return choice; }
		const Say say{ a_quiet };
		const char* sideKey = starts::kSides[a_side].key;
		const auto& sources = g_sides[a_side];

		for (const auto& source : sources)
		{
			Verdict verdict;
			verdict.id = source.id;
			verdict.label = source.label;
			const bool ok = TrySource(source, choice, verdict, say, sideKey);
			choice.verdicts.push_back(verdict);
			if (ok)
			{
				if (source.plugins.empty()) { say.Info("kit: no kit mod is installed for the {} start, so it uses the game's own gear ({})", sideKey, source.label); }
				else { say.Info("kit: {} is installed, so the {} start uses its gear ({}, {} piece(s))", source.label, sideKey, choice.pluginFile, choice.pieces.size()); }
				return choice;
			}
		}

		// Every source failed, or the file left out the one that needs no mod. The compiled table's last source is the
		// game's own gear: take it, skipping any piece the game does not have, so a side is never handed nothing.
		const std::vector<Source> builtIn = BuiltIn(a_side);
		const Source& vanilla = builtIn.back();
		say.Warn("kit: no source in the file was usable for the {} start; falling back to the game's own gear", sideKey);
		choice.found = true;
		choice.sourceId = vanilla.id;
		choice.sourceLabel = vanilla.label;
		choice.pluginFile.clear();
		choice.pieces.clear();
		for (const auto& piece : vanilla.pieces)
		{
			if (auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(piece.formID)) { choice.pieces.push_back({ object, piece.slot, piece.name, piece.formID }); }
		}
		Verdict verdict;
		verdict.id = vanilla.id;
		verdict.label = vanilla.label;
		verdict.installed = true;
		verdict.usable = !choice.pieces.empty();
		verdict.why = "last resort (the file listed no usable source)";
		choice.verdicts.push_back(verdict);
		return choice;
	}

	std::string DescribeJson()
	{
		std::string sides;
		for (std::size_t i = 0; i < starts::kSideCount; ++i)
		{
			const Choice choice = Choose(i, true);
			std::string tried;
			for (std::size_t v = 0; v < choice.verdicts.size(); ++v)
			{
				const auto& d = choice.verdicts[v];
				tried += std::format("{}{{\"id\":\"{}\",\"label\":\"{}\",\"installed\":{},\"usable\":{},\"why\":\"{}\"}}",
									 v ? "," : "", EscapeJson(d.id), EscapeJson(d.label), d.installed ? "true" : "false",
									 d.usable ? "true" : "false", EscapeJson(d.why));
			}
			std::string pieces;
			for (std::size_t p = 0; p < choice.pieces.size(); ++p)
			{
				const auto& r = choice.pieces[p];
				const char* actual = r.object ? r.object->GetName() : nullptr;
				pieces += std::format("{}{{\"slot\":\"{}\",\"form\":\"{:06X}\",\"expected\":\"{}\",\"actual\":\"{}\",\"runtimeFormID\":\"{:08X}\"}}",
									  p ? "," : "", EscapeJson(r.slot), r.formID, EscapeJson(r.name),
									  EscapeJson(actual ? actual : ""), r.object ? r.object->GetFormID() : 0u);
			}
			sides += std::format("{}{{\"side\":\"{}\",\"listed\":{},\"chosen\":\"{}\",\"file\":\"{}\",\"tried\":[{}],\"pieces\":[{}]}}",
								 i ? "," : "", starts::kSides[i].key, g_sides[i].size(),
								 EscapeJson(choice.sourceId), EscapeJson(choice.pluginFile), tried, pieces);
		}
		return std::format("\"kitsOrigin\":\"{}\",\"kits\":[{}]", EscapeJson(g_origin), sides);
	}
}
