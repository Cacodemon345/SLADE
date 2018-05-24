
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    StringUtils.cpp
// Description: Various string utility functions
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
#include "App.h"
#include "Archive/ArchiveManager.h"
#include "StringUtils.h"
#include "Tokenizer.h"
#include <regex>


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace StrUtil
{
std::regex re_int1{ "^[+-]?[0-9]+[0-9]*$" };
std::regex re_int2{ "^0[0-9]+$" };
std::regex re_int3{ "^0x[0-9A-Fa-f]+$" };
std::regex re_float{ "^[-+]?[0-9]*.?[0-9]+([eE][-+]?[0-9]+)?$" };
} // namespace StrUtil


// -----------------------------------------------------------------------------
//
// StringUtils Namespace Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// StringUtils::isInteger
//
// Returns true if [str] is a valid integer. If [allow_hex] is true, can also
// be a valid hex string
// -----------------------------------------------------------------------------
bool StrUtil::isInteger(string_view str, bool allow_hex)
{
	return std::regex_search(str.data(), re_int1) || std::regex_search(str.data(), re_int2)
		   || (allow_hex && std::regex_search(str.data(), re_int3));
}

// -----------------------------------------------------------------------------
// StringUtils::isHex
//
// Returns true if [str] is a valid hex string
// -----------------------------------------------------------------------------
bool StrUtil::isHex(string_view str)
{
	return std::regex_search(str.data(), re_int3);
}

// -----------------------------------------------------------------------------
// StringUtils::isFloat
//
// Returns true if [str] is a valid floating-point number
// -----------------------------------------------------------------------------
bool StrUtil::isFloat(string_view str)
{
	if (str.empty() || str[0] == '$')
		return false;

	return std::regex_search(str.data(), re_float);
}

bool StrUtil::equalCI(string_view left, string_view right)
{
	auto sz = left.size();
	if (right.size() != sz)
		return false;

	for (auto a = 0u; a < sz; ++a)
		if (tolower(left[a]) != tolower(right[a]))
			return false;

	return true;
	// return _stricmp(left.data(), right.data()) == 0;
}

bool StrUtil::equalCI(string_view left, const char* right)
{
	auto a  = 0u;
	auto sz = left.size();
	while (a < sz && right[a] != 0)
	{
		if (tolower(left[a]) != tolower(right[a]))
			return false;
		++a;
	}

	return true;

	// return _stricmp(left.data(), right) == 0;
}

bool StrUtil::startsWith(string_view str, string_view check)
{
	return check.size() <= str.size() && str.compare(0, check.size(), check) == 0;
}

bool StrUtil::startsWith(string_view str, char check)
{
	return !str.empty() && str[0] == check;
}

bool StrUtil::startsWithCI(string_view str, string_view check)
{
	const auto c_size = check.size();
	if (c_size > str.size())
		return false;

	for (unsigned c = 0; c < c_size; ++c)
		if (tolower(str[c]) != tolower(check[c]))
			return false;

	return true;
}

bool StrUtil::startsWithCI(string_view str, char check)
{
	return !str.empty() && tolower(str[0]) == tolower(check);
}

bool StrUtil::endsWith(string_view str, string_view check)
{
	return check.size() <= str.size() && str.compare(str.size() - check.size(), check.size(), check) == 0;
}

bool StrUtil::endsWith(string_view str, char check)
{
	return !str.empty() && str.back() == check;
}

bool StrUtil::endsWithCI(string_view str, string_view check)
{
	const auto c_size = check.size();
	if (c_size > str.size())
		return false;

	const auto s_size = str.size();
	for (unsigned c = 0; c < c_size; ++c)
		if (tolower(str[s_size - c_size + c]) != tolower(check[c]))
			return false;

	return true;
}

bool endsWithCI(string_view str, char check)
{
	return !str.empty() && tolower(str.back()) == tolower(check);
}

bool StrUtil::contains(string_view str, char check)
{
	return str.find(check) != string::npos;
}

bool StrUtil::containsCI(string_view str, char check)
{
	const auto lc = tolower(check);
	for (auto c : str)
		if (tolower(c) == lc)
			return true;

	return false;
}

bool StrUtil::contains(string_view str, string_view check)
{
	return str.find(check) != string::npos;
}

bool StrUtil::containsCI(string_view str, string_view check)
{
	// TODO: Could be improved to avoid copying strings
	return lower(str).find(lower(check)) != string::npos;
}

bool StrUtil::matches(string_view str, string_view check)
{
	return std::regex_search(str.data(), std::regex(wildcardToRegex(check)));
}

