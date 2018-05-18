
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    ArchiveEntryList.cpp
// Description: A list widget that shows all entries in an archive.
//              Keeps in sync with its associated archive automatically.
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
#include "ArchiveEntryList.h"
#include "General/ColourConfiguration.h"
#include "General/UndoRedo.h"
#include "Graphics/Icons.h"
#include "UI/WxUtils.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(Int, elist_colname_width, 80, CVAR_SAVE)
CVAR(Int, elist_colsize_width, 64, CVAR_SAVE)
CVAR(Int, elist_coltype_width, 160, CVAR_SAVE)
CVAR(Int, elist_colindex_width, 64, CVAR_SAVE)
CVAR(Bool, elist_colsize_show, true, CVAR_SAVE)
CVAR(Bool, elist_coltype_show, true, CVAR_SAVE)
CVAR(Bool, elist_colindex_show, false, CVAR_SAVE)
CVAR(Bool, elist_hrules, false, CVAR_SAVE)
CVAR(Bool, elist_vrules, false, CVAR_SAVE)
CVAR(Bool, elist_filter_dirs, false, CVAR_SAVE)
CVAR(Bool, elist_type_bgcol, false, CVAR_SAVE)
CVAR(Float, elist_type_bgcol_intensity, 0.18, CVAR_SAVE)
CVAR(Bool, elist_name_monospace, false, CVAR_SAVE)
CVAR(Bool, elist_alt_row_colour, false, CVAR_SAVE)
wxDEFINE_EVENT(EVT_AEL_DIR_CHANGED, wxCommandEvent);


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, list_font_monospace)


// -----------------------------------------------------------------------------
//
// ArchiveEntryList Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// ArchiveEntryList class constructor
// -----------------------------------------------------------------------------
ArchiveEntryList::ArchiveEntryList(wxWindow* parent) : VirtualListView(parent)
{
	// Create dummy 'up folder' entry
	entry_dir_back_ = std::make_unique<ArchiveEntry>();
	entry_dir_back_->setType(EntryType::folderType());
	entry_dir_back_->setState(0);
	entry_dir_back_->setName("..");

	// Setup columns
	setupColumns();

	// Setup entry icons
	auto image_list   = WxUtils::createSmallImageList();
	auto et_icon_list = EntryType::iconList();
	for (const auto& icon : et_icon_list)
	{
		if (image_list->Add(Icons::getIcon(Icons::Entry, icon)) < 0)
			image_list->Add(Icons::getIcon(Icons::Entry, "default"));
	}

	wxListCtrl::SetImageList(image_list, wxIMAGE_LIST_SMALL);

	// Bind events
	Bind(wxEVT_LIST_COL_RIGHT_CLICK, &ArchiveEntryList::onColumnHeaderRightClick, this);
	Bind(wxEVT_LIST_COL_END_DRAG, &ArchiveEntryList::onColumnResize, this);
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &ArchiveEntryList::onListItemActivated, this);

	// Setup flags
	SetSingleStyle(wxLC_HRULES, elist_hrules);
	SetSingleStyle(wxLC_VRULES, elist_vrules);
}

// -----------------------------------------------------------------------------
// Called when the widget requests the text for [item] at [column]
// -----------------------------------------------------------------------------
string ArchiveEntryList::getItemText(long item, long column, long index) const
{
	// Get entry
	ArchiveEntry* entry = entryAt(index, false);

	// Check entry
	if (!entry)
		return "INVALID INDEX";

	// Determine what column we want
	int col = columnType(column);

	if (col == 0)
		return entry->name(); // Name column
	else if (col == 1)
	{
		// Size column
		if (entry->type() == EntryType::folderType())
		{
			// Entry is a folder, return the number of entries+subdirectories in it
			ArchiveTreeNode* dir;

			// Get selected directory
			if (entry == entry_dir_back_.get())
				dir = (ArchiveTreeNode*)
						  current_dir_->parent(); // If it's the 'back directory', get the current dir's parent
			else
				dir = archive_->getDir(entry->name(), current_dir_);

			// If it's null, return error
			if (!dir)
				return "INVALID DIRECTORY";

			// Return the number of items in the directory
			return S_FMT("%d entries", dir->numEntries() + dir->nChildren());
		}
		else
			return entry->sizeString(); // Not a folder, just return the normal size string
	}
	else if (col == 2)
		return entry->typeString(); // Type column
	else if (col == 3)
	{
		// Index column
		if (entry->type() == EntryType::folderType())
			return "";
		else
			return S_FMT("%d", entry->parentDir()->entryIndex(entry));
	}
	else
		return "INVALID COLUMN"; // Invalid column
}

