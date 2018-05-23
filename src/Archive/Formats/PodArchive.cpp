
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    PodArchive.cpp
// Description: PodArchive, archive class to handle the Terminal Velocity /
//              Fury3 POD archive format
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
#include "General/Console/Console.h"
#include "General/UI.h"
#include "MainEditor/MainEditor.h"
#include "PodArchive.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, archive_load_data)
EXTERN_CVAR(Bool, wad_force_uppercase)


// -----------------------------------------------------------------------------
//
// PodArchive Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// PodArchive class constructor
// -----------------------------------------------------------------------------
PodArchive::PodArchive() : Archive("pod")
{
	// Blank id
	memset(id_, 0, 80);
}

// -----------------------------------------------------------------------------
// Sets the description/id of the pod archive
// -----------------------------------------------------------------------------
void PodArchive::setId(string_view id)
{
	memset(id_, 0, 80);
	memcpy(id_, id.data(), id.size() > 80 ? 80 : id.size());
}

// -----------------------------------------------------------------------------
// Reads pod format data from a MemChunk
// Returns true if successful, false otherwise
// -----------------------------------------------------------------------------
bool PodArchive::open(MemChunk& mc)
{
	// Check data was given
	if (!mc.hasData())
		return false;

	// Read no. of files
	mc.seek(0, 0);
	uint32_t num_files;
	mc.read(&num_files, 4);

	// Read id
	mc.read(id_, 80);

	// Read directory
	FileEntry* files = new FileEntry[num_files];
	mc.read(files, num_files * sizeof(FileEntry));

	// Stop announcements (don't want to be announcing modification due to entries being added etc)
	setMuted(true);

	// Create entries
	UI::setSplashProgressMessage("Reading pod archive data");
	for (unsigned a = 0; a < num_files; a++)
	{
		// Create entry
		ArchiveEntry* new_entry     = new ArchiveEntry(StrUtil::Path::fileNameOf(files[a].name), files[a].size);
		new_entry->exProp("Offset") = files[a].offset;
		new_entry->setLoaded(false);

		// Add entry and directory to directory tree
		auto             path = StrUtil::Path::pathOf(files[a].name, false);
		ArchiveTreeNode* ndir = createDir(path);
		ndir->addEntry(new_entry);

		new_entry->setState(0);

		Log::info(5, fmt::format("File size: {}, offset: {}, name: {}", files[a].size, files[a].offset, files[a].name));
	}

	// Detect entry types
	vector<ArchiveEntry*> all_entries;
	getEntryTreeAsList(all_entries);
	UI::setSplashProgressMessage("Detecting entry types");
	for (unsigned a = 0; a < all_entries.size(); a++)
	{
		// Skip dir/marker
		if (all_entries[a]->size() == 0 || all_entries[a]->type() == EntryType::folderType())
		{
			all_entries[a]->setState(0);
			continue;
		}

		// Update splash window progress
		UI::setSplashProgress((((float)a / (float)all_entries.size())));

		// Read data
		MemChunk edata;
		mc.exportMemChunk(edata, all_entries[a]->exProp("Offset").intValue(), all_entries[a]->size());
		all_entries[a]->importMemChunk(edata);

		// Detect entry type
		EntryType::detectEntryType(all_entries[a]);

		// Unload entry data if needed
		if (!archive_load_data)
			all_entries[a]->unloadData();

		// Set entry to unchanged
		all_entries[a]->setState(0);
		Log::info(5, fmt::format("entry {} size {}", all_entries[a]->name(), all_entries[a]->size()));
	}

	// Clean up
	delete[] files;

	// Setup variables
	setMuted(false);
	setModified(false);
	announce("opened");

	UI::setSplashProgressMessage("");

	return true;
}

