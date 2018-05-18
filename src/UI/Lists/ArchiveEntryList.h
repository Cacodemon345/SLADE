#pragma once

#include "Archive/Archive.h"
#include "General/ListenerAnnouncer.h"
#include "General/SAction.h"
#include "VirtualListView.h"

wxDECLARE_EVENT(EVT_AEL_DIR_CHANGED, wxCommandEvent);

class UndoManager;
class ArchiveEntryList : public VirtualListView, public Listener, public SActionHandler
{
public:
	ArchiveEntryList(wxWindow* parent);
	~ArchiveEntryList() = default;

	ArchiveTreeNode* currentDir() const { return current_dir_; }

	bool showDirBack() const { return show_dir_back_; }
	void showDirBack(bool db) { show_dir_back_ = db; }

	void setArchive(Archive* archive);
	void setUndoManager(UndoManager* manager) { undo_manager_ = manager; }

	void setupColumns();
	int  columnType(int column) const;
	void updateList(bool clear = false) override;
	int  entriesBegin() const;

	void filterList(string_view filter = "", string_view category = "");
	void applyFilter() override;
	bool goUpDir();
	bool setDir(ArchiveTreeNode* dir);

	void setEntriesAutoUpdate(bool update) { entries_update_ = update; }

	// Sorting
	void sortItems() override;

	ArchiveEntry*            entryAt(int index, bool filtered = true) const;
	int                      entryIndexAt(int index, bool filtered = true);
	ArchiveEntry*            focusedEntry() const;
	vector<ArchiveEntry*>    selectedEntries() const;
	ArchiveEntry*            lastSelectedEntry() const;
	vector<ArchiveTreeNode*> selectedDirectories() const;

	// Label editing
	void labelEdited(int col, int index, const wxString& new_label) override;

	void onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data) override;

	// SAction handler
	bool handleAction(string_view id) override;

	// Events
	void onColumnHeaderRightClick(wxListEvent& e);
	void onColumnResize(wxListEvent& e);
	void onListItemActivated(wxListEvent& e);

protected:
	// Virtual wxListCtrl overrides
	string getItemText(long item, long column, long index) const override;
	int    getItemIcon(long item, long column, long index) const override;
	void   updateItemAttr(long item, long column, long index) const override;

private:
	Archive*           archive_ = nullptr;
	string             filter_category_;
	ArchiveTreeNode*   current_dir_ = nullptr;
	ArchiveEntry::UPtr entry_dir_back_;
	bool               show_dir_back_  = false;
	UndoManager*       undo_manager_   = nullptr;
	int                col_index_      = 0;
	int                col_name_       = 1;
	int                col_size_       = 2;
	int                col_type_       = 3;
	bool               entries_update_ = true;

	int entrySize(long index) const;
};