// -----------------------------------------------------------------------------
// Called when the widget requests the icon for [item]
// -----------------------------------------------------------------------------
int ArchiveEntryList::getItemIcon(long item, long column, long index) const
{
	if (column > 0)
		return -1;

	// Get associated entry
	ArchiveEntry* entry = entryAt(item);

	// If entry doesn't exist, return invalid image
	if (!entry)
		return -1;

	return entry->type()->index();
}

// -----------------------------------------------------------------------------
// Called when widget requests the attributes
// (text colour / background colour / font) for [item]
// -----------------------------------------------------------------------------
void ArchiveEntryList::updateItemAttr(long item, long column, long index) const
{
	// Get associated entry
	ArchiveEntry* entry = entryAt(item);

	// Init attributes
	wxColour col_bg = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
	item_attr_->SetTextColour(WXCOL(ColourConfiguration::colour("error")));
	item_attr_->SetBackgroundColour(col_bg);

	// If entry doesn't exist, return error colour
	if (!entry)
		return;

	// Set font
	if (elist_name_monospace && !list_font_monospace)
		item_attr_->SetFont((column == 0) ? *font_monospace_ : *font_normal_);
	else
		item_attr_->SetFont(list_font_monospace ? *font_monospace_ : *font_normal_);

	// Set background colour defined in entry type (if any)
	ColRGBA col = entry->type()->colour();
	if ((col.r != 255 || col.g != 255 || col.b != 255) && elist_type_bgcol)
	{
		ColRGBA bcol;

		bcol.r = (col.r * elist_type_bgcol_intensity) + (col_bg.Red() * (1.0 - elist_type_bgcol_intensity));
		bcol.g = (col.g * elist_type_bgcol_intensity) + (col_bg.Green() * (1.0 - elist_type_bgcol_intensity));
		bcol.b = (col.b * elist_type_bgcol_intensity) + (col_bg.Blue() * (1.0 - elist_type_bgcol_intensity));

		item_attr_->SetBackgroundColour(WXCOL(bcol));
	}

	// Alternating row colour
	if (elist_alt_row_colour && item % 2 > 0)
	{
		wxColour dark = item_attr_->GetBackgroundColour().ChangeLightness(95);
		item_attr_->SetBackgroundColour(dark);
	}

	// Set colour depending on entry state
	switch (entry->state())
	{
	case 1: item_attr_->SetTextColour(WXCOL(ColourConfiguration::colour("modified"))); break;
	case 2: item_attr_->SetTextColour(WXCOL(ColourConfiguration::colour("new"))); break;
	default: item_attr_->SetTextColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT)); break;
	};

	// Locked state overrides others
	if (entry->isLocked())
		item_attr_->SetTextColour(WXCOL(ColourConfiguration::colour("locked")));
}

// -----------------------------------------------------------------------------
// Sets the archive for this widget to handle (can be NULL for no archive)
// -----------------------------------------------------------------------------
void ArchiveEntryList::setArchive(Archive* archive)
{
	// Stop listening to current archive (if any)
	if (this->archive_)
		stopListening(this->archive_);

	// Set archive (allow null)
	this->archive_ = archive;

	// Init new archive if given
	if (archive)
	{
		// Listen to it
		listenTo(archive);

		// Open root directory
		current_dir_ = archive->rootDir();
		applyFilter();
		updateList();
	}
}

// -----------------------------------------------------------------------------
// Creates/sets the list columns depending on user options
// -----------------------------------------------------------------------------
void ArchiveEntryList::setupColumns()
{
	// Remove existing columns
	while (GetColumnCount() > 0)
		DeleteColumn(0);

	// Create columns
	int col_num = 0;
	col_index_  = -1;
	col_name_   = 0;
	col_size_   = -1;
	col_type_   = -1;

	// Index
	if (elist_colindex_show)
	{
		AppendColumn("#");
		SetColumnWidth(col_num, elist_colindex_width);
		col_index_ = col_num++;
	}

	// Name (always)
	AppendColumn("Name");
	SetColumnWidth(col_num, elist_colname_width);
	col_name_ = col_num++;

	// Size
	if (elist_colsize_show)
	{
		AppendColumn("Size");
		SetColumnWidth(col_num, elist_colsize_width);
		col_size_ = col_num++;
	}

	// Type
	if (elist_coltype_show)
	{
		AppendColumn("Type");
		SetColumnWidth(col_num, elist_coltype_width);
		col_type_ = col_num;
	}

	// Set editable
	setColumnEditable(col_name_); // Name column

	// Reset sorting
	sort_column_  = -1;
	sort_descend_ = false;
}

