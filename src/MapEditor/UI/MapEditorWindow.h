#pragma once

#include "Archive/Archive.h"
#include "General/SAction.h"
#include "UI/STopWindow.h"

class MapObject;
class MapObjectPropsPanel;
class ScriptEditorPanel;
class ObjectEditPanel;
class ObjectEditGroup;
class WadArchive;
class MapCanvas;
class MapChecksPanel;
class UndoManagerHistoryPanel;
class UndoManager;
class ArchiveEntry;

class MapEditorWindow : public STopWindow, public SActionHandler
{
public:
	MapEditorWindow();
	~MapEditorWindow();

	// Layout save/load
	void loadLayout();
	void saveLayout();

	void        setupMenu();
	void        setupLayout();
	bool        chooseMap(Archive* archive = nullptr);
	bool        openMap(const Archive::MapDesc& map);
	void        loadMapScripts(const Archive::MapDesc& map);
	WadArchive* writeMap(string_view name = "MAP01", bool nodes = true);
	bool        saveMap();
	bool        saveMapAs();
	void        forceRefresh(bool renderer = false) const;
	void        refreshToolBar() const;
	void        setUndoManager(UndoManager* manager) const;
	bool        tryClose();
	void        reloadScriptsMenu() const;

	MapObjectPropsPanel* propsPanel() const { return panel_obj_props_; }
	ObjectEditPanel*     objectEditPanel() const { return panel_obj_edit_; }

	void showObjectEditPanel(bool show, ObjectEditGroup* group);
	void showShapeDrawPanel(bool show = true);

	// SAction handler
	bool handleAction(string_view id) override;

private:
	MapCanvas*                 map_canvas_          = nullptr;
	MapObjectPropsPanel*       panel_obj_props_     = nullptr;
	ScriptEditorPanel*         panel_script_editor_ = nullptr;
	vector<ArchiveEntry::UPtr> map_data_;
	ObjectEditPanel*           panel_obj_edit_     = nullptr;
	MapChecksPanel*            panel_checks_       = nullptr;
	UndoManagerHistoryPanel*   panel_undo_history_ = nullptr;
	wxMenu*                    menu_scripts_       = nullptr;

	void buildNodes(Archive* wad);

	// Events
	void onClose(wxCloseEvent& e);
	void onSize(wxSizeEvent& e);
};
