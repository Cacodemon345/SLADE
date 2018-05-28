
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    ZScript.cpp
// Description: ZScript definition classes and parsing
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "ZScript.h"
#include "Archive/Archive.h"
#include "Archive/ArchiveManager.h"
#include "Utility/StringUtils.h"
#include "Utility/Tokenizer.h"

using namespace ZScript;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace ZScript
{
EntryType* etype_zscript = nullptr;

// ZScript keywords (can't be function/variable names)
vector<string> keywords = { "class",      "default", "private",  "static", "native",   "return",       "if",
							"else",       "for",     "while",    "do",     "break",    "continue",     "deprecated",
							"state",      "null",    "readonly", "true",   "false",    "struct",       "extend",
							"clearscope", "vararg",  "ui",       "play",   "virtual",  "virtualscope", "meta",
							"Property",   "version", "in",       "out",    "states",   "action",       "override",
							"super",      "is",      "let",      "const",  "replaces", "protected",    "self" };

// For test_parse_zscript console command
bool dump_parsed_blocks    = false;
bool dump_parsed_states    = false;
bool dump_parsed_functions = false;

string db_comment = "//$";
} // namespace ZScript


// -----------------------------------------------------------------------------
//
// Local Functions
//
// -----------------------------------------------------------------------------
namespace ZScript
{
// -----------------------------------------------------------------------------
// Writes a log [message] of [type] beginning with the location of [statement]
// -----------------------------------------------------------------------------
void logParserMessage(ParsedStatement& statement, Log::MessageType type, const string& message)
{
	string location = "<unknown location>";
	if (statement.entry)
		location = statement.entry->path(true);

	Log::message(type, fmt::format("{}:{}: {}", location, statement.line, message));
}

// -----------------------------------------------------------------------------
// Parses a ZScript type (eg. 'class<Actor>') from [tokens] beginning at [index]
// -----------------------------------------------------------------------------
string parseType(const vector<string>& tokens, unsigned& index)
{
	string type;

	// Qualifiers
	while (index < tokens.size())
	{
		if (StrUtil::equalCI(tokens[index], "in") || StrUtil::equalCI(tokens[index], "out"))
			type += tokens[index++] + " ";
		else
			break;
	}

	type += tokens[index];

	// Check for ...
	if (index + 2 < tokens.size() && tokens[index] == '.' && tokens[index + 1] == '.' && tokens[index + 2] == '.')
	{
		type = "...";
		index += 2;
	}

	// Check for <>
	if (index + 1 < tokens.size() && tokens[index + 1] == "<")
	{
		type += "<";
		index += 2;
		while (index < tokens.size() && tokens[index] != ">")
			type += tokens[index++];
		type += ">";
		++index;
	}

	++index;

	return type;
}

// -----------------------------------------------------------------------------
// Parses a ZScript value from [tokens] beginning at [index]
// -----------------------------------------------------------------------------
string parseValue(const vector<string>& tokens, unsigned& index)
{
	string value;
	while (true)
	{
		// Read between ()
		if (tokens[index] == '(')
		{
			int level = 1;
			value += tokens[index++];
			while (level > 0)
			{
				if (tokens[index] == '(')
					++level;
				if (tokens[index] == ')')
					--level;

				value += tokens[index++];
			}

			continue;
		}

		if (tokens[index] == ',' || tokens[index] == ';' || tokens[index] == ')')
			break;

		value += tokens[index++];

		if (index >= tokens.size())
			break;
	}

	return value;
}

// -----------------------------------------------------------------------------
// Checks for a ZScript keyword+value statement in [tokens] beginning at
// [index], eg. deprecated("#.#") or version("#.#")
// Returns true if there is a keyword+value statement and writes the value to
// [value]
// -----------------------------------------------------------------------------
bool checkKeywordValueStatement(const vector<string>& tokens, unsigned index, const string& word, string& value)
{
	if (index + 3 >= tokens.size())
		return false;

	if (StrUtil::equalCI(tokens[index], word) && tokens[index + 1] == '(' && tokens[index + 3] == ')')
	{
		value = tokens[index + 2];
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
// Parses all statements/blocks in [entry], adding them to [parsed]
// -----------------------------------------------------------------------------
void parseBlocks(ArchiveEntry* entry, vector<ParsedStatement>& parsed)
{
	Tokenizer tz;
	tz.setSpecialCharacters(Tokenizer::DEFAULT_SPECIAL_CHARACTERS + "()+-[]&!?.");
	tz.enableDecorate(true);
	tz.setCommentTypes(Tokenizer::CommentTypes::CPPStyle | Tokenizer::CommentTypes::CStyle);
	tz.openMem(entry->data(), "ZScript");

	while (!tz.atEnd())
	{
		// Preprocessor
		if (!tz.current().text.empty() && tz.current().text[0] == '#')
		{
			if (tz.checkNC("#include"))
			{
				auto inc_entry = entry->relativeEntry(tz.next().text);

				// Check #include path could be resolved
				if (!inc_entry)
				{
					Log::warning(fmt::format(
						"Warning parsing ZScript entry {}: "
						"Unable to find #included entry \"{}\" at line {}, skipping",
						entry->name(),
						tz.current().text,
						tz.current().line_no));
				}
				else
					parseBlocks(inc_entry, parsed);
			}

			tz.advToNextLine();
			continue;
		}

		// Version
		else if (tz.checkNC("version"))
		{
			tz.advToNextLine();
			continue;
		}

		// ZScript
		parsed.push_back({});
		parsed.back().entry = entry;
		if (!parsed.back().parse(tz))
			parsed.pop_back();
	}

	// Set entry type
	if (etype_zscript && entry->type() != etype_zscript)
		entry->setType(etype_zscript);
}

bool isKeyword(const string& word)
{
	for (auto& kw : keywords)
		if (StrUtil::equalCI(word, kw))
			return true;

	return false;
}

} // namespace ZScript


// -----------------------------------------------------------------------------
//
// Enumerator Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Parses an enumerator block [statement]
// -----------------------------------------------------------------------------
bool Enumerator::parse(ParsedStatement& statement)
{
	// Check valid statement
	if (statement.block.empty())
		return false;
	if (statement.tokens.size() < 2)
		return false;

	// Parse name
	name_ = statement.tokens[1];

	// Parse values
	unsigned index = 0;
	unsigned count = statement.block[0].tokens.size();
	while (index < count)
	{
		string val_name = statement.block[0].tokens[index];

		// TODO: Parse value

		values_.push_back({ val_name, 0 });

		// Skip past next ,
		while (index + 1 < count)
			if (statement.block[0].tokens[++index] == ',')
				break;

		index++;
	}

	return true;
}


// -----------------------------------------------------------------------------
//
// Function Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Parses a function parameter from [tokens] beginning at [index]
// -----------------------------------------------------------------------------
unsigned Function::Parameter::parse(const vector<string>& tokens, unsigned index)
{
	// Type
	type = parseType(tokens, index);

	// Special case - '...'
	if (type == "...")
	{
		name = "...";
		type.clear();
		return index;
	}

	// Name
	if (index >= tokens.size() || tokens[index] == ')')
		return index;
	name = tokens[index++];

	// Default value
	if (index < tokens.size() && tokens[index] == '=')
	{
		++index;
		default_value = parseValue(tokens, index);
	}

	return index;
}

// -----------------------------------------------------------------------------
// Parses a function declaration [statement]
// -----------------------------------------------------------------------------
bool Function::parse(ParsedStatement& statement)
{
	unsigned index;
	int      last_qualifier = -1;
	for (index = 0; index < statement.tokens.size(); index++)
	{
		if (StrUtil::equalCI(statement.tokens[index], "virtual"))
		{
			virtual_       = true;
			last_qualifier = index;
		}
		else if (StrUtil::equalCI(statement.tokens[index], "static"))
		{
			static_        = true;
			last_qualifier = index;
		}
		else if (StrUtil::equalCI(statement.tokens[index], "native"))
		{
			native_        = true;
			last_qualifier = index;
		}
		else if (StrUtil::equalCI(statement.tokens[index], "action"))
		{
			action_        = true;
			last_qualifier = index;
		}
		else if (StrUtil::equalCI(statement.tokens[index], "override"))
		{
			override_      = true;
			last_qualifier = index;
		}
		else if ((int)index > last_qualifier + 2 && statement.tokens[index] == '(')
		{
			name_        = statement.tokens[index - 1];
			return_type_ = statement.tokens[index - 2];
			break;
		}
		else if (checkKeywordValueStatement(statement.tokens, index, "deprecated", deprecated_))
			index += 3;
		else if (checkKeywordValueStatement(statement.tokens, index, "version", version_))
			index += 3;
	}

	if (name_.empty() || return_type_.empty())
	{
		logParserMessage(statement, Log::MessageType::Warning, "Function parse failed");
		return false;
	}

	// Name can't be a keyword
	if (isKeyword(name_))
	{
		logParserMessage(statement, Log::MessageType::Warning, "Function name can't be a keyword");
		return false;
	}

	// Parse parameters
	while (statement.tokens[index] != '(')
	{
		if (index == statement.tokens.size())
			return true;
		++index;
	}
	++index; // Skip (

	while (index < statement.tokens.size()  && statement.tokens[index] != ')')
	{
		parameters_.emplace_back();
		index = parameters_.back().parse(statement.tokens, index);

		if (index < statement.tokens.size() && statement.tokens[index] == ',')
			++index;
	}

	if (dump_parsed_functions)
		Log::debug(asString());

	return true;
}

// -----------------------------------------------------------------------------
// Returns a string representation of the function
// -----------------------------------------------------------------------------
string Function::asString()
{
	string str;
	if (!deprecated_.empty())
		str += fmt::format("deprecated v{} ", deprecated_);
	if (static_)
		str += "static ";
	if (native_)
		str += "native ";
	if (virtual_)
		str += "virtual ";
	if (action_)
		str += "action ";

	str += fmt::format("{} {}(", return_type_, name_);

	for (auto& p : parameters_)
	{
		str += fmt::format("{} {}", p.type, p.name);
		if (!p.default_value.empty())
			str += " = " + p.default_value;

		if (&p != &parameters_.back())
			str += ", ";
	}
	str += ")";

	return str;
}

// -----------------------------------------------------------------------------
// Returns true if [statement] is a valid function declaration
// -----------------------------------------------------------------------------
bool Function::isFunction(ParsedStatement& statement)
{
	// Need at least type, name, (, )
	if (statement.tokens.size() < 4)
		return false;

	// Check for ( before =
	bool special_func = false;
	for (auto& token : statement.tokens)
	{
		if (token == '=')
			return false;

		if (!special_func && token == '(')
			return true;

		if (StrUtil::equalCI(token, "deprecated") || StrUtil::equalCI(token, "version"))
			special_func = true;
		else if (special_func && token == ')')
			special_func = false;
	}

	// No ( found
	return false;
}


// -----------------------------------------------------------------------------
//
// State Struct Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the first valid frame sprite (eg. TNT1 A -> TNT1A?)
// -----------------------------------------------------------------------------
string State::editorSprite()
{
	if (frames.empty())
		return "";

	for (auto& f : frames)
		if (!f.sprite_frame.empty())
			return f.sprite_base + f.sprite_frame[0] + "?";

	return "";
}


// -----------------------------------------------------------------------------
//
// StateTable Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Parses a states definition statement/block [states]
// -----------------------------------------------------------------------------
bool StateTable::parse(ParsedStatement& states)
{
	vector<string> current_states;
	for (auto& statement : states.block)
	{
		if (statement.tokens.empty())
			continue;

		auto states_added = false;
		auto index        = 0u;
		auto n_tokens     = statement.tokens.size();

		// Check for state labels
		for (auto a = 0u; a < n_tokens; ++a)
		{
			if (statement.tokens[a] == '(')
				break;

			if (statement.tokens[a] == ':')
			{
				// Ignore ::
				if (a + 1 < n_tokens && statement.tokens[a + 1] == ':')
				{
					++a;
					continue;
				}

				if (!states_added)
					current_states.clear();

				string state;
				for (auto b = index; b < a; ++b)
					state += statement.tokens[b];

				current_states.push_back(StrUtil::lower(state));
				if (state_first_.empty())
					state_first_ = state;
				states_added = true;

				index = a + 1;
			}
		}

		if (index >= n_tokens)
		{
			logParserMessage(
				statement,
				Log::MessageType::Warning,
				fmt::format("Failed to parse states block beginning on line {}", states.line));
			continue;
		}

		// Ignore state commands
		if (StrUtil::equalCI(statement.tokens[index], "stop") || StrUtil::equalCI(statement.tokens[index], "goto")
			|| StrUtil::equalCI(statement.tokens[index], "loop") || StrUtil::equalCI(statement.tokens[index], "wait")
			|| StrUtil::equalCI(statement.tokens[index], "fail"))
			continue;

		State::Frame frame;

		// Sprite base
		// Check for "----" (will be 4 separate tokens)
		if (statement.tokens[index] == StrUtil::DASH)
		{
			while (index < n_tokens)
			{
				if (statement.tokens[index] != StrUtil::DASH)
					break;
				frame.sprite_base += statement.tokens[index++];
			}
		}
		else
			frame.sprite_base = statement.tokens[index++];

		if (index >= n_tokens)
		{
			logParserMessage(
				statement,
				Log::MessageType::Warning,
				fmt::format("Failed to parse states block beginning on line {}", states.line));
			continue;
		}

		// Sprite frame
		while (index < n_tokens && statement.tokens[index] != '-'
			   && !StrUtil::isInteger(statement.tokens[index], false))
			frame.sprite_frame += statement.tokens[index++];

		if (index >= n_tokens)
		{
			logParserMessage(
				statement,
				Log::MessageType::Warning,
				fmt::format("Failed to parse states block beginning on line {}", states.line));
			continue;
		}

		// Duration
		// Check for negative number
		if (statement.tokens[index] == "-" && index + 1 < n_tokens)
			frame.duration = -StrUtil::toInt(statement.tokens[index + 1]);
		else
			frame.duration = -StrUtil::toInt(statement.tokens[index]);

		for (auto& state : current_states)
			states_[state].frames.push_back(frame);

		//if (index + 2 < statement.tokens.size())
		//{
		//	// Parse duration
		//	int duration;
		//	//if (statement.tokens[index + 2] == "-" && index + 3 < statement.tokens.size())
		//	//{
		//	//	// Negative number
		//	//	duration = StrUtil::toInt(statement.tokens[index + 3]);
		//	//	duration = -duration;
		//	//}
		//	//else
		//		duration = StrUtil::toInt(statement.tokens[index + 2]);

		//	for (auto& state : current_states)
		//		states_[state].frames.push_back(
		//			{ statement.tokens[index], statement.tokens[index + 1], (int)duration });
		//}
	}

	states_.erase("");

	if (dump_parsed_states)
	{
		for (auto& state : states_)
		{
			Log::debug(fmt::format("State {}:", state.first));
			for (auto& frame : state.second.frames)
				Log::debug(fmt::format(
					"Sprite: {}, Frames: {}, Duration: {}", frame.sprite_base, frame.sprite_frame, frame.duration));
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Returns the most appropriate sprite from the state table to use for the
// editor.
// Uses a state priority: Idle > See > Inactive > Spawn > [first defined]
// -----------------------------------------------------------------------------
string StateTable::editorSprite()
{
	if (!states_["idle"].frames.empty())
		return states_["idle"].editorSprite();
	if (!states_["see"].frames.empty())
		return states_["see"].editorSprite();
	if (!states_["inactive"].frames.empty())
		return states_["inactive"].editorSprite();
	if (!states_["spawn"].frames.empty())
		return states_["spawn"].editorSprite();
	if (!states_[state_first_].frames.empty())
		return states_[state_first_].editorSprite();

	return "";
}


// -----------------------------------------------------------------------------
//
// Class Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Parses a class definition statement/block [class_statement]
// -----------------------------------------------------------------------------
bool Class::parse(ParsedStatement& class_statement)
{
	if (class_statement.tokens.size() < 2)
	{
		logParserMessage(class_statement, Log::MessageType::Warning, "Class parse failed");
		return false;
	}

	name_ = class_statement.tokens[1];

	for (unsigned a = 0; a < class_statement.tokens.size(); a++)
	{
		// Inherits
		if (class_statement.tokens[a] == ':' && a < class_statement.tokens.size() - 1)
			inherits_class_ = class_statement.tokens[a + 1];

		// Native
		else if (StrUtil::equalCI(class_statement.tokens[a], "native"))
			native_ = true;

		// Deprecated
		else if (checkKeywordValueStatement(class_statement.tokens, a, "deprecated", deprecated_))
			a += 3;

		// Version
		else if (checkKeywordValueStatement(class_statement.tokens, a, "version", version_))
			a += 3;
	}

	if (!parseClassBlock(class_statement.block))
	{
		logParserMessage(class_statement, Log::MessageType::Warning, "Class parse failed");
		return false;
	}

	// Set editor sprite from parsed states
	default_properties_["sprite"] = states_.editorSprite();

	// Add DB comment props to default properties
	for (auto& i : db_properties_)
	{
		// Sprite
		if (StrUtil::equalCI(i.first, "EditorSprite") || StrUtil::equalCI(i.first, "Sprite"))
			default_properties_["sprite"] = i.second;

		// Angled
		else if (StrUtil::equalCI(i.first, "Angled"))
			default_properties_["angled"] = true;
		else if (StrUtil::equalCI(i.first, "NotAngled"))
			default_properties_["angled"] = false;

		// Is Decoration
		else if (StrUtil::equalCI(i.first, "IsDecoration"))
			default_properties_["decoration"] = true;

		// Icon
		else if (StrUtil::equalCI(i.first, "Icon"))
			default_properties_["icon"] = i.second;

		// DB2 Color
		else if (StrUtil::equalCI(i.first, "Color"))
			default_properties_["color"] = i.second;

		// SLADE 3 Colour (overrides DB2 color)
		// Good thing US spelling differs from ABC (Aussie/Brit/Canuck) spelling! :p
		else if (StrUtil::equalCI(i.first, "Colour"))
			default_properties_["colour"] = i.second;

		// Obsolete thing
		else if (StrUtil::equalCI(i.first, "Obsolete"))
			default_properties_["obsolete"] = true;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Parses a class definition block [block] only (ignores the class declaration
// statement line, used for 'extend class')
// -----------------------------------------------------------------------------
bool Class::extend(ParsedStatement& block)
{
	return parseClassBlock(block.block);
}

// -----------------------------------------------------------------------------
// Adds this class as a ThingType to [parsed], or updates an existing ThingType
// definition in [types] or [parsed]
// -----------------------------------------------------------------------------
void Class::toThingType(std::map<int, Game::ThingType>& types, vector<Game::ThingType>& parsed)
{
	// Find existing definition
	Game::ThingType* def = nullptr;

	// Check types with ednums first
	for (auto& type : types)
		if (StrUtil::equalCI(name_, type.second.className()))
		{
			def = &type.second;
			break;
		}
	if (!def)
	{
		// Check all parsed types
		for (auto& type : parsed)
			if (StrUtil::equalCI(name_, type.className()))
			{
				def = &type;
				break;
			}
	}

	// Create new type if it didn't exist
	if (!def)
	{
		parsed.emplace_back(name_, "ZScript", name_);
		def = &parsed.back();
	}

	// Set properties from DB comments
	string title = name_;
	string group = "ZScript";
	for (auto& prop : db_properties_)
	{
		if (StrUtil::equalCI(prop.first, "Title"))
			title = prop.second;
		else if (StrUtil::equalCI(prop.first, "Group") || StrUtil::equalCI(prop.first, "Category"))
			group = "ZScript/" + prop.second;
	}
	def->define(def->number(), title, group);

	// Set properties from defaults section
	def->loadProps(default_properties_, true, true);
}

// -----------------------------------------------------------------------------
// Parses a class definition from statements in [block]
// -----------------------------------------------------------------------------
bool Class::parseClassBlock(vector<ParsedStatement>& block)
{
	for (auto& statement : block)
	{
		if (statement.tokens.empty())
			continue;

		// Default block
		if (StrUtil::equalCI(statement.tokens[0], "default"))
		{
			if (!parseDefaults(statement.block))
				return false;
		}

		// Enum
		else if (StrUtil::equalCI(statement.tokens[0], "enum"))
		{
			Enumerator e;
			if (!e.parse(statement))
				return false;

			enumerators_.push_back(e);
		}

		// States
		else if (StrUtil::equalCI(statement.tokens[0], "states"))
			states_.parse(statement);

		// DB property comment
		else if (StrUtil::startsWith(statement.tokens[0], db_comment))
		{
			if (statement.tokens.size() > 1)
				db_properties_.emplace_back(statement.tokens[0].substr(3), statement.tokens[1]);
			else
				db_properties_.emplace_back(statement.tokens[0].substr(3), "true");
		}

		// Function
		else if (Function::isFunction(statement))
		{
			Function fn;
			if (fn.parse(statement))
				functions_.push_back(fn);
		}

		// Variable
		else if (statement.tokens.size() >= 2 && statement.block.empty())
			continue;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Parses a 'default' block from statements in [block]
// -----------------------------------------------------------------------------
bool Class::parseDefaults(vector<ParsedStatement>& defaults)
{
	for (auto& statement : defaults)
	{
		if (statement.tokens.empty())
			continue;

		// DB property comment
		if (StrUtil::startsWith(statement.tokens[0], db_comment))
		{
			if (statement.tokens.size() > 1)
				db_properties_.emplace_back(statement.tokens[0].substr(3), statement.tokens[1]);
			else
				db_properties_.emplace_back(statement.tokens[0].substr(3), "true");
			continue;
		}

		// Flags
		unsigned t     = 0;
		unsigned count = statement.tokens.size();
		while (t < count)
		{
			if (statement.tokens[t] == '+')
				default_properties_[statement.tokens[++t]] = true;
			else if (statement.tokens[t] == '-')
				default_properties_[statement.tokens[++t]] = false;
			else
				break;

			t++;
		}

		if (t >= count)
			continue;

		// Name
		string name = statement.tokens[t];
		if (t + 2 < count && statement.tokens[t + 1] == '.')
		{
			name += "." + statement.tokens[t + 2];
			t += 2;
		}

		// Value
		// For now ignore anything after the first whitespace/special character
		// so stuff like arithmetic expressions or comma separated lists won't
		// really work properly yet
		if (t + 1 < count)
			default_properties_[name] = statement.tokens[t + 1];

		// Name only (no value), set as boolean true
		else if (t < count)
			default_properties_[name] = true;
	}

	return true;
}


// -----------------------------------------------------------------------------
//
// Definitions Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Clears all definitions
// -----------------------------------------------------------------------------
void Definitions::clear()
{
	classes_.clear();
	enumerators_.clear();
	variables_.clear();
	functions_.clear();
}

// -----------------------------------------------------------------------------
// Parses ZScript in [entry]
// -----------------------------------------------------------------------------
bool Definitions::parseZScript(ArchiveEntry* entry)
{
	// Parse into tree of expressions and blocks
	auto                    start = App::runTimer();
	vector<ParsedStatement> parsed;
	parseBlocks(entry, parsed);
	Log::debug(2, fmt::format("parseBlocks: {}ms", App::runTimer() - start));
	start = App::runTimer();

	for (auto& block : parsed)
	{
		if (block.tokens.empty())
			continue;

		if (dump_parsed_blocks)
			block.dump();

		// Class
		if (StrUtil::equalCI(block.tokens[0], "class"))
		{
			Class nc(Class::Type::Class);

			if (!nc.parse(block))
				return false;

			classes_.push_back(nc);
		}

		// Struct
		else if (StrUtil::equalCI(block.tokens[0], "struct"))
		{
			Class nc(Class::Type::Struct);

			if (!nc.parse(block))
				return false;

			classes_.push_back(nc);
		}

		// Extend Class
		else if (
			block.tokens.size() > 2 && StrUtil::equalCI(block.tokens[0], "extend")
			&& StrUtil::equalCI(block.tokens[1], "class"))
		{
			for (auto& c : classes_)
				if (StrUtil::equalCI(c.name(), block.tokens[2]))
				{
					c.extend(block);
					break;
				}
		}

		// Enum
		else if (StrUtil::equalCI(block.tokens[0], "enum"))
		{
			Enumerator e;

			if (!e.parse(block))
				return false;

			enumerators_.push_back(e);
		}
	}

	Log::debug(2, fmt::format("ZScript: {}ms", App::runTimer() - start));

	return true;
}

// -----------------------------------------------------------------------------
// Parses all ZScript entries in [archive]
// -----------------------------------------------------------------------------
bool Definitions::parseZScript(Archive* archive)
{
	// Get base ZScript file
	Archive::SearchOptions opt;
	opt.match_name      = "zscript";
	opt.ignore_ext      = true;
	auto zscript_enries = archive->findAll(opt);
	if (zscript_enries.empty())
		return false;

	Log::info(2, fmt::format("Parsing ZScript entries found in archive {}", archive->filename()));

	// Get ZScript entry type (all parsed ZScript entries will be set to this)
	etype_zscript = EntryType::fromId("zscript");
	if (etype_zscript == EntryType::unknownType())
		etype_zscript = nullptr;

	// Parse ZScript entries
	bool ok = true;
	for (auto entry : zscript_enries)
		if (!parseZScript(entry))
			ok = false;

	return ok;
}

// -----------------------------------------------------------------------------
// Exports all classes to ThingTypes in [types] and [parsed] (from a
// Game::Configuration object)
// -----------------------------------------------------------------------------
void Definitions::exportThingTypes(std::map<int, Game::ThingType>& types, vector<Game::ThingType>& parsed)
{
	for (auto& cdef : classes_)
		cdef.toThingType(types, parsed);
}


// -----------------------------------------------------------------------------
//
// ParsedStatement Struct Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Parses a ZScript 'statement'. This isn't technically correct but suits our
// purposes well enough
//
// tokens
// {
//     block[0].tokens
//     {
//         block[0].block[0].tokens;
//         ...
//     }
//
//     block[1].tokens;
//     ...
// }
// -----------------------------------------------------------------------------
bool ParsedStatement::parse(Tokenizer& tz)
{
	// Check for unexpected token
	if (tz.check('}'))
	{
		tz.adv();
		return false;
	}

	line = tz.lineNo();

	// Tokens
	bool in_initializer = false;
	while (true)
	{
		// End of statement (;)
		if (tz.advIf(';'))
			return true;

		// DB comment
		if (StrUtil::startsWith(tz.current().text, db_comment))
		{
			tokens.push_back(tz.current().text);
			tokens.push_back(tz.getLine());
			return true;
		}

		if (tz.check('}'))
		{
			// End of array initializer
			if (in_initializer)
			{
				in_initializer = false;
				tokens.emplace_back("}");
				tz.adv();
				continue;
			}

			// End of statement
			return true;
		}

		if (tz.atEnd())
		{
			Log::debug(fmt::format("Failed parsing zscript statement/block beginning line {}", line));
			return false;
		}

		// Beginning of block
		if (tz.advIf('{'))
			break;

		// Array initializer: ... = { ... }
		if (tz.current().text == "=" && tz.peek() == '{')
		{
			tokens.emplace_back("=");
			tokens.emplace_back("{");
			tz.adv(2);
			in_initializer = true;
			continue;
		}

		tokens.push_back(tz.current().text);
		tz.adv();
	}

	// Block
	while (true)
	{
		if (tz.advIf('}'))
			return true;

		if (tz.atEnd())
		{
			Log::debug(fmt::format("Failed parsing zscript statement/block beginning line {}", line));
			return false;
		}

		block.push_back({});
		block.back().entry = entry;
		if (!block.back().parse(tz) || block.back().tokens.empty())
			block.pop_back();
	}
}

// -----------------------------------------------------------------------------
// Dumps this statement to the log (debug), indenting by 2*[indent] spaces
// -----------------------------------------------------------------------------
void ParsedStatement::dump(int indent)
{
	string line;
	for (int a = 0; a < indent; a++)
		line += "  ";

	// Tokens
	for (auto& token : tokens)
		line += token + " ";
	Log::debug(line);

	// Blocks
	for (auto& b : block)
		b.dump(indent + 1);
}







// TESTING CONSOLE COMMANDS
#include "General/Console/Console.h"
#include "MainEditor/MainEditor.h"

CONSOLE_COMMAND(test_parse_zscript, 0, false)
{
	dump_parsed_blocks    = false;
	dump_parsed_states    = false;
	dump_parsed_functions = false;
	ArchiveEntry* entry   = nullptr;

	for (auto& arg : args)
	{
		if (StrUtil::equalCI(arg, "dump"))
			dump_parsed_blocks = true;
		else if (StrUtil::equalCI(arg, "states"))
			dump_parsed_states = true;
		else if (StrUtil::equalCI(arg, "func"))
			dump_parsed_functions = true;
		else if (!entry)
			entry = MainEditor::currentArchive()->entryAtPath(arg);
	}

	if (!entry)
		entry = MainEditor::currentEntry();

	if (entry)
	{
		Definitions test;
		if (test.parseZScript(entry))
			Log::console("Parsed Successfully");
		else
			Log::console("Parsing failed");
	}
	else
		Log::console("Select an entry or enter a valid entry name/path");

	dump_parsed_blocks    = false;
	dump_parsed_states    = false;
	dump_parsed_functions = false;
}


CONSOLE_COMMAND(test_parseblocks, 1, false)
{
	int num = StrUtil::toInt(args[0]);

	auto entry = MainEditor::currentEntry();
	if (!entry)
		return;

	auto                    start = App::runTimer();
	vector<ParsedStatement> parsed;
	for (auto a = 0; a < num; ++a)
	{
		parseBlocks(entry, parsed);
		parsed.clear();
	}
	Log::console(fmt::format("Took {}ms", App::runTimer() - start));
}
