
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         https://slade.mancubus.net
// Filename:    MapBackupManager.cpp
// Description: MapBackupManager class - creates/manages map backups
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
#include "Archive/Formats/ZipArchive.h"
#include "General/Misc.h"
#include "MapBackupManager.h"
#include "MapEditor.h"
#include "UI/MapBackupPanel.h"
#include "UI/SDialog.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(Int, max_map_backups, 25, CVAR_SAVE)

namespace
{
// List of entry names to be ignored for backups
string mb_ignore_entries[] = { "NODES",    "SSECTORS", "ZNODES",  "SEGS",     "REJECT",
							   "BLOCKMAP", "GL_VERT",  "GL_SEGS", "GL_SSECT", "GL_NODES" };
} // namespace


// -----------------------------------------------------------------------------
//
// MapBackupManager Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Writes a backup for [map_name] in [archive_name], with the map data entries
// in [map_data]
// -----------------------------------------------------------------------------
bool MapBackupManager::writeBackup(vector<ArchiveEntry::UPtr>& map_data, string_view archive_name, string_view map_name)
	const
{
	// Create backup directory if needed
	string backup_dir = App::path("backups", App::Dir::User);
	if (!wxDirExists(backup_dir))
		wxMkdir(backup_dir);

	// Open or create backup zip
	ZipArchive backup;
	backup_dir += '/';
	string backup_file{ archive_name.data(), archive_name.size() };
	std::replace(backup_file.begin(), backup_file.end(), '.', '_');
	StrUtil::prependIP(backup_file, backup_dir);
	backup_file += "_backup.zip";
	if (!backup.open(backup_file))
		backup.setFilename(backup_file);

	// Filter ignored entries
	vector<ArchiveEntry*> backup_entries;
	for (auto& entry : map_data)
	{
		// Check for ignored entry
		bool ignored = false;
		for (const auto& ignore : mb_ignore_entries)
		{
			if (StrUtil::equalCI(ignore, entry->name()))
			{
				ignored = true;
				break;
			}
		}

		if (!ignored)
			backup_entries.push_back(entry.get());
	}

	// Compare with last backup (if any)
	ArchiveTreeNode* map_dir = backup.getDir(map_name);
	if (map_dir && map_dir->nChildren() > 0)
	{
		auto last_backup = (ArchiveTreeNode*)map_dir->childAt(map_dir->nChildren() - 1);
		bool same        = true;
		if (last_backup->numEntries() != backup_entries.size())
			same = false;
		else
		{
			for (unsigned a = 0; a < last_backup->numEntries(); a++)
			{
				ArchiveEntry* e1 = backup_entries[a];
				ArchiveEntry* e2 = last_backup->entryAt(a);
				if (e1->size() != e2->size())
				{
					same = false;
					break;
				}

				uint32_t crc1 = Misc::crc(e1->dataRaw(), e1->size());
				uint32_t crc2 = Misc::crc(e2->dataRaw(), e2->size());
				if (crc1 != crc2)
				{
					same = false;
					break;
				}
			}
		}

		if (same)
		{
			Log::info(2, "Same data as previous backup - ignoring");
			return true;
		}
	}

	// Add map data to backup
	string timestamp = wxDateTime::Now().FormatISOCombined('_').ToStdString();
	StrUtil::replaceIP(timestamp, ":", "");
	string dir{ map_name.data(), map_name.size() };
	dir += '/';
	dir += timestamp;
	for (auto& entry : backup_entries)
		backup.addEntry(entry, dir, true);

	// Check for max backups & remove old ones if over
	map_dir = backup.getDir(map_name);
	while ((int)map_dir->nChildren() > max_map_backups)
		backup.removeDir(map_dir->childAt(0)->name(), map_dir);

	// Save backup file
	Archive::save_backup = false;
	bool ok              = backup.save();
	Archive::save_backup = true;

	return ok;
}

// -----------------------------------------------------------------------------
// Shows the map backups for [map_name] in [archive_name], returns the selected
// map backup data in a WadArchive
// -----------------------------------------------------------------------------
Archive* MapBackupManager::openBackup(string_view archive_name, string_view map_name) const
{
	SDialog dlg(MapEditor::windowWx(), fmt::sprintf("Restore %s backup", map_name), "map_backup", 500, 400);
	auto    sizer = new wxBoxSizer(wxVERTICAL);
	dlg.SetSizer(sizer);
	auto panel_backup = new MapBackupPanel(&dlg);
	sizer->Add(panel_backup, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
	sizer->AddSpacer(4);
	sizer->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT, 6);
	sizer->AddSpacer(10);

	if (panel_backup->loadBackups(archive_name.to_string(), map_name))
	{
		if (dlg.ShowModal() == wxID_OK)
			return panel_backup->selectedMapData();
	}
	else
		wxMessageBox(
			fmt::sprintf("No backups exist for %s of %s", map_name, archive_name),
			"Restore Backup",
			wxICON_INFORMATION,
			MapEditor::windowWx());

	return nullptr;
}
