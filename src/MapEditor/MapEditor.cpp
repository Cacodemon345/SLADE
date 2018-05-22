
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapEditor.cpp
// Description: MapEditor namespace - general map editor functions and types,
//              mostly to interact with the map editor window without needing to
//              access it (and wx) directly
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
#include "MapBackupManager.h"
#include "MapEditContext.h"
#include "MapEditor/UI/Dialogs/MapTextureBrowser.h"
#include "MapEditor/UI/Dialogs/ThingTypeBrowser.h"
#include "MapEditor/UI/PropsPanel/LinePropsPanel.h"
#include "MapEditor/UI/PropsPanel/SectorPropsPanel.h"
#include "MapEditor/UI/PropsPanel/ThingPropsPanel.h"
#include "MapTextureManager.h"
#include "UI/MapCanvas.h"
#include "UI/MapEditorWindow.h"
#include "UI/PropsPanel/MapObjectPropsPanel.h"
#include "UI/SDialog.h"
#include "UI/WxUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace MapEditor
{
std::unique_ptr<MapEditContext> edit_context;
MapTextureManager               texture_manager;
Archive::MapDesc                current_map_desc;
MapEditorWindow*                map_window;
MapBackupManager                backup_manager;
} // namespace MapEditor


// -----------------------------------------------------------------------------
//
// MapEditor Namespace Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the current map editor context
// -----------------------------------------------------------------------------
MapEditContext& MapEditor::editContext()
{
	if (!edit_context)
		edit_context = std::make_unique<MapEditContext>();

	return *edit_context;
}

// -----------------------------------------------------------------------------
// Returns the map editor texture manager
// -----------------------------------------------------------------------------
MapTextureManager& MapEditor::textureManager()
{
	return texture_manager;
}

// -----------------------------------------------------------------------------
// Returns the map editor window
// -----------------------------------------------------------------------------
MapEditorWindow* MapEditor::window()
{
	if (!map_window)
		init();

	return map_window;
}

// -----------------------------------------------------------------------------
// Returns the map editor window (as a wxWindow)
// -----------------------------------------------------------------------------
wxWindow* MapEditor::windowWx()
{
	if (!map_window)
		init();

	return map_window;
}

// -----------------------------------------------------------------------------
// Returns the map editor backup manager
// -----------------------------------------------------------------------------
MapBackupManager& MapEditor::backupManager()
{
	return backup_manager;
}

// -----------------------------------------------------------------------------
// Initialises the map editor
// -----------------------------------------------------------------------------
void MapEditor::init()
{
	map_window = new MapEditorWindow();
	texture_manager.init();
}

// -----------------------------------------------------------------------------
// Forces a refresh of the map editor window (and the renderer if [renderer] is
// true)
// -----------------------------------------------------------------------------
void MapEditor::forceRefresh(bool renderer)
{
	if (map_window)
		map_window->forceRefresh(renderer);
}

// -----------------------------------------------------------------------------
// Opens the map editor launcher dialog to create or open a map
// -----------------------------------------------------------------------------
bool MapEditor::chooseMap(Archive* archive)
{
	if (!map_window)
		init();

	return map_window->chooseMap(archive);
}

// -----------------------------------------------------------------------------
// Sets the active undo [manager] for the map editor
// -----------------------------------------------------------------------------
void MapEditor::setUndoManager(UndoManager* manager)
{
	map_window->setUndoManager(manager);
}

// -----------------------------------------------------------------------------
// Sets the map editor window status bar [text] at [column]
// -----------------------------------------------------------------------------
void ::MapEditor::setStatusText(string_view text, int column)
{
	map_window->CallAfter(&MapEditorWindow::SetStatusText, WxUtils::stringFromView(text), column);
}

// -----------------------------------------------------------------------------
// Locks or unlocks the mouse cursor
// -----------------------------------------------------------------------------
void MapEditor::lockMouse(bool lock)
{
	edit_context->canvas()->lockMouse(lock);
}

