
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2019 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    DirArchive.cpp
// Description: DirArchive, archive class that opens a directory and treats it
//              as an archive. All entry data is still stored in memory and only
//              written to the file system when saving the 'archive'
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
#include "DirArchive.h"
#include "App.h"
#include "General/UI.h"
#include "Utility/FileUtils.h"
#include "Utility/StringUtils.h"
#include "WadArchive.h"


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, archive_load_data)


// -----------------------------------------------------------------------------
//
// DirArchive Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// DirArchive class constructor
// -----------------------------------------------------------------------------
DirArchive::DirArchive() : Archive("folder")
{
	// Setup separator character
#ifdef WIN32
	separator_ = '\\';
#else
	separator_ = '/';
#endif

	rootDir()->allowDuplicateNames(false);
}

// -----------------------------------------------------------------------------
// Reads files from the directory [filename] into the archive
// Returns true if successful, false otherwise
// -----------------------------------------------------------------------------
bool DirArchive::open(string_view filename)
{
	UI::setSplashProgressMessage("Reading directory structure");
	UI::setSplashProgress(0);
	vector<string>      files, dirs;
	DirArchiveTraverser traverser(files, dirs);
	wxDir               dir(string{ filename });
	dir.Traverse(traverser, "", wxDIR_FILES | wxDIR_DIRS);

	// Stop announcements (don't want to be announcing modification due to entries being added etc)
	ArchiveModSignalBlocker sig_blocker{ *this };

	UI::setSplashProgressMessage("Reading files");
	for (unsigned a = 0; a < files.size(); a++)
	{
		UI::setSplashProgress((float)a / (float)files.size());

		// Cut off directory to get entry name + relative path
		auto name = files[a];
		name.erase(0, filename.size());
		if (StrUtil::startsWith(name, separator_))
			name.erase(0, 1);

		// Log::info(3, fn.GetPath(true, wxPATH_UNIX));

		// Create entry
		auto fn        = StrUtil::Path{ name };
		auto new_entry = std::make_shared<ArchiveEntry>(fn.fileName());

		// Setup entry info
		new_entry->setLoaded(false);
		new_entry->exProp("filePath") = files[a];

		// Add entry and directory to directory tree
		auto ndir = createDir(fn.path());
		ndir->addEntry(new_entry);
		ndir->dirEntry()->exProp("filePath") = fmt::format("{}{}", filename, fn.path());

		// Read entry data
		new_entry->importFile(files[a]);
		new_entry->setLoaded(true);

		file_modification_times_[new_entry.get()] = wxFileModificationTime(files[a]);

		// Detect entry type
		EntryType::detectEntryType(*new_entry);

		// Unload data if needed
		if (!archive_load_data)
			new_entry->unloadData();
	}

	// Add empty directories
	for (const auto& subdir : dirs)
	{
		auto name = subdir;
		name.erase(0, filename.size());
		StrUtil::removePrefixIP(name, separator_);
		std::replace(name.begin(), name.end(), '\\', '/');

		auto ndir                            = createDir(name);
		ndir->dirEntry()->exProp("filePath") = subdir;
	}

	// Set all entries/directories to unmodified
	vector<ArchiveEntry*> entry_list;
	putEntryTreeAsList(entry_list);
	for (auto& entry : entry_list)
		entry->setState(ArchiveEntry::State::Unmodified);

	// Enable announcements
	sig_blocker.unblock();

	// Setup variables
	filename_ = filename;
	setModified(false);
	on_disk_ = true;

	UI::setSplashProgressMessage("");

	return true;
}

// -----------------------------------------------------------------------------
// Reads an archive from an ArchiveEntry (not implemented)
// -----------------------------------------------------------------------------
bool DirArchive::open(ArchiveEntry* entry)
{
	Global::error = "Cannot open Folder Archive from entry";
	return false;
}

// -----------------------------------------------------------------------------
// Reads data from a MemChunk (not implemented)
// -----------------------------------------------------------------------------
bool DirArchive::open(MemChunk& mc)
{
	Global::error = "Cannot open Folder Archive from memory";
	return false;
}