bool StrUtil::matchesCI(string_view str, string_view check)
{
	return std::regex_search(str.data(), std::regex(wildcardToRegex(check)));
}

// -----------------------------------------------------------------------------
// StringUtils::escapedString
//
// Returns a copy of [str] with escaped double quotes and backslashes.
// If [swap_backslash] is true, instead of escaping it will swap backslashes to
// forward slashes
// -----------------------------------------------------------------------------
string StrUtil::escapedString(string_view str, bool swap_backslash)
{
	auto escaped = str.to_string();

	replaceIP(escaped, SLASH_BACK, swap_backslash ? SLASH_FORWARD : ESCAPED_SLASH_BACK);
	replaceIP(escaped, QUOTE_DOUBLE, ESCAPED_QUOTE_DOUBLE);

	return escaped;
}

void StrUtil::replaceIP(string& str, string_view from, string_view to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from.data(), start_pos)) != string::npos)
	{
		str.replace(start_pos, from.length(), to.data(), to.size());
		start_pos += to.length();
	}
}

string StrUtil::replace(string_view str, string_view from, string_view to)
{
	auto s = str.to_string();
	replaceIP(s, from, to);
	return s;
}

void StrUtil::replaceFirstIP(string& str, string_view from, string_view to)
{
	auto pos = str.find(from.data(), 0);
	if (pos != string::npos)
		str.replace(pos, from.length(), to.data(), to.size());
}

string StrUtil::replaceFirst(string_view str, string_view from, string_view to)
{
	auto s = str.to_string();
	replaceFirstIP(s, from, to);
	return s;
}

void StrUtil::lowerIP(string& str)
{
	transform(str.begin(), str.end(), str.begin(), ::tolower);
}

void StrUtil::upperIP(string& str)
{
	transform(str.begin(), str.end(), str.begin(), ::toupper);
}