// -----------------------------------------------------------------------------
// Pops up the context menu for the current edit mode
// -----------------------------------------------------------------------------
void MapEditor::openContextMenu()
{
	// Context menu
	wxMenu menu_context;

	// Set 3d camera
	SAction::fromId("mapw_camera_set")->addToMenu(&menu_context, true);

	// Run from here
	SAction::fromId("mapw_run_map_here")->addToMenu(&menu_context, true);

	// Mode-specific
	bool object_selected = edit_context->selection().hasHilightOrSelection();
	if (edit_context->editMode() == Mode::Vertices)
	{
		menu_context.AppendSeparator();
		SAction::fromId("mapw_vertex_create")->addToMenu(&menu_context, true);
	}
	else if (edit_context->editMode() == Mode::Lines)
	{
		if (object_selected)
		{
			menu_context.AppendSeparator();
			SAction::fromId("mapw_line_changetexture")->addToMenu(&menu_context, true);
			SAction::fromId("mapw_line_changespecial")->addToMenu(&menu_context, true);
			SAction::fromId("mapw_line_tagedit")->addToMenu(&menu_context, true);
			SAction::fromId("mapw_line_flip")->addToMenu(&menu_context, true);
			SAction::fromId("mapw_line_correctsectors")->addToMenu(&menu_context, true);
		}
	}
	else if (edit_context->editMode() == Mode::Things)
	{
		menu_context.AppendSeparator();

		if (object_selected)
			SAction::fromId("mapw_thing_changetype")->addToMenu(&menu_context, true);

		SAction::fromId("mapw_thing_create")->addToMenu(&menu_context, true);
	}
	else if (edit_context->editMode() == Mode::Sectors)
	{
		if (object_selected)
		{
			SAction::fromId("mapw_sector_changetexture")->addToMenu(&menu_context, true);
			SAction::fromId("mapw_sector_changespecial")->addToMenu(&menu_context, true);
			if (edit_context->selection().size() > 1)
			{
				SAction::fromId("mapw_sector_join")->addToMenu(&menu_context, true);
				SAction::fromId("mapw_sector_join_keep")->addToMenu(&menu_context, true);
			}
		}

		SAction::fromId("mapw_sector_create")->addToMenu(&menu_context, true);
	}

	if (object_selected)
	{
		// General edit
		menu_context.AppendSeparator();
		SAction::fromId("mapw_edit_objects")->addToMenu(&menu_context, true);
		SAction::fromId("mapw_mirror_x")->addToMenu(&menu_context, true);
		SAction::fromId("mapw_mirror_y")->addToMenu(&menu_context, true);

		// Properties
		menu_context.AppendSeparator();
		SAction::fromId("mapw_item_properties")->addToMenu(&menu_context, true);
	}

	map_window->PopupMenu(&menu_context);
}

// -----------------------------------------------------------------------------
// Opens [object] in the map editor object properties panel
// -----------------------------------------------------------------------------
void MapEditor::openObjectProperties(MapObject* object)
{
	map_window->propsPanel()->openObject(object);
}

// -----------------------------------------------------------------------------
// Opens multiple [objects] in the map editor object properties panel
// -----------------------------------------------------------------------------
void MapEditor::openMultiObjectProperties(vector<MapObject*>& objects)
{
	map_window->propsPanel()->openObjects(objects);
}

// -----------------------------------------------------------------------------
// Shows or hides the shape draw panel
// -----------------------------------------------------------------------------
void MapEditor::showShapeDrawPanel(bool show)
{
	map_window->showShapeDrawPanel(show);
}

// -----------------------------------------------------------------------------
// Shows or hides the object edit panel and opens [group] if being shown
// -----------------------------------------------------------------------------
void MapEditor::showObjectEditPanel(bool show, ObjectEditGroup* group)
{
	map_window->showObjectEditPanel(show, group);
}

// -----------------------------------------------------------------------------
// Opens the texture browser for [tex_type] textures, with [init_texture]
// initially selected. Returns the selected texture
// -----------------------------------------------------------------------------
string MapEditor::browseTexture(string_view init_texture, int tex_type, SLADEMap& map, string_view title)
{
	// Unlock cursor if locked
	bool cursor_locked = edit_context->mouseLocked();
	if (cursor_locked)
		edit_context->lockMouse(false);

	// Setup texture browser
	MapTextureBrowser browser(map_window, tex_type, init_texture, &map);
	browser.SetTitle(WxUtils::stringFromView(title));

	// Get selected texture
	string tex;
	if (browser.ShowModal() == wxID_OK)
		tex = browser.getSelectedItem()->name();

	// Re-lock cursor if needed
	if (cursor_locked)
		edit_context->lockMouse(true);

	return tex;
}