// -----------------------------------------------------------------------------
// Returns the 'type' of column at [column] (name, size or type)
// -----------------------------------------------------------------------------
int ArchiveEntryList::columnType(int column) const
{
	if (column == col_name_)
		return 0;
	else if (column == col_size_)
		return 1;
	else if (column == col_type_)
		return 2;
	else if (column == col_index_)
		return 3;

	return -1;
}

// -----------------------------------------------------------------------------
// Updates + refreshes the list
// -----------------------------------------------------------------------------
void ArchiveEntryList::updateList(bool clear)
{
	// If no current directory, set size to 0
	if (!current_dir_)
	{
		SetItemCount(0);
		Refresh();
		return;
	}

	// Update list
	SetItemCount(items_.size());
	sortItems();

	Refresh();
}

// -----------------------------------------------------------------------------
// Filters the list to only entries and directories with names matching
// [filter], and with type categories matching [category].
// -----------------------------------------------------------------------------
void ArchiveEntryList::filterList(string_view filter, string_view category)
{
	// Update variables
	S_SET_VIEW(filter_text_, filter);
	S_SET_VIEW(filter_category_, category);

	// Save current selection
	vector<ArchiveEntry*> selection = selectedEntries();
	ArchiveEntry*         focus     = focusedEntry();

	// Apply the filter
	clearSelection();
	applyFilter();

	// Restore selection (if selected entries aren't filtered)
	ArchiveEntry* entry;
	for (int a = 0; a < GetItemCount(); a++)
	{
		entry = entryAt(a, false);
		for (auto& sel_entry : selection)
		{
			if (entry == sel_entry)
			{
				selectItem(a);
				break;
			}
		}

		if (entry == focus)
		{
			focusItem(a);
			EnsureVisible(a);
		}
	}
}

// -----------------------------------------------------------------------------
// Applies the current filter(s) to the list
// -----------------------------------------------------------------------------
void ArchiveEntryList::applyFilter()
{
	// Clear current filter list
	items_.clear();

	// Check if any filters were given
	if (filter_text_.empty() && filter_category_.empty())
	{
		// No filter, just refresh the list
		unsigned count = current_dir_->numEntries() + current_dir_->nChildren();
		for (unsigned a = 0; a < count; a++)
			items_.push_back(a);
		updateList();

		return;
	}

	// Filter by category
	unsigned      index = 0;
	ArchiveEntry* entry = entryAt(index, false);
	while (entry)
	{
		if (filter_category_.empty() || entry->type() == EntryType::folderType())
			items_.push_back(index); // If no category specified, just add all entries to the filter
		else
		{
			// Check for category match
			if (StrUtil::equalCI(entry->type()->category(), filter_category_))
				items_.push_back(index);
		}

		entry = entryAt(++index, false);
	}

	// Now filter by name if needed
	if (!filter_text_.empty())
	{
		// Split filter by ,
		auto terms = StrUtil::split(filter_text_, ',');

		// Process filter strings
		for (auto& term : terms)
		{
			// Remove spaces
			StrUtil::replaceIP(term, StrUtil::SPACE, StrUtil::EMPTY);

			// Set to lowercase and add * to the end
			if (!term.empty())
				term.push_back('*');
		}

		// Go through filtered list
		for (unsigned a = 0; a < items_.size(); a++)
		{
			entry = entryAt(items_[a], false);

			// Don't filter folders if !elist_filter_dirs
			if (!elist_filter_dirs && entry->type() == EntryType::folderType())
				continue;

			// Check for name match with filter
			bool match = false;
			for (const auto& term : terms)
			{
				if (entry == entry_dir_back_.get() || StrUtil::matchesCI(entry->name(), term))
				{
					match = true;
					break;
				}
			}
			if (match)
				continue;

			// No match, remove from filtered list
			items_.erase(items_.begin() + a);
			a--;
		}
	}

	// Update the list
	updateList();
}