string StrUtil::lower(string_view str)
{
	auto s = str.to_string();
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

string StrUtil::upper(string_view str)
{
	auto s = str.to_string();
	transform(s.begin(), s.end(), s.begin(), ::toupper);
	return s;
}

void StrUtil::ltrimIP(string& str)
{
	str.erase(0, str.find_first_not_of(WHITESPACE_CHARACTERS));
}

void StrUtil::rtrimIP(string& str)
{
	str.erase(0, str.find_last_not_of(WHITESPACE_CHARACTERS) + 1);
}

void StrUtil::trimIP(string& str)
{
	str.erase(0, str.find_first_not_of(WHITESPACE_CHARACTERS));    // left
	str.erase(0, str.find_last_not_of(WHITESPACE_CHARACTERS) + 1); // right
}

string StrUtil::ltrim(string_view str)
{
	auto s = str.to_string();
	s.erase(0, s.find_first_not_of(WHITESPACE_CHARACTERS));
	return s;
}

string StrUtil::rtrim(string_view str)
{
	auto s = str.to_string();
	s.erase(0, s.find_last_not_of(WHITESPACE_CHARACTERS) + 1);
	return s;
}

string StrUtil::trim(string_view str)
{
	auto s = str.to_string();
	s.erase(0, s.find_first_not_of(WHITESPACE_CHARACTERS));    // left
	s.erase(0, s.find_last_not_of(WHITESPACE_CHARACTERS) + 1); // right
	return s;
}

void StrUtil::capitalizeIP(string& str)
{
	if (str.empty())
		return;

	transform(str.begin(), str.end(), str.begin(), ::tolower);
	str[0] = ::toupper(str[0]);
}

string StrUtil::capitalize(string_view str)
{
	if (str.empty())
		return {};

	auto s = str.to_string();
	transform(s.begin(), s.end(), s.begin(), ::tolower);
	s[0] = ::toupper(s[0]);
	return s;
}

string StrUtil::wildcardToRegex(string_view str)
{
	// Process [str] to be a valid regex string
	// (replace ? with . and add . before any *)
	auto regex = str.to_string();
	for (unsigned c = 0; c < regex.size(); ++c)
	{
		if (regex[c] == '?')
			regex[c] = '.';
		else if (regex[c] == '*')
		{
			regex.insert(regex.begin() + c, '.');
			++c;
		}
	}

	return regex;
}

string StrUtil::prepend(string_view str, string_view prefix)
{
	string s{ str.data(), str.size() };
	s.insert(s.begin(), prefix.begin(), prefix.end());
	return s;
}

void StrUtil::prependIP(string& str, string_view prefix)
{
	str.insert(str.begin(), prefix.begin(), prefix.end());
}

string StrUtil::transform(const string_view str, int options)
{
	auto s = str.to_string();

	// Trim
	if (options & TrimLeft)
		s.erase(0, s.find_first_not_of(WHITESPACE_CHARACTERS));
	if (options & TrimRight)
		s.erase(0, s.find_last_not_of(WHITESPACE_CHARACTERS) + 1);

	// Case change
	if (options & LowerCase)
		transform(s.begin(), s.end(), s.begin(), ::tolower);
	else if (options & UpperCase)
		transform(s.begin(), s.end(), s.begin(), ::toupper);

	return s;
}

string StrUtil::afterLast(string_view str, char chr)
{
	for (int i = str.size() - 1; i >= 0; --i)
		if (str[i] == chr)
			return str.substr(i + 1).to_string();

	return str.to_string();
}

string StrUtil::afterFirst(string_view str, char chr)
{
	for (unsigned i = 0; i < str.size(); i++)
		if (str[i] == chr)
			return str.substr(i + 1).to_string();

	return str.to_string();
}

string StrUtil::beforeLast(string_view str, char chr)
{
	for (int i = str.size() - 1; i >= 0; --i)
		if (str[i] == chr)
			return str.substr(0, i).to_string();

	return str.to_string();
}

string StrUtil::beforeFirst(string_view str, char chr)
{
	for (unsigned i = 0; i < str.size(); i++)
		if (str[i] == chr)
			return str.substr(0, i).to_string();

	return str.to_string();
}

vector<string> StrUtil::split(string_view str, char separator)
{
	unsigned       start = 0;
	auto           size  = str.size();
	vector<string> split;
	for (unsigned c = 0; c < size; ++c)
	{
		if (str[c] == separator)
		{
			split.emplace_back(str.substr(start, c - start));
			start = c + 1;
		}
	}

	split.emplace_back(str.substr(start, size - start));

	return split;
}

vector<string_view> StrUtil::splitToViews(string_view str, char separator)
{
	unsigned            start = 0;
	auto                size  = str.size();
	vector<string_view> split;
	for (unsigned c = 0; c < size; ++c)
	{
		if (str[c] == separator)
		{
			split.push_back(str.substr(start, c - start));
			start = c + 1;
		}
	}

	split.push_back(str.substr(start, size - start));

	return split;
}

string StrUtil::truncate(string_view str, unsigned length)
{
	if (str.size() > length)
		return { str.data(), length };
	else
		return { str.data(), str.size() };
}

void StrUtil::truncateIP(string& str, unsigned length)
{
	if (str.size() > length)
		str.resize(length);
}

string StrUtil::removeLast(string_view str, unsigned n)
{
	if (str.size() >= n)
		return {};

	return { str.data(), str.size() - n };
}

void StrUtil::removeLastIP(string& str, unsigned n)
{
	if (str.size() < n)
		str.resize(str.size() - n);
}

string StrUtil::removePrefix(string_view str, char prefix)
{
	if (!str.empty() && str[0] == prefix)
		str.remove_prefix(1);
	return { str.data(), str.size() };
}

void StrUtil::removePrefixIP(string& str, char prefix)
{
	if (!str.empty() && str[0] == prefix)
		str.erase(0, 1);
}

string StrUtil::removeSuffix(string_view str, char suffix)
{
	if (!str.empty() && str.back() == suffix)
		str.remove_suffix(1);
	return { str.data(), str.size() };
}

void StrUtil::removeSuffixIP(string& str, char suffix)
{
	if (!str.empty() && str.back() == suffix)
		str.pop_back();
}

StrUtil::Path::Path(string_view full_path) : full_path_{ full_path.data(), full_path.size() }
{
	// Enforce / as separators
	std::replace(full_path_.begin(), full_path_.end(), '\\', '/');

	auto last_sep_pos = full_path_.find_last_of('/');
	filename_start_   = last_sep_pos == string::npos ? 0 : last_sep_pos + 1;
	auto ext_pos      = full_path_.find('.');
	filename_end_     = ext_pos == string::npos ? full_path_.size() : ext_pos;
}

string_view StrUtil::Path::path(bool include_end_sep) const
{
	if (filename_start_ == 0 || filename_start_ == string::npos)
		return {};

	return include_end_sep ? string_view{ full_path_.data(), filename_start_ } :
							 string_view{ full_path_.data(), filename_start_ - 1 };
}

string_view StrUtil::Path::fileName(bool include_extension) const
{
	if (filename_start_ == string::npos)
		return {};

	return include_extension ? string_view{ full_path_.data() + filename_start_ } :
							   string_view{ full_path_.data() + filename_start_, filename_end_ - filename_start_ };
}

string_view StrUtil::Path::extension() const
{
	if (filename_start_ == string::npos || filename_end_ >= full_path_.size())
		return {};

	return string_view{ full_path_.data() + filename_end_ + 1 };
}

vector<string_view> StrUtil::Path::pathParts() const
{
	if (filename_start_ == 0 || filename_start_ == string::npos)
		return {};

	return splitToViews({ full_path_.data(), filename_start_ - 1 }, '/');
}

bool StrUtil::Path::hasExtension() const
{
	return filename_start_ != string::npos && filename_end_ < full_path_.size();
}

void StrUtil::Path::set(string_view full_path)
{
	S_SET_VIEW(full_path_, full_path);

	// Enforce / as separators
	std::replace(full_path_.begin(), full_path_.end(), '\\', '/');

	auto last_sep_pos = full_path_.find_last_of('/');
	filename_start_   = last_sep_pos == string::npos ? 0 : last_sep_pos + 1;
	auto ext_pos      = full_path_.find('.');
	filename_end_     = ext_pos == string::npos ? full_path_.size() : ext_pos;
}

void StrUtil::Path::setPath(string_view path)
{
	if (filename_start_ == string::npos)
		return;

	// Ensure given path doesn't begin or end with a separator
	if (path.back() == '/' || path.back() == '\\')
		path.remove_suffix(1);
	if (path[0] == '/' || path[0] == '\\')
		path.remove_prefix(1);

	if (filename_start_ == 0)
	{
		// No path, insert path and separator at beginning
		full_path_.insert(full_path_.begin(), '/');
		full_path_.insert(full_path_.begin(), path.begin(), path.end());
		filename_start_ += path.size() + 1;
		filename_end_ += path.size() + 1;
	}
	else
	{
		full_path_.replace(0, filename_start_ - 1, path.data(), path.size());
		auto fn_start_old = filename_start_;
		filename_start_   = path.size() + 1;
		filename_end_ -= fn_start_old - filename_start_;
	}
}

void StrUtil::Path::setPath(const vector<string_view>& parts) {}

void StrUtil::Path::setPath(const vector<string>& parts) {}

void StrUtil::Path::setFileName(string_view file_name)
{
	if (filename_start_ == string::npos)
		return;

	if (filename_start_ >= full_path_.size())
	{
		filename_start_ = full_path_.size();
		full_path_.append(file_name.data(), file_name.size());
		filename_end_ = full_path_.size();
	}
	else
	{
		full_path_.replace(filename_start_, filename_end_ - filename_start_, file_name.data(), file_name.size());
		filename_end_ = filename_start_ + file_name.size();
	}

	// Sanitize new filename
	std::replace(full_path_.begin() + filename_start_, full_path_.begin() + filename_end_, '/', '_');
	std::replace(full_path_.begin() + filename_start_, full_path_.begin() + filename_end_, '\\', '_');
	std::replace(full_path_.begin() + filename_start_, full_path_.begin() + filename_end_, '.', '_');
}

void StrUtil::Path::setExtension(string_view extension)
{
	if (extension.empty())
	{
		full_path_.erase(filename_end_);
		return;
	}

	if (filename_end_ < full_path_.size())
		full_path_.erase(filename_end_ + 1);
	else
		full_path_.push_back('.');

	full_path_.append(extension.data(), extension.size());

	// Sanitize new extension
	std::replace(full_path_.begin() + filename_end_, full_path_.end(), '/', '_');
	std::replace(full_path_.begin() + filename_end_, full_path_.end(), '\\', '_');
}

string_view StrUtil::Path::fileNameOf(string_view full_path, bool include_extension)
{
	int pos;
	for (pos = full_path.size() - 1; pos > 0; --pos)
		if (full_path[pos - 1] == '/' || full_path[pos - 1] == '\\')
			break;

	if (!include_extension)
	{
		auto ext_pos = full_path.find('.', pos);
		if (ext_pos != string_view::npos)
			full_path.remove_suffix(full_path.size() - ext_pos);
	}

	return pos > 0 ? full_path.substr(pos) : full_path;
}

string_view StrUtil::Path::extensionOf(string_view full_path)
{
	// Ignore any '.' in path
	auto last_sep_pos = full_path.find_last_of("/\\");
	if (last_sep_pos == string_view::npos)
		last_sep_pos = 0;

	auto ext_pos = full_path.find('.', last_sep_pos);
	return ext_pos == string_view::npos ? string_view{} : full_path.substr(ext_pos + 1);
}

string_view StrUtil::Path::pathOf(string_view full_path, bool include_end_sep)
{
	auto last_sep_pos = full_path.find_last_of("/\\");
	return last_sep_pos == string_view::npos ? string_view{} :
											   full_path.substr(0, include_end_sep ? last_sep_pos + 1 : last_sep_pos);
}

// -----------------------------------------------------------------------------
// StringUtils::processIncludes
//
// Reads the text file at [filename], processing any #include statements in the
// file recursively. The resulting 'expanded' text is written to [out]
// -----------------------------------------------------------------------------
void StrUtil::processIncludes(string filename, string& out)
{
	// Open file
	wxTextFile file;
	if (!file.Open(filename))
		return;

	// Get file path
	Path fn(filename);
	auto path = fn.path(true);

	// Go through line-by-line
	string    line = file.GetNextLine().ToStdString();
	Tokenizer tz;
	tz.setSpecialCharacters("");
	while (!file.Eof())
	{
		// Check for #include
		if (startsWithCI(ltrim(line), "#include"))
		{
			// Get filename to include
			tz.openString(line);
			tz.adv(); // Skip #include

			// Process the file
			processIncludes(fmt::format("{}{}", path, tz.next().text), out);
		}
		else
			out.append(line + "\n");

		line = file.GetNextLine();
	}
}

// -----------------------------------------------------------------------------
// StringUtils::processIncludes
//
// Reads the text entry [entry], processing any #include statements in the
// entry text recursively. This will search in the resource folder and archive
// as well as in the parent archive. The resulting 'expanded' text is written
// to [out]
// -----------------------------------------------------------------------------
void StrUtil::processIncludes(ArchiveEntry* entry, string& out, bool use_res)
{
	// Check entry was given
	if (!entry)
		return;

	// Write entry to temp file
	string filename = App::path(entry->name(), App::Dir::Temp);
	entry->exportFile(filename);

	// Open file
	wxTextFile file;
	if (!file.Open(filename))
		return;

	// Go through line-by-line
	Tokenizer tz;
	tz.setSpecialCharacters("");
	string line = file.GetFirstLine().ToStdString();
	while (!file.Eof())
	{
		// Check for #include
		if (startsWithCI(ltrim(line), "#include"))
		{
			// Get name of entry to include
			tz.openString(line);
			string name = entry->path() + tz.next().text;

			// Get the entry
			bool          done      = false;
			ArchiveEntry* entry_inc = entry->parent()->entryAtPath(name);
			// DECORATE paths start from the root, not from the #including entry's directory
			if (!entry_inc)
				entry_inc = entry->parent()->entryAtPath(tz.current().text);
			if (entry_inc)
			{
				processIncludes(entry_inc, out);
				done = true;
			}
			else
				Log::warning(2, fmt::sprintf("Couldn't find entry to #include: %s", name));

			// Look in resource pack
			if (use_res && !done && App::archiveManager().programResourceArchive())
			{
				name      = "config/games/" + tz.current().text;
				entry_inc = App::archiveManager().programResourceArchive()->entryAtPath(name);
				if (entry_inc)
				{
					processIncludes(entry_inc, out);
					done = true;
				}
			}

			// Okay, we've exhausted all possibilities
			if (!done)
				Log::error(fmt::sprintf(
					"Attempting to #include nonexistant entry \"%s\" from entry %s", name, entry->name()));
		}
		else
			out.append(line + "\n");

		line = file.GetNextLine();
	}

	// Delete temp file
	wxRemoveFile(filename);
}

int StrUtil::toInt(const string& str)
{
	try
	{
		return std::stoi(str);
	}
	catch (const std::exception& ex)
	{
		Log::error(fmt::format("Can't convert \"{}\" to an integer: {}", str, ex.what()));
		return 0;
	}
}

float StrUtil::toFloat(const string& str)
{
	try
	{
		return std::stof(str);
	}
	catch (const std::exception& ex)
	{
		Log::error(fmt::format("Can't convert \"{}\" to a float: {}", str, ex.what()));
		return 0.f;
	}
}

double StrUtil::toDouble(const string& str)
{
	try
	{
		return std::stod(str);
	}
	catch (const std::exception& ex)
	{
		Log::error(fmt::format("Can't convert \"{}\" to a double: {}", str, ex.what()));
		return 0.;
	}
}

bool StrUtil::toBoolean(const string& str)
{
	// Empty, 0 or "false" are false, everything else true
	return !(str.empty() || str == "0" || equalCI(str, "false"));
}