// -----------------------------------------------------------------------------
// Writes the archive to a MemChunk (not implemented)
// -----------------------------------------------------------------------------
bool DirArchive::write(MemChunk& mc, bool update)
{
	Global::error = "Cannot write Folder Archive to memory";
	return false;
}

// -----------------------------------------------------------------------------
// Writes the archive to a file (not implemented)
// -----------------------------------------------------------------------------
bool DirArchive::write(string_view filename, bool update)
{
	return true;
}

// -----------------------------------------------------------------------------
// Saves any changes to the directory to the file system
// -----------------------------------------------------------------------------
bool DirArchive::save(string_view filename)
{
	// Get flat entry list
	vector<ArchiveEntry*> entries;
	putEntryTreeAsList(entries);

	// Get entry path list
	vector<string> entry_paths;
	for (auto& entry : entries)
	{
		entry_paths.push_back(filename_ + entry->path(true));
		if (separator_ != '/')
			std::replace(entry_paths.back().begin(), entry_paths.back().end(), '/', separator_);
	}

	// Get current directory structure
	long                time = App::runTimer();
	vector<string>      files, dirs;
	DirArchiveTraverser traverser(files, dirs);
	wxDir               dir(filename_);
	dir.Traverse(traverser, "", wxDIR_FILES | wxDIR_DIRS);
	Log::info(2, "GetAllFiles took {}ms", App::runTimer() - time);

	// Check for any files to remove
	time = App::runTimer();
	std::error_code ec;
	for (const auto& removed_file : removed_files_)
	{
		if (FileUtil::fileExists(removed_file))
		{
			Log::info(2, "Removing file {}", removed_file);
			FileUtil::removeFile(removed_file);
		}
	}

	// Check for any directories to remove
	for (int a = dirs.size() - 1; a >= 0; a--)
	{
		// Check if dir path matches an existing dir
		bool found = false;
		for (const auto& entry_path : entry_paths)
		{
			if (dirs[a] == entry_path)
			{
				found = true;
				break;
			}
		}

		// Dir on disk isn't part of the archive in memory
		// (Note that this will fail if there are any untracked files in the
		// directory)
		if (!found && wxRmdir(dirs[a]))
			Log::info(2, "Removing directory {}", dirs[a]);
	}
	Log::info(2, "Remove check took {}ms", App::runTimer() - time);

	// Go through entries
	vector<string> files_written;
	for (unsigned a = 0; a < entries.size(); a++)
	{
		// Check for folder
		auto path = entry_paths[a];
		if (entries[a]->type() == EntryType::folderType())
		{
			// Create if needed
			if (!wxDirExists(path))
				wxMkdir(path);

			// Set unmodified
			entries[a]->exProp("filePath") = path;
			entries[a]->setState(ArchiveEntry::State::Unmodified);

			continue;
		}

		// Check if entry needs to be (re)written
		if (entries[a]->state() == ArchiveEntry::State::Unmodified
			&& path == entries[a]->exProp("filePath").stringValue())
			continue;

		// Write entry to file
		if (!entries[a]->exportFile(path))
			Log::error("Unable to save entry {}: {}", entries[a]->name(), Global::error);
		else
			files_written.push_back(path);

		// Set unmodified
		entries[a]->setState(ArchiveEntry::State::Unmodified);
		entries[a]->exProp("filePath")       = path;
		file_modification_times_[entries[a]] = wxFileModificationTime(path);
	}

	removed_files_.clear();
	setModified(false);

	return true;
}