// -----------------------------------------------------------------------------
// Opens the thing type browser with [init_type] initially selected.
// Returns the selected type
// -----------------------------------------------------------------------------
int MapEditor::browseThingType(int init_type, SLADEMap& map)
{
	// Unlock cursor if locked
	bool cursor_locked = edit_context->mouseLocked();
	if (cursor_locked)
		edit_context->lockMouse(false);

	// Setup thing browser
	ThingTypeBrowser browser(map_window, init_type);

	// Get selected type
	int type = -1;
	if (browser.ShowModal() == wxID_OK)
		type = browser.getSelectedType();

	// Re-lock cursor if needed
	if (cursor_locked)
		edit_context->lockMouse(true);

	return type;
}

// -----------------------------------------------------------------------------
// Opens the appropriate properties dialog for objects in [list].
// Returns true if the property edit was applied
// -----------------------------------------------------------------------------
bool MapEditor::editObjectProperties(vector<MapObject*>& list)
{
	string selsize;
	string type    = edit_context->modeString(false);
	if (list.size() == 1)
		type += fmt::sprintf(" #%d", list[0]->index());
	else if (list.size() > 1)
		selsize = fmt::sprintf("(%lu selected)", list.size());

	// Create dialog for properties panel
	SDialog dlg(
		MapEditor::window(),
		fmt::sprintf("%s Properties %s", type, selsize),
		fmt::sprintf("mobjprops_%s", edit_context->modeString(false)),
		-1,
		-1);
	auto sizer = new wxBoxSizer(wxVERTICAL);
	dlg.SetSizer(sizer);

	// Create/add properties panel
	PropsPanelBase* panel_props = nullptr;
	switch (edit_context->editMode())
	{
	case Mode::Lines: panel_props = new LinePropsPanel(&dlg); break;
	case Mode::Sectors: panel_props = new SectorPropsPanel(&dlg); break;
	case Mode::Things: panel_props = new ThingPropsPanel(&dlg); break;
	default: panel_props = new MapObjectPropsPanel(&dlg, true);
	}
	sizer->Add(panel_props, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, UI::padLarge());

	// Add dialog buttons
	sizer->AddSpacer(UI::pad());
	sizer->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, UI::padLarge());

	// Open current selection
	panel_props->openObjects(list);

	// Open the dialog and apply changes if OK was clicked
	dlg.SetMinClientSize(sizer->GetMinSize());
	dlg.CenterOnParent();
	if (dlg.ShowModal() == wxID_OK)
	{
		panel_props->applyChanges();
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
// Resets/clears the object properties panel
// -----------------------------------------------------------------------------
void MapEditor::resetObjectPropertiesPanel()
{
	map_window->propsPanel()->clearGrid();
}

// -----------------------------------------------------------------------------
// Returns the 'base' item type for [type]
// (eg. WallMiddle is technically a Side)
// -----------------------------------------------------------------------------
MapEditor::ItemType MapEditor::baseItemType(const ItemType& type)
{
	switch (type)
	{
	case ItemType::Vertex: return ItemType::Vertex;
	case ItemType::Line: return ItemType::Line;
	case ItemType::Side:
	case ItemType::WallBottom:
	case ItemType::WallMiddle:
	case ItemType::WallTop: return ItemType::Side;
	case ItemType::Sector:
	case ItemType::Ceiling:
	case ItemType::Floor: return ItemType::Sector;
	case ItemType::Thing: return ItemType::Thing;
	default: return ItemType::Any;
	}
}

// -----------------------------------------------------------------------------
// Returns the map editor item type of the given map [object]
// -----------------------------------------------------------------------------
MapEditor::ItemType MapEditor::itemTypeFromObject(const MapObject* object)
{
	switch (object->objType())
	{
	case MapObject::Type::Vertex: return ItemType::Vertex;
	case MapObject::Type::Line: return ItemType::Line;
	case MapObject::Type::Side: return ItemType::Side;
	case MapObject::Type::Sector: return ItemType::Sector;
	case MapObject::Type::Thing: return ItemType::Thing;
	default: return ItemType::Any;
	}
}