// -----------------------------------------------------------------------------
// Opens the given directory (if it exists)
// -----------------------------------------------------------------------------
bool ArchiveEntryList::setDir(ArchiveTreeNode* dir)
{
	// If it doesn't exist, do nothing
	if (!dir)
		return false;

	// Set current dir
	current_dir_ = dir;

	// Clear current selection
	clearSelection();

	// Update filter
	applyFilter();

	// Update list
	updateList();

	// Fire event
	wxCommandEvent evt(EVT_AEL_DIR_CHANGED, GetId());
	ProcessWindowEvent(evt);

	return true;
}

// -----------------------------------------------------------------------------
// Opens the parent directory of the current directory (if it exists)
// -----------------------------------------------------------------------------
bool ArchiveEntryList::goUpDir()
{
	// Get parent directory
	return (setDir((ArchiveTreeNode*)current_dir_->parent()));
}

// -----------------------------------------------------------------------------
// Returns either the size of the entry at [index], or if it is a folder, the
// number of entries+subfolders within it
// -----------------------------------------------------------------------------
int ArchiveEntryList::entrySize(long index) const
{
	ArchiveEntry* entry = entryAt(index, false);
	if (entry->type() == EntryType::folderType())
	{
		ArchiveTreeNode* dir = archive_->getDir(entry->name(), current_dir_);
		if (dir)
			return dir->numEntries() + dir->nChildren();
		else
			return 0;
	}
	else
		return entry->size();
}

// -----------------------------------------------------------------------------
// Sorts the list items depending on the current sorting column
// -----------------------------------------------------------------------------
void ArchiveEntryList::sortItems()
{
	lv_current_ = this;
	std::sort(items_.begin(), items_.end(), [&](long left, long right) {
		auto le = entryAt(left, false);
		auto re = entryAt(right, false);

		// Sort folder->entry first
		if (le->type() == EntryType::folderType() && re->type() != EntryType::folderType())
			return true;
		if (re->type() == EntryType::folderType() && le->type() != EntryType::folderType())
			return false;

		// Name sort
		if (col_name_ >= 0 && col_name_ == sortColumn())
			return sort_descend_ ? le->name() > re->name() : le->name() < re->name();

		// Size sort
		if (col_size_ >= 0 && col_size_ == sortColumn())
			return sort_descend_ ? entrySize(left) > entrySize(right) : entrySize(left) < entrySize(right);

		// Index sort
		if (col_index_ >= 0 && col_index_ == sortColumn())
			return sort_descend_ ? left > right : left < right;

		// Other (default) sort
		return VirtualListView::defaultSort(left, right);
	});
}

// -----------------------------------------------------------------------------
// Returns the index of the first list item that is an entry (rather than a
// directory), or -1 if no directory/archive is open)
// -----------------------------------------------------------------------------
int ArchiveEntryList::entriesBegin() const
{
	// Check directory is open
	if (!current_dir_)
		return -1;

	// Determine first entry index
	int index = 0;
	if (show_dir_back_ && current_dir_->parent()) // Offset if '..' item exists
		index++;
	index += current_dir_->nChildren(); // Offset by number of subdirs

	return index;
}

// -----------------------------------------------------------------------------
// Returns the ArchiveEntry associated with the list item at [index].
// Returns NULL if the index is out of bounds or no archive is open
// -----------------------------------------------------------------------------
ArchiveEntry* ArchiveEntryList::entryAt(int index, bool filtered) const
{
	// Check index & archive
	if (index < 0 || !archive_)
		return nullptr;

	// Modify index for filtered list
	if (filtered)
	{
		if ((unsigned)index >= items_.size())
			return nullptr;
		else
			index = items_[index];
	}

	// Index modifier if 'up folder' entry exists
	if (show_dir_back_ && current_dir_->parent())
	{
		if (index == 0)
			return entry_dir_back_.get();
		else
			index--;
	}

	// Subdirectories
	int subdirs = current_dir_->nChildren();
	if (index < subdirs)
		return ((ArchiveTreeNode*)(current_dir_->childAt(index)))->dirEntry();

	// Entries
	if ((unsigned)index < subdirs + current_dir_->numEntries())
		return current_dir_->entryAt(index - subdirs);

	// Out of bounds
	return nullptr;
}