// -----------------------------------------------------------------------------
// Loads an entry's data from the saved copy of the archive if any
// -----------------------------------------------------------------------------
bool DirArchive::loadEntryData(ArchiveEntry* entry)
{
	if (entry->importFile(entry->exProp("filePath").stringValue()))
	{
		file_modification_times_[entry] = wxFileModificationTime(entry->exProp("filePath").stringValue());
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
// Deletes the directory matching [path], starting from [base]. If [base] is
// null, the root directory is used.
// Returns false if the directory does not exist, true otherwise
//
// For DirArchive also adds all subdirs and entries to the removed files list,
// so they are ignored when checking for changes on disk
// -----------------------------------------------------------------------------
shared_ptr<ArchiveDir> DirArchive::removeDir(string_view path, ArchiveDir* base)
{
	// Abort if read only
	if (read_only_)
		return nullptr;

	// Get the dir to remove
	auto dir = dirAtPath(path, base);

	// Check it exists (and that it isn't the root dir)
	if (!dir || dir == rootDir().get())
		return nullptr;

	// Get all entries in the directory (and subdirectories)
	vector<ArchiveEntry*> entries;
	putEntryTreeAsList(entries, dir);

	// Add to removed files list
	for (auto& entry : entries)
	{
		Log::info(2, entry->exProp("filePath").stringValue());
		removed_files_.push_back(entry->exProp("filePath").stringValue());
	}

	// Do normal dir remove
	return Archive::removeDir(path, base);
}

// -----------------------------------------------------------------------------
// Renames [dir] to [new_name].
// Returns false if [dir] isn't part of the archive, true otherwise
// -----------------------------------------------------------------------------
bool DirArchive::renameDir(ArchiveDir* dir, string_view new_name)
{
	auto path = dir->parent()->path();
	if (separator_ != '/')
		std::replace(path.begin(), path.end(), '/', separator_);
	StringPair rename(path + dir->name(), fmt::format("{}{}", path, new_name));
	renamed_dirs_.push_back(rename);
	Log::info(2, "RENAME {} to {}", rename.first, rename.second);

	return Archive::renameDir(dir, new_name);
}

// -----------------------------------------------------------------------------
// Adds [entry] to the end of the namespace matching [add_namespace]. If [copy]
// is true a copy of the entry is added.
// Returns the added entry or NULL if the entry is invalid
//
// Namespaces in a folder are treated the same way as a zip archive
// -----------------------------------------------------------------------------
shared_ptr<ArchiveEntry> DirArchive::addEntry(shared_ptr<ArchiveEntry> entry, string_view add_namespace)
{
	// Check namespace
	if (add_namespace.empty() || add_namespace == "global")
		return Archive::addEntry(entry, 0xFFFFFFFF, nullptr);

	// Get/Create namespace dir
	auto dir = createDir(StrUtil::lower(add_namespace));

	// Add the entry to the dir
	return Archive::addEntry(entry, 0xFFFFFFFF, dir.get());
}

// -----------------------------------------------------------------------------
// Removes [entry] from the archive.
// If [delete_entry] is true, the entry will also be deleted.
// Returns true if the removal succeeded
// -----------------------------------------------------------------------------
bool DirArchive::removeEntry(ArchiveEntry* entry)
{
	auto old_name = entry->exProp("filePath").stringValue();
	bool success  = Archive::removeEntry(entry);
	if (success)
		removed_files_.push_back(old_name);
	return success;
}

// -----------------------------------------------------------------------------
// Renames [entry].  Returns true if the rename succeeded
// -----------------------------------------------------------------------------
bool DirArchive::renameEntry(ArchiveEntry* entry, string_view name)
{
	// Check rename won't result in duplicated name
	if (entry->parentDir()->entry(name))
	{
		Global::error = fmt::format("An entry named {} already exists", name);
		return false;
	}

	auto old_name = entry->exProp("filePath").stringValue();
	bool success  = Archive::renameEntry(entry, name);
	if (success)
		removed_files_.push_back(old_name);
	return success;
}

// -----------------------------------------------------------------------------
// Returns the mapdesc_t information about the map at [entry], if [entry] is
// actually a valid map (ie. a wad archive in the maps folder)
// -----------------------------------------------------------------------------
Archive::MapDesc DirArchive::mapDesc(ArchiveEntry* entry)
{
	MapDesc map;

	// Check entry
	if (!checkEntry(entry))
		return map;

	// Check entry type
	if (entry->type()->formatId() != "archive_wad")
		return map;

	// Check entry directory
	if (entry->parentDir()->parent() != rootDir() || entry->parentDir()->name() != "maps")
		return map;

	// Setup map info
	map.archive = true;
	map.head    = entry->getShared();
	map.end     = entry->getShared();
	map.name    = entry->upperNameNoExt();

	return map;
}

// -----------------------------------------------------------------------------
// Detects all the maps in the archive and returns a vector of information about
// them.
// -----------------------------------------------------------------------------
vector<Archive::MapDesc> DirArchive::detectMaps()
{
	vector<MapDesc> ret;

	// Get the maps directory
	auto mapdir = dirAtPath("maps");
	if (!mapdir)
		return ret;

	// Go through entries in map dir
	for (unsigned a = 0; a < mapdir->numEntries(); a++)
	{
		auto entry = mapdir->sharedEntryAt(a);

		// Maps can only be wad archives
		if (entry->type()->formatId() != "archive_wad")
			continue;

		// Detect map format (probably kinda slow but whatever, no better way to do it really)
		auto       format = MapFormat::Unknown;
		WadArchive tempwad;
		tempwad.open(entry->data());
		auto emaps = tempwad.detectMaps();
		if (!emaps.empty())
			format = emaps[0].format;

		// Add map description
		MapDesc md;
		md.head    = entry;
		md.end     = entry;
		md.archive = true;
		md.name    = entry->upperNameNoExt();
		md.format  = format;
		ret.push_back(md);
	}

	return ret;
}

// -----------------------------------------------------------------------------
// Returns the first entry matching the search criteria in [options], or null if
// no matching entry was found
// -----------------------------------------------------------------------------
ArchiveEntry* DirArchive::findFirst(SearchOptions& options)
{
	// Init search variables
	auto dir = rootDir().get();

	// Check for search directory (overrides namespace)
	if (options.dir)
	{
		dir = options.dir;
	}
	// Check for namespace
	else if (!options.match_namespace.empty())
	{
		dir = dirAtPath(options.match_namespace);

		// If the requested namespace doesn't exist, return nothing
		if (!dir)
			return nullptr;
		else
			options.search_subdirs = true; // Namespace search always includes namespace subdirs
	}

	// Do default search
	auto opt            = options;
	opt.dir             = dir;
	opt.match_namespace = "";
	return Archive::findFirst(opt);
}

// -----------------------------------------------------------------------------
// Returns the last entry matching the search criteria in [options], or null if
// no matching entry was found
// -----------------------------------------------------------------------------
ArchiveEntry* DirArchive::findLast(SearchOptions& options)
{
	// Init search variables
	auto dir = rootDir().get();

	// Check for search directory (overrides namespace)
	if (options.dir)
	{
		dir = options.dir;
	}
	// Check for namespace
	else if (!options.match_namespace.empty())
	{
		dir = dirAtPath(options.match_namespace);

		// If the requested namespace doesn't exist, return nothing
		if (!dir)
			return nullptr;
		else
			options.search_subdirs = true; // Namespace search always includes namespace subdirs
	}

	// Do default search
	auto opt            = options;
	opt.dir             = dir;
	opt.match_namespace = "";
	return Archive::findLast(opt);
}

// -----------------------------------------------------------------------------
// Returns all entries matching the search criteria in [options]
// -----------------------------------------------------------------------------
vector<ArchiveEntry*> DirArchive::findAll(SearchOptions& options)
{
	// Init search variables
	auto dir = rootDir().get();

	// Check for search directory (overrides namespace)
	if (options.dir)
	{
		dir = options.dir;
	}
	// Check for namespace
	else if (!options.match_namespace.empty())
	{
		dir = dirAtPath(options.match_namespace);

		// If the requested namespace doesn't exist, return nothing
		if (!dir)
			return {};
		else
			options.search_subdirs = true; // Namespace search always includes namespace subdirs
	}

	// Do default search
	auto opt            = options;
	opt.dir             = dir;
	opt.match_namespace = "";
	return Archive::findAll(opt);
}

// -----------------------------------------------------------------------------
// Remember to ignore the given files until they change again
// -----------------------------------------------------------------------------
void DirArchive::ignoreChangedEntries(vector<DirEntryChange>& changes)
{
	for (auto& change : changes)
		ignored_file_changes_[change.file_path] = change;
}

// -----------------------------------------------------------------------------
// Updates entries/directories based on [changes] list
// -----------------------------------------------------------------------------
void DirArchive::updateChangedEntries(vector<DirEntryChange>& changes)
{
	bool was_modified = isModified();

	for (auto& change : changes)
	{
		ignored_file_changes_.erase(change.file_path);

		// Modified Entries
		if (change.action == DirEntryChange::Action::Updated)
		{
			auto entry = entryAtPath(change.entry_path);
			entry->importFile(change.file_path);
			EntryType::detectEntryType(*entry);
			file_modification_times_[entry] = wxFileModificationTime(change.file_path);
		}

		// Deleted Entries
		else if (change.action == DirEntryChange::Action::DeletedFile)
		{
			auto entry = entryAtPath(change.entry_path);
			// If the parent directory was already removed, this entry no longer exists
			if (entry)
				removeEntry(entry);
		}

		// Deleted Directories
		else if (change.action == DirEntryChange::Action::DeletedDir)
			removeDir(change.entry_path);

		// New Directory
		else if (change.action == DirEntryChange::Action::AddedDir)
		{
			auto name = change.file_path;
			name.erase(0, filename_.size());
			StrUtil::removePrefixIP(name, separator_);
			std::replace(name.begin(), name.end(), '\\', '/');

			auto ndir = createDir(name);
			ndir->dirEntry()->setState(ArchiveEntry::State::Unmodified);
			ndir->dirEntry()->exProp("filePath") = change.file_path;
		}

		// New Entry
		else if (change.action == DirEntryChange::Action::AddedFile)
		{
			auto name = change.file_path;
			name.erase(0, filename_.size());
			if (StrUtil::startsWith(name, separator_))
				name.erase(0, 1);
			std::replace(name.begin(), name.end(), '\\', '/');

			// Create entry
			StrUtil::Path fn(name);
			auto          new_entry = std::make_shared<ArchiveEntry>(fn.fileName());

			// Setup entry info
			new_entry->setLoaded(false);
			new_entry->exProp("filePath") = change.file_path;

			// Add entry and directory to directory tree
			auto ndir = createDir(fn.path());
			ndir->addEntry(new_entry);

			// Read entry data
			new_entry->importFile(change.file_path);
			new_entry->setLoaded(true);

			file_modification_times_[new_entry.get()] = FileUtil::fileModifiedTime(change.file_path);

			// Detect entry type
			EntryType::detectEntryType(*new_entry);

			// Unload data if needed
			if (!archive_load_data)
				new_entry->unloadData();

			// Set entry not modified
			new_entry->setState(ArchiveEntry::State::Unmodified);
		}
	}

	// Preserve old modified state
	setModified(was_modified);
}

// -----------------------------------------------------------------------------
// Returns true iff the user has previously indicated no interest in this change
// -----------------------------------------------------------------------------
bool DirArchive::shouldIgnoreEntryChange(DirEntryChange& change)
{
	auto it = ignored_file_changes_.find(change.file_path);
	// If we've never seen this file before, definitely don't ignore the change
	if (it == ignored_file_changes_.end())
		return false;

	auto old_change = it->second;
	bool was_deleted =
		(old_change.action == DirEntryChange::Action::DeletedFile
		 || old_change.action == DirEntryChange::Action::DeletedDir);
	bool is_deleted =
		(change.action == DirEntryChange::Action::DeletedFile || change.action == DirEntryChange::Action::DeletedDir);

	// Was deleted, is still deleted, nothing's changed
	if (was_deleted && is_deleted)
		return true;

	// Went from deleted to not, or vice versa; interesting
	if (was_deleted != is_deleted)
		return false;

	// Otherwise, it was modified both times, which is only interesting if the
	// mtime is different.  (You might think it's interesting if the mtime is
	// /greater/, but this is more robust against changes to the system clock,
	// and an unmodified file will never change mtime.)
	return (old_change.mtime == change.mtime);
}