// -----------------------------------------------------------------------------
// Writes the pod archive to a MemChunk
// Returns true if successful, false otherwise
// -----------------------------------------------------------------------------
bool PodArchive::write(MemChunk& mc, bool update)
{
	// Get all entries
	vector<ArchiveEntry*> entries;
	getEntryTreeAsList(entries);

	// Process entries
	int      ndirs     = 0;
	uint32_t data_size = 0;
	for (auto& entry : entries)
	{
		if (entry->type() == EntryType::folderType())
			ndirs++;
		else
			data_size += entry->size();
	}

	// Init MemChunk
	mc.clear();
	mc.reSize(4 + 80 + (entries.size() * 40) + data_size, false);
	Log::info(5, fmt::format("MC size {}", mc.size()));

	// Write no. entries
	uint32_t n_entries = entries.size() - ndirs;
	Log::info(5, fmt::format("n_entries {}", n_entries));
	mc.write(&n_entries, 4);

	// Write id
	Log::info(5, fmt::format("id {}", id_));
	mc.write(id_, 80);

	// Write directory
	FileEntry fe{};
	fe.offset = 4 + 80 + (n_entries * 40);
	for (auto& entry : entries)
	{
		if (entry->type() == EntryType::folderType())
			continue;

		// Name
		memset(fe.name, 0, 32);
		string path = entry->path(true);
		std::replace(path.begin(), path.end(), '/', '\\');
		path = StrUtil::afterFirst(path, '\\');
		memcpy(fe.name, path.c_str(), path.size());

		// Size
		fe.size = entry->size();

		// Write directory entry
		mc.write(fe.name, 32);
		mc.write(&fe.size, 4);
		mc.write(&fe.offset, 4);
		Log::info(
			5,
			fmt::format(
				"entry {}: old={} new={} size={}",
				fe.name,
				entry->exProp("Offset").intValue(),
				fe.offset,
				entry->size()));

		// Next offset
		fe.offset += fe.size;
	}

	// Write entry data
	for (auto& entry : entries)
		if (entry->type() != EntryType::folderType())
			mc.write(entry->dataRaw(), entry->size());

	return true;
}

// -----------------------------------------------------------------------------
// Loads an entry's data from the archive file
// Returns true if successful, false otherwise
// -----------------------------------------------------------------------------
bool PodArchive::loadEntryData(ArchiveEntry* entry)
{
	// Check the entry is valid and part of this archive
	if (!checkEntry(entry))
		return false;

	// Do nothing if the lump's size is zero,
	// or if it has already been loaded
	if (entry->size() == 0 || entry->isLoaded())
	{
		entry->setLoaded();
		return true;
	}

	// Open file
	wxFile file(filename_);

	// Check if opening the file failed
	if (!file.IsOpened())
	{
		Log::info(fmt::format("PodArchive::loadEntryData: Failed to open file {}", filename_));
		return false;
	}

	// Seek to lump offset in file and read it in
	file.Seek((int)entry->exProp("Offset"), wxFromStart);
	entry->importFileStream(file, entry->size());

	// Set the lump to loaded
	entry->setLoaded();

	return true;
}


// -----------------------------------------------------------------------------
//
// PodArchive Class Static Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Checks if the given data is a valid pod archive
// -----------------------------------------------------------------------------
bool PodArchive::isPodArchive(MemChunk& mc)
{
	// Check size for header
	if (mc.size() < 84)
		return false;

	// Read no. of files
	mc.seek(0, 0);
	uint32_t num_files;
	mc.read(&num_files, 4);

	// Read id
	char id[80];
	mc.read(id, 80);

	// Check size for directory
	if (mc.size() < 84 + (num_files * 40))
		return false;

	// Read directory and check offsets
	FileEntry entry{};
	for (unsigned a = 0; a < num_files; a++)
	{
		mc.read(&entry, 40);
		if (entry.offset + entry.size > mc.size())
			return false;
	}
	return true;
}

// -----------------------------------------------------------------------------
// Checks if the file at [filename] is a valid pod archive
// -----------------------------------------------------------------------------
bool PodArchive::isPodArchive(string_view filename)
{
	wxFile file;
	if (!file.Open(filename.to_string()))
		return false;

	file.SeekEnd(0);
	uint32_t file_size = file.Tell();

	// Check size for header
	if (file_size < 84)
	{
		file.Close();
		return false;
	}

	// Read no. of files
	file.Seek(0);
	uint32_t num_files;
	file.Read(&num_files, 4);

	// Read id
	char id[80];
	file.Read(id, 80);

	// Check size for directory
	if (file_size < 84 + (num_files * 40))
	{
		file.Close();
		return false;
	}

	// Read directory and check offsets
	FileEntry entry{};
	for (unsigned a = 0; a < num_files; a++)
	{
		file.Read(&entry, 40);
		if (entry.offset + entry.size > file_size)
			return false;
	}
	return true;
}


// -----------------------------------------------------------------------------
//
// Console Commands
//
// -----------------------------------------------------------------------------

CONSOLE_COMMAND(pod_get_id, 0, 1)
{
	Archive* archive = MainEditor::currentArchive();
	if (archive && archive->formatId() == "pod")
		Log::console(((PodArchive*)archive)->id().to_string());
	else
		Log::console("Current tab is not a POD archive");
}

CONSOLE_COMMAND(pod_set_id, 1, true)
{
	Archive* archive = MainEditor::currentArchive();
	if (archive && archive->formatId() == "pod")
		((PodArchive*)archive)->setId(args[0].substr(0, 80));
	else
		Log::console("Current tab is not a POD archive");
}