// -----------------------------------------------------------------------------
// Returns the ArchiveEntry index associated with the list item at [index].
// Returns -1 if the index is out of bounds or no archive is open
// -----------------------------------------------------------------------------
int ArchiveEntryList::entryIndexAt(int index, bool filtered)
{
	// Check index & archive
	if (index < 0 || !archive_)
		return -1;

	// Modify index for filtered list
	if (filtered)
	{
		if ((unsigned)index >= items_.size())
			return -1;
		else
			index = items_[index];
	}

	// Index modifier if 'up folder' entry exists
	if (show_dir_back_ && current_dir_->parent())
	{
		if (index == 0)
			return -1;
		else
			index--;
	}

	// Entries
	int subdirs = current_dir_->nChildren();
	if ((unsigned)index < subdirs + current_dir_->numEntries())
		return index - subdirs;

	// Out of bounds or subdir
	return -1;
}

// -----------------------------------------------------------------------------
// Gets the archive entry associated with the currently focused list item.
// Returns NULL if nothing is focused or no archive is open
// -----------------------------------------------------------------------------
ArchiveEntry* ArchiveEntryList::focusedEntry() const
{
	// Get the focus index
	int focus = focusedItem();

	// Check that the focus index is valid
	if (focus < 0 || focus > GetItemCount())
		return nullptr;

	// Return the focused archive entry
	if (archive_)
		return entryAt(focus);
	else
		return nullptr;
}

// -----------------------------------------------------------------------------
// Returns a vector of all selected archive entries
// -----------------------------------------------------------------------------
vector<ArchiveEntry*> ArchiveEntryList::selectedEntries() const
{
	// Init vector
	vector<ArchiveEntry*> ret;

	// Return empty if no archive open
	if (!archive_)
		return ret;

	// Get selection
	vector<long> selection = VirtualListView::selection();

	// Go through selection and add associated entries to the return vector
	ArchiveEntry* entry = nullptr;
	for (long index : selection)
	{
		entry = entryAt(index);
		if (entry && entry->type() != EntryType::folderType())
			ret.push_back(entry);
	}

	return ret;
}

// -----------------------------------------------------------------------------
// Gets the archive entry associated with the last selected item in the list.
// Returns NULL if no item is selected
// -----------------------------------------------------------------------------
ArchiveEntry* ArchiveEntryList::lastSelectedEntry() const
{
	int index = lastSelected();

	if (index >= 0 && archive_)
		return entryAt(index);
	else
		return nullptr;
}

// -----------------------------------------------------------------------------
// Returns a vector of all currently selected directories
// -----------------------------------------------------------------------------
vector<ArchiveTreeNode*> ArchiveEntryList::selectedDirectories() const
{
	vector<ArchiveTreeNode*> ret;

	// Get all selected items
	vector<long> selection = VirtualListView::selection();

	// Go through the selection
	for (long index : selection)
	{
		ArchiveEntry* entry = entryAt(index);

		// If the selected entry is the 'back folder', ignore it
		if (entry == entry_dir_back_.get())
			continue;
		else if (entry->type() == EntryType::folderType())
		{
			// If the entry is a folder type, get its ArchiveTreeNode counterpart
			ArchiveTreeNode* dir = archive_->getDir(entry->name(), current_dir_);

			// Add it to the return list
			if (dir)
				ret.push_back(dir);
		}
	}

	return ret;
}

// -----------------------------------------------------------------------------
// Called when a label has been edited
// -----------------------------------------------------------------------------
void ArchiveEntryList::labelEdited(int col, int index, const wxString& new_label)
{
	if (undo_manager_)
		undo_manager_->beginRecord("Rename Entry");

	// Rename the entry
	ArchiveEntry* entry = entryAt(index);
	if (entry->parent())
		entry->parent()->renameEntry(entry, WxUtils::stringToView(new_label));
	else
		entry->rename(WxUtils::stringToView(new_label));

	if (undo_manager_)
		undo_manager_->endRecord(true);
}

// -----------------------------------------------------------------------------
// Called when an announcement is recieved from the archive being managed
// -----------------------------------------------------------------------------
void ArchiveEntryList::onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data)
{
	if (entries_update_ && announcer == archive_ && event_name != "closed")
	{
		// updateList();
		applyFilter();
	}
}

// -----------------------------------------------------------------------------
// Handles the action [id].
// Returns true if the action was handled, false otherwise
// -----------------------------------------------------------------------------
bool ArchiveEntryList::handleAction(string_view id)
{
	// Don't handle action if hidden
	if (!IsShown())
		return false;

	// Only interested in actions beginning with aelt_
	if (!StrUtil::startsWith(id, "aelt_"))
		return false;

	if (id == "aelt_sizecol")
	{
		elist_colsize_show = !elist_colsize_show;
		setupColumns();
		updateWidth();
		updateList();
		if (GetParent())
			GetParent()->Layout();
	}
	else if (id == "aelt_typecol")
	{
		elist_coltype_show = !elist_coltype_show;
		setupColumns();
		updateWidth();
		updateList();
		if (GetParent())
			GetParent()->Layout();
	}
	else if (id == "aelt_indexcol")
	{
		elist_colindex_show = !elist_colindex_show;
		setupColumns();
		updateWidth();
		updateList();
		if (GetParent())
			GetParent()->Layout();
	}
	else if (id == "aelt_hrules")
	{
		elist_hrules = !elist_hrules;
		SetSingleStyle(wxLC_HRULES, elist_hrules);
		Refresh();
	}
	else if (id == "aelt_vrules")
	{
		elist_vrules = !elist_vrules;
		SetSingleStyle(wxLC_VRULES, elist_vrules);
		Refresh();
	}
	else if (id == "aelt_bgcolour")
	{
		elist_type_bgcol = !elist_type_bgcol;
		Refresh();
	}
	else if (id == "aelt_bgalt")
	{
		elist_alt_row_colour = !elist_alt_row_colour;
		Refresh();
	}

	// Unknown action
	else
		return false;

	// Action handled, return true
	return true;
}


// -----------------------------------------------------------------------------
//
// ARCHIVEENTRYLIST EVENTS
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when a column header is right clicked
// -----------------------------------------------------------------------------
void ArchiveEntryList::onColumnHeaderRightClick(wxListEvent& e)
{
	// Create simple popup menu with options to toggle columns
	wxMenu popup;
	SAction::fromId("aelt_indexcol")->addToMenu(&popup, true);
	SAction::fromId("aelt_sizecol")->addToMenu(&popup, true);
	SAction::fromId("aelt_typecol")->addToMenu(&popup, true);
	SAction::fromId("aelt_hrules")->addToMenu(&popup, true);
	SAction::fromId("aelt_vrules")->addToMenu(&popup, true);
	SAction::fromId("aelt_bgcolour")->addToMenu(&popup, true);
	SAction::fromId("aelt_bgalt")->addToMenu(&popup, true);
	popup.Check(SAction::fromId("aelt_indexcol")->wxId(), elist_colindex_show);
	popup.Check(SAction::fromId("aelt_sizecol")->wxId(), elist_colsize_show);
	popup.Check(SAction::fromId("aelt_typecol")->wxId(), elist_coltype_show);
	popup.Check(SAction::fromId("aelt_hrules")->wxId(), elist_hrules);
	popup.Check(SAction::fromId("aelt_vrules")->wxId(), elist_vrules);
	popup.Check(SAction::fromId("aelt_bgcolour")->wxId(), elist_type_bgcol);
	popup.Check(SAction::fromId("aelt_bgalt")->wxId(), elist_alt_row_colour);

	// Pop it up
	PopupMenu(&popup);
}

// -----------------------------------------------------------------------------
// Called when a column is resized
// -----------------------------------------------------------------------------
void ArchiveEntryList::onColumnResize(wxListEvent& e)
{
	// Save column widths
	elist_colname_width = GetColumnWidth(col_name_);
	if (elist_colsize_show)
		elist_colsize_width = GetColumnWidth(col_size_);
	if (elist_coltype_show)
		elist_coltype_width = GetColumnWidth(col_type_);
	if (elist_colindex_show)
		elist_colindex_width = GetColumnWidth(col_index_);
	e.Skip();
}

// -----------------------------------------------------------------------------
// Called when a list item is 'activated' (double-click or enter)
// -----------------------------------------------------------------------------
void ArchiveEntryList::onListItemActivated(wxListEvent& e)
{
	// Get item entry
	ArchiveEntry* entry = entryAt(e.GetIndex());

	// Do nothing if NULL (shouldn't be)
	if (!entry)
		return;

	// If it's a folder, open it
	if (entry->type() == EntryType::folderType())
	{
		// Get directory to open
		ArchiveTreeNode* dir;
		if (entry == entry_dir_back_.get())
			dir = (ArchiveTreeNode*)current_dir_->parent(); // 'Back directory' entry, open current dir's parent
		else
			dir = archive_->getDir(entry->name(), current_dir_);

		// Check it exists (really should)
		if (!dir)
		{
			LOG_MESSAGE(1, "Error: Trying to open nonexistant directory");
			return;
		}

		// Set current dir
		setDir(dir);
	}
	else
		e.Skip();
}
