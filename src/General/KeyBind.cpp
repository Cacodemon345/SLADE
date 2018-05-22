
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    KeyBind.cpp
// Description: Input key binding system
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
#include "KeyBind.h"
#include "TextEditor/TextStyle.h"
#include "Utility/Tokenizer.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace
{
vector<KeyBind>         keybinds;
vector<KeyBind>         keybinds_sorted;
KeyBind                 kb_none("-none-");
vector<KeyBindHandler*> kb_handlers;
} // namespace


// -----------------------------------------------------------------------------
//
// KeyBind Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Adds a key combination to the keybind
// -----------------------------------------------------------------------------
void KeyBind::addKey(string_view key, bool alt, bool ctrl, bool shift)
{
	keys_.emplace_back(key, alt, ctrl, shift);
}

// -----------------------------------------------------------------------------
// Returns a string representation of all the keys bound to this keybind
// -----------------------------------------------------------------------------
string KeyBind::keysAsString()
{
	string ret;

	for (unsigned a = 0; a < keys_.size(); a++)
	{
		if (keys_[a].ctrl)
			ret += "Ctrl+";
		if (keys_[a].alt)
			ret += "Alt+";
		if (keys_[a].shift)
			ret += "Shift+";

		string keyname = keys_[a].key;
		std::replace(keyname.begin(), keyname.end(), '_', ' ');
		StrUtil::capitalizeIP(keyname);
		ret += keyname;

		if (a < keys_.size() - 1)
			ret += ", ";
	}

	if (ret.empty())
		return "None";
	else
		return ret;
}


// -----------------------------------------------------------------------------
//
// KeyBind Class Static Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the keybind [name]
// -----------------------------------------------------------------------------
KeyBind& KeyBind::getBind(string_view name)
{
	for (auto& keybind : keybinds)
		if (keybind.name_ == name)
			return keybind;

	return kb_none;
}

// -----------------------------------------------------------------------------
// Returns a list of all keybind names bound to [key]
// -----------------------------------------------------------------------------
vector<string> KeyBind::getBinds(const KeyPress& key)
{
	vector<string> matches;

	// Go through all keybinds
	for (auto& kb : keybinds)
	{
		// Go through all keys bound to this keybind
		for (unsigned a = 0; a < kb.keys_.size(); a++)
		{
			KeyPress& kp = kb.keys_[a];

			// Check for match with keypress
			if (kp.shift == key.shift && kp.alt == key.alt && kp.ctrl == key.ctrl && kp.key == key.key)
				matches.push_back(kb.name_);
		}
	}

	return matches;
}

// -----------------------------------------------------------------------------
// Returns true if keybind [name] is currently pressed
// -----------------------------------------------------------------------------
bool KeyBind::isPressed(string_view name)
{
	return getBind(name).pressed_;
}

// -----------------------------------------------------------------------------
// Adds a new keybind
// -----------------------------------------------------------------------------
bool KeyBind::addBind(
	string_view name,
	KeyPress    key,
	string_view desc,
	string_view group,
	bool        ignore_shift,
	int         priority)
{
	// Find keybind
	KeyBind* bind = nullptr;
	for (auto& keybind : keybinds)
	{
		if (keybind.name_ == name)
		{
			bind = &keybind;
			break;
		}
	}

	// Add keybind if it doesn't exist
	if (!bind)
	{
		keybinds.emplace_back(name);
		bind                = &keybinds.back();
		bind->ignore_shift_ = ignore_shift;
	}

	// Set keybind description/group
	if (!desc.empty())
	{
		S_SET_VIEW(bind->description_, desc);
		S_SET_VIEW(bind->group_, group);
	}

	// Check if the key is already bound to it
	for (unsigned a = 0; a < bind->keys_.size(); a++)
	{
		if (bind->keys_[a].alt == key.alt && bind->keys_[a].ctrl == key.ctrl && bind->keys_[a].shift == key.shift
			&& bind->keys_[a].key == key.key)
		{
			// It is, remove the bind
			bind->keys_.erase(bind->keys_.begin() + a);
			return false;
		}
	}

	// Set priority
	if (priority >= 0)
		bind->priority_ = priority;

	// Add the keybind
	bind->addKey(key.key, key.alt, key.ctrl, key.shift);

	return true;
}

// -----------------------------------------------------------------------------
// Returns a string representation of [key]
// -----------------------------------------------------------------------------
string KeyBind::keyName(int key)
{
	// Return string representation of key id
	switch (key)
	{
	case WXK_BACK: return "backspace";
	case WXK_TAB: return "tab";
	case WXK_RETURN: return "return";
	case WXK_ESCAPE: return "escape";
	case WXK_SPACE: return "space";
	case WXK_DELETE: return "delete";
	case WXK_CLEAR: return "clear";
	case WXK_SHIFT: return "shift";
	case WXK_ALT: return "alt";
	case WXK_PAUSE: return "pause";
	case WXK_END: return "end";
	case WXK_HOME: return "home";
	case WXK_LEFT: return "left";
	case WXK_UP: return "up";
	case WXK_RIGHT: return "right";
	case WXK_DOWN: return "down";
	case WXK_INSERT: return "insert";
	case WXK_NUMPAD0: return "num_0";
	case WXK_NUMPAD1: return "num_1";
	case WXK_NUMPAD2: return "num_2";
	case WXK_NUMPAD3: return "num_3";
	case WXK_NUMPAD4: return "num_4";
	case WXK_NUMPAD5: return "num_5";
	case WXK_NUMPAD6: return "num_6";
	case WXK_NUMPAD7: return "num_7";
	case WXK_NUMPAD8: return "num_8";
	case WXK_NUMPAD9: return "num_9";
	case WXK_ADD: return "plus";
	case WXK_SUBTRACT: return "minus";
	case WXK_F1: return "f1";
	case WXK_F2: return "f2";
	case WXK_F3: return "f3";
	case WXK_F4: return "f4";
	case WXK_F5: return "f5";
	case WXK_F6: return "f6";
	case WXK_F7: return "f7";
	case WXK_F8: return "f8";
	case WXK_F9: return "f9";
	case WXK_F10: return "f10";
	case WXK_F11: return "f11";
	case WXK_F12: return "f12";
	case WXK_F13: return "f13";
	case WXK_F14: return "f14";
	case WXK_F15: return "f15";
	case WXK_F16: return "f16";
	case WXK_F17: return "f17";
	case WXK_F18: return "f18";
	case WXK_F19: return "f19";
	case WXK_F20: return "f20";
	case WXK_F21: return "f21";
	case WXK_F22: return "f22";
	case WXK_F23: return "f23";
	case WXK_F24: return "f24";
	case WXK_NUMLOCK: return "numlock";
	case WXK_PAGEUP: return "pageup";
	case WXK_PAGEDOWN: return "pagedown";
	case WXK_NUMPAD_SPACE: return "num_space";
	case WXK_NUMPAD_TAB: return "num_tab";
	case WXK_NUMPAD_ENTER: return "num_enter";
	case WXK_NUMPAD_F1: return "num_f1";
	case WXK_NUMPAD_F2: return "num_f2";
	case WXK_NUMPAD_F3: return "num_f3";
	case WXK_NUMPAD_F4: return "num_f4";
	case WXK_NUMPAD_HOME: return "num_home";
	case WXK_NUMPAD_LEFT: return "num_left";
	case WXK_NUMPAD_UP: return "num_up";
	case WXK_NUMPAD_RIGHT: return "num_right";
	case WXK_NUMPAD_DOWN: return "num_down";
	case WXK_NUMPAD_PAGEUP: return "num_pageup";
	case WXK_NUMPAD_PAGEDOWN: return "num_pagedown";
	case WXK_NUMPAD_END: return "num_end";
	case WXK_NUMPAD_BEGIN: return "num_begin";
	case WXK_NUMPAD_INSERT: return "num_insert";
	case WXK_NUMPAD_DELETE: return "num_delete";
	case WXK_NUMPAD_EQUAL: return "num_equal";
	case WXK_NUMPAD_MULTIPLY: return "num_multiply";
	case WXK_NUMPAD_ADD: return "num_plus";
	case WXK_NUMPAD_SEPARATOR: return "num_separator";
	case WXK_NUMPAD_SUBTRACT: return "num_minus";
	case WXK_NUMPAD_DECIMAL: return "num_decimal";
	case WXK_NUMPAD_DIVIDE: return "num_divide";
	case WXK_WINDOWS_LEFT: return "win_left";
	case WXK_WINDOWS_RIGHT: return "win_right";
	case WXK_WINDOWS_MENU: return "win_menu";
	case WXK_PRINT: return "printscrn";
#ifdef __APPLE__
	case WXK_COMMAND: return "command";
#else
	case WXK_CONTROL: return "control";
#endif
	default: break;
	};

	// Check for ascii character
	if (key > 32 && key < 128)
		return { (char)key };

	// Unknown character, just return "key##"
	return fmt::format("key{}", key);
}

// -----------------------------------------------------------------------------
// Returns a string representation of mouse [button]
// -----------------------------------------------------------------------------
string KeyBind::mbName(int button)
{
	switch (button)
	{
	case wxMOUSE_BTN_LEFT: return "mouse1";
	case wxMOUSE_BTN_RIGHT: return "mouse2";
	case wxMOUSE_BTN_MIDDLE: return "mouse3";
	case wxMOUSE_BTN_AUX1: return "mouse4";
	case wxMOUSE_BTN_AUX2: return "mouse5";
	default: return fmt::format("mouse{}", button);
	};
}

// -----------------------------------------------------------------------------
// 'Presses' all keybinds bound to [key]
// -----------------------------------------------------------------------------
bool KeyBind::keyPressed(KeyPress key)
{
	// Ignore raw modifier keys
	if (key.key == "control" || key.key == "shift" || key.key == "alt" || key.key == "command")
		return false;

	// Go through all keybinds
	// (use sorted list for priority system)
	bool pressed = false;
	for (auto& kb : keybinds_sorted)
	{
		// Go through all keys bound to this keybind
		for (unsigned a = 0; a < kb.keys_.size(); a++)
		{
			KeyPress& kp = kb.keys_[a];

			// Check for match with keypress
			if ((kp.shift == key.shift || kb.ignore_shift_) && kp.alt == key.alt && kp.ctrl == key.ctrl
				&& kp.key == key.key)
			{
				// Set bind state
				getBind(kb.name_).pressed_ = true;

				// Send key pressed event to keybind handlers
				for (auto& kb_handler : kb_handlers)
					kb_handler->onKeyBindPress(kb.name_);

				pressed = true;
				break;
			}
		}
	}

	return pressed;
}

// -----------------------------------------------------------------------------
// 'Releases' all keybinds bound to [key]
// -----------------------------------------------------------------------------
bool KeyBind::keyReleased(string_view key)
{
	// Ignore raw modifier keys
	if (key == "control" || key == "shift" || key == "alt" || key == "command")
		return false;

	// Go through all keybinds
	bool released = false;
	for (auto& kb : keybinds)
	{
		// Go through all keys bound to this keybind
		for (unsigned a = 0; a < kb.keys_.size(); a++)
		{
			// Check for match with keypress, and that it is currently pressed
			if (kb.keys_[a].key == key && kb.pressed_)
			{
				// Set bind state
				kb.pressed_ = false;

				// Send key released event to keybind handlers
				for (auto& kb_handler : kb_handlers)
					kb_handler->onKeyBindRelease(kb.name_);

				released = true;
				break;
			}
		}
	}

	return released;
}

// -----------------------------------------------------------------------------
// 'Presses' the keybind [name]
// -----------------------------------------------------------------------------
void KeyBind::pressBind(string_view name)
{
	for (auto& keybind : keybinds)
	{
		if (keybind.name_ == name)
		{
			// Send key pressed event to keybind handlers
			for (auto& kb_handler : kb_handlers)
				kb_handler->onKeyBindPress(name);

			return;
		}
	}
}

// -----------------------------------------------------------------------------
// Returns [keycode] and [modifiers] as a keypress_t struct
// -----------------------------------------------------------------------------
KeyPress KeyBind::asKeyPress(int keycode, int modifiers)
{
	return KeyPress(
		keyName(keycode),
		((modifiers & wxMOD_ALT) != 0),
		((modifiers & wxMOD_CMD) != 0),
		((modifiers & wxMOD_SHIFT) != 0));
}

// -----------------------------------------------------------------------------
// Adds all keybinds to [list]
// -----------------------------------------------------------------------------
void KeyBind::allKeyBinds(vector<KeyBind*>& list)
{
	for (auto& keybind : keybinds)
		list.push_back(&keybind);
}

// -----------------------------------------------------------------------------
// 'Releases' all keybinds
// -----------------------------------------------------------------------------
void KeyBind::releaseAll()
{
	for (auto& keybind : keybinds)
		keybind.pressed_ = false;
}

// -----------------------------------------------------------------------------
// Initialises all default keybinds
// -----------------------------------------------------------------------------
void KeyBind::initBinds()
{
	// General
	string group = "General";
	addBind("copy", KeyPress("C", KPM_CTRL), "Copy", group);
	addBind("cut", KeyPress("X", KPM_CTRL), "Cut", group);
	addBind("paste", KeyPress("V", KPM_CTRL), "Paste", group);
	addBind("select_all", KeyPress("A", KPM_CTRL), "Select All", group);

	// Entry List (el*)
	group = "Entry List";
	addBind("el_new", KeyPress("N", KPM_CTRL), "New Entry", group);
	addBind("el_delete", KeyPress("delete"), "Delete Entry", group);
	addBind("el_move_up", KeyPress("U", KPM_CTRL), "Move Entry up", group);
	addBind("el_move_down", KeyPress("D", KPM_CTRL), "Move Entry down", group);
	addBind("el_rename", KeyPress("R", KPM_CTRL), "Rename Entry", group);
	addBind("el_rename", KeyPress("f2"));
	addBind("el_import", KeyPress("I", KPM_CTRL), "Import to Entry", group);
	addBind("el_import_files", KeyPress("I", KPM_CTRL | KPM_SHIFT), "Import Files", group);
	addBind("el_export", KeyPress("E", KPM_CTRL), "Export Entry", group);
	addBind("el_up_dir", KeyPress("backspace"), "Up one directory", group);

	// Text editor (ted*)
	group = "Text Editor";
	addBind("ted_autocomplete", KeyPress("space", KPM_CTRL), "Open Autocompletion list", group);
	addBind("ted_calltip", KeyPress("space", KPM_CTRL | KPM_SHIFT), "Open CallTip", group);
	addBind("ted_findreplace", KeyPress("F", KPM_CTRL), "Find/Replace", group);
	addBind("ted_findnext", KeyPress("f3"), "Find next", group);
	addBind("ted_findprev", KeyPress("f3", KPM_SHIFT), "Find previous", group);
	addBind("ted_replacenext", KeyPress("R", KPM_ALT), "Replace next", group);
	addBind("ted_replaceall", KeyPress("R", KPM_ALT | KPM_SHIFT), "Replace all", group);
	addBind("ted_jumptoline", KeyPress("G", KPM_CTRL), "Jump to Line", group);
	addBind("ted_fold_foldall", KeyPress("[", KPM_CTRL | KPM_SHIFT), "Fold All", group);
	addBind("ted_fold_unfoldall", KeyPress("]", KPM_CTRL | KPM_SHIFT), "Fold All", group);
	addBind("ted_line_comment", KeyPress("/", KPM_CTRL), "Line Comment", group);
	addBind("ted_block_comment", KeyPress("/", KPM_CTRL | KPM_SHIFT), "Block Comment", group);

	// Texture editor (txed*)
	group = "Texture Editor";
	addBind("txed_patch_left", KeyPress("left", KPM_CTRL), "Move Patch left", group);
	addBind("txed_patch_left8", KeyPress("left"), "Move Patch left 8", group);
	addBind("txed_patch_up", KeyPress("up", KPM_CTRL), "Move Patch up", group);
	addBind("txed_patch_up8", KeyPress("up"), "Move Patch up 8", group);
	addBind("txed_patch_right", KeyPress("right", KPM_CTRL), "Move Patch right", group);
	addBind("txed_patch_right8", KeyPress("right"), "Move Patch right 8", group);
	addBind("txed_patch_down", KeyPress("down", KPM_CTRL), "Move Patch down", group);
	addBind("txed_patch_down8", KeyPress("down"), "Move Patch down 8", group);
	addBind("txed_patch_add", KeyPress("insert"), "Add Patch", group);
	addBind("txed_patch_delete", KeyPress("delete"), "Delete Patch", group);
	addBind("txed_patch_replace", KeyPress("f2"), "Replace Patch", group);
	addBind("txed_patch_replace", KeyPress("R", KPM_CTRL));
	addBind("txed_patch_duplicate", KeyPress("D", KPM_CTRL), "Duplicate Patch", group);
	addBind("txed_patch_forward", KeyPress("]"), "Bring Patch forward", group);
	addBind("txed_patch_back", KeyPress("["), "Send Patch back", group);
	addBind("txed_tex_up", KeyPress("up", KPM_CTRL), "Move Texture up", group);
	addBind("txed_tex_up", KeyPress("U", KPM_CTRL));
	addBind("txed_tex_down", KeyPress("down", KPM_CTRL), "Move Texture down", group);
	addBind("txed_tex_down", KeyPress("D", KPM_CTRL));
	addBind("txed_tex_new", KeyPress("N", KPM_CTRL), "New Texture", group);
	addBind("txed_tex_new_patch", KeyPress("N", KPM_CTRL | KPM_SHIFT), "New Texture from Patch", group);
	addBind("txed_tex_new_file", KeyPress("N", KPM_CTRL | KPM_ALT), "New Texture from File", group);
	addBind("txed_tex_delete", KeyPress("delete"), "Delete Texture", group);

	// Map Editor (map*)
	group = "Map Editor General";
	addBind("map_edit_accept", KeyPress("return"), "Accept edit", group);
	addBind("map_edit_accept", KeyPress("num_enter"));
	addBind("map_edit_cancel", KeyPress("escape"), "Cancel edit", group);
	addBind("map_toggle_3d", KeyPress("Q"), "Toggle 3d mode", group);
	addBind("map_screenshot", KeyPress("P", KPM_CTRL | KPM_SHIFT), "Take Screenshot", group);

	// Map Editor 2D (me2d*)
	group = "Map Editor 2D Mode";
	addBind("me2d_clear_selection", KeyPress("C"), "Clear selection", group);
	addBind("me2d_pan_view", KeyPress("mouse3"), "Pan view", group);
	addBind("me2d_pan_view", KeyPress("space", KPM_CTRL));
	addBind("me2d_move", KeyPress("Z"), "Toggle item move mode", group);
	addBind("me2d_zoom_in_m", KeyPress("mwheelup"), "Zoom in (towards mouse)", group);
	addBind("me2d_zoom_out_m", KeyPress("mwheeldown"), "Zoom out (towards mouse)", group);
	addBind("me2d_zoom_in", KeyPress("="), "Zoom in (towards screen center)", group);
	addBind("me2d_zoom_out", KeyPress("-"), "Zoom out (towards screen center)", group);
	addBind("me2d_show_object", KeyPress("=", KPM_SHIFT), "Zoom in, show current object", group);
	addBind("me2d_show_object", KeyPress("mwheelup", KPM_SHIFT));
	addBind("me2d_show_all", KeyPress("-", KPM_SHIFT), "Zoom out, show full map", group);
	addBind("me2d_show_all", KeyPress("mwheeldown", KPM_SHIFT));
	addBind("me2d_left", KeyPress("left"), "Scroll left", group);
	addBind("me2d_right", KeyPress("right"), "Scroll right", group);
	addBind("me2d_up", KeyPress("up"), "Scroll up", group);
	addBind("me2d_down", KeyPress("down"), "Scroll down", group);
	addBind("me2d_grid_inc", KeyPress("["), "Increment grid level", group);
	addBind("me2d_grid_dec", KeyPress("]"), "Decrement grid level", group);
	addBind("me2d_grid_toggle_snap", KeyPress("G", KPM_SHIFT), "Toggle Grid Snap", group);
	addBind("me2d_mode_vertices", KeyPress("V"), "Vertices mode", group);
	addBind("me2d_mode_lines", KeyPress("L"), "Lines mode", group);
	addBind("me2d_mode_sectors", KeyPress("S"), "Sectors mode", group);
	addBind("me2d_mode_things", KeyPress("T"), "Things mode", group);
	addBind("me2d_flat_type", KeyPress("F", KPM_CTRL), "Cycle flat type", group);
	addBind("me2d_split_line", KeyPress("S", KPM_CTRL | KPM_SHIFT), "Split nearest line", group);
	addBind("me2d_lock_hilight", KeyPress("H", KPM_CTRL), "Lock/unlock hilight", group);
	addBind("me2d_begin_linedraw", KeyPress("space"), "Begin line drawing", group);
	addBind("me2d_begin_shapedraw", KeyPress("space", KPM_SHIFT), "Begin shape drawing", group);
	addBind("me2d_create_object", KeyPress("insert"), "Create object", group);
	addBind("me2d_delete_object", KeyPress("delete"), "Delete object", group);
	addBind("me2d_copy_properties", KeyPress("C", KPM_CTRL | KPM_SHIFT), "Copy object properties", group);
	addBind("me2d_paste_properties", KeyPress("V", KPM_CTRL | KPM_SHIFT), "Paste object properties", group);
	addBind("me2d_begin_object_edit", KeyPress("E"), "Begin object edit", group);
	addBind("me2d_toggle_selection_numbers", KeyPress("N"), "Toggle selection numbers", group);
	addBind("me2d_mirror_x", KeyPress("M", KPM_CTRL), "Mirror selection horizontally", group);
	addBind("me2d_mirror_y", KeyPress("M", KPM_CTRL | KPM_SHIFT), "Mirror selection vertically", group);
	addBind("me2d_object_properties", KeyPress("return"), "Object Properties", group, false, 100);

	// Map Editor 2D Lines mode (me2d_line*)
	group = "Map Editor 2D Lines Mode";
	addBind("me2d_line_change_texture", KeyPress("T", KPM_CTRL), "Change texture(s)", group);
	addBind("me2d_line_flip", KeyPress("F"), "Flip line(s)", group);
	addBind("me2d_line_flip_nosides", KeyPress("F", KPM_SHIFT), "Flip line(s) but not sides", group);
	addBind("me2d_line_tag_edit", KeyPress("T", KPM_SHIFT), "Begin tag edit", group);

	// Map Editor 2D Sectors mode (me2d_sector*)
	group = "Map Editor 2D Sectors Mode";
	addBind("me2d_sector_light_up16", KeyPress("'"), "Light level up 16", group);
	addBind("me2d_sector_light_up", KeyPress("'", KPM_SHIFT), "Light level up 1", group);
	addBind("me2d_sector_light_down16", KeyPress(";"), "Light level down 16", group);
	addBind("me2d_sector_light_down", KeyPress(";", KPM_SHIFT), "Light level down 1", group);
	addBind("me2d_sector_floor_up8", KeyPress(".", KPM_CTRL), "Floor height up 8", group);
	addBind("me2d_sector_floor_up", KeyPress(".", KPM_CTRL | KPM_SHIFT), "Floor height up 1", group);
	addBind("me2d_sector_floor_down8", KeyPress(",", KPM_CTRL), "Floor height down 8", group);
	addBind("me2d_sector_floor_down", KeyPress(",", KPM_CTRL | KPM_SHIFT), "Floor height down 1", group);
	addBind("me2d_sector_ceil_up8", KeyPress(".", KPM_ALT), "Ceiling height up 8", group);
	addBind("me2d_sector_ceil_up", KeyPress(".", KPM_ALT | KPM_SHIFT), "Ceiling height up 1", group);
	addBind("me2d_sector_ceil_down8", KeyPress(",", KPM_ALT), "Ceiling height down 8", group);
	addBind("me2d_sector_ceil_down", KeyPress(",", KPM_ALT | KPM_SHIFT), "Ceiling height down 1", group);
	addBind("me2d_sector_height_up8", KeyPress("."), "Height up 8", group);
	addBind("me2d_sector_height_up", KeyPress(".", KPM_SHIFT), "Height up 1", group);
	addBind("me2d_sector_height_down8", KeyPress(","), "Height down 8", group);
	addBind("me2d_sector_height_down", KeyPress(",", KPM_SHIFT), "Height down 1", group);
	addBind("me2d_sector_change_texture", KeyPress("T", KPM_CTRL), "Change texture(s)", group);
	addBind("me2d_sector_join", KeyPress("J"), "Join sectors", group);
	addBind("me2d_sector_join_keep", KeyPress("J", KPM_SHIFT), "Join sectors (keep lines)", group);

	// Map Editor 2D Things mode (me2d_thing*)
	group = "Map Editor 2D Things Mode";
	addBind("me2d_thing_change_type", KeyPress("T", KPM_CTRL), "Change type", group);
	addBind("me2d_thing_quick_angle", KeyPress("D"), "Quick angle edit", group);

	// Map Editor 3D (me3d*)
	group = "Map Editor 3D Mode";
	addBind("me3d_toggle_fog", KeyPress("F"), "Toggle fog", group);
	addBind("me3d_toggle_fullbright", KeyPress("B"), "Toggle full brightness", group);
	addBind("me3d_adjust_brightness", KeyPress("B", KPM_SHIFT), "Adjust brightness", group);
	addBind("me3d_toggle_gravity", KeyPress("G"), "Toggle camera gravity", group);
	addBind("me3d_release_mouse", KeyPress("tab"), "Release mouse cursor", group);
	addBind("me3d_clear_selection", KeyPress("C"), "Clear selection", group);
	addBind("me3d_toggle_things", KeyPress("T"), "Toggle thing display", group);
	addBind("me3d_thing_style", KeyPress("T", KPM_SHIFT), "Cycle thing render style", group);
	addBind("me3d_toggle_hilight", KeyPress("H"), "Toggle hilight", group);
	addBind("me3d_copy_tex_type", KeyPress("C", KPM_CTRL), "Copy texture or thing type", group);
	addBind("me3d_copy_tex_type", KeyPress("mouse3"));
	addBind("me3d_paste_tex_type", KeyPress("V", KPM_CTRL), "Paste texture or thing type", group);
	addBind("me3d_paste_tex_type", KeyPress("mouse3", KPM_CTRL));
	addBind("me3d_paste_tex_adj", KeyPress("mouse3", KPM_SHIFT), "Flood-fill texture", group);
	addBind("me3d_toggle_info", KeyPress("I"), "Toggle information overlay", group);
	addBind("me3d_quick_texture", KeyPress("T", KPM_CTRL), "Quick Texture", group);
	addBind("me3d_generic_up8", KeyPress("mwheelup", KPM_CTRL), "Raise target 8", group);
	addBind("me3d_generic_up", KeyPress("mwheelup", KPM_CTRL | KPM_SHIFT), "Raise target 1", group);
	addBind("me3d_generic_down8", KeyPress("mwheeldown", KPM_CTRL), "Lower target 8", group);
	addBind("me3d_generic_down", KeyPress("mwheeldown", KPM_CTRL | KPM_SHIFT), "Lower target 1", group);

	// Map Editor 3D Camera (me3d_camera*)
	group = "Map Editor 3D Mode Camera";
	addBind("me3d_camera_forward", KeyPress("W"), "Camera forward", group, true);
	addBind("me3d_camera_back", KeyPress("S"), "Camera backward", group, true);
	addBind("me3d_camera_left", KeyPress("A"), "Camera strafe left", group, true);
	addBind("me3d_camera_right", KeyPress("D"), "Camera strafe right", group, true);
	addBind("me3d_camera_up", KeyPress("up"), "Camera move up", group, true);
	addBind("me3d_camera_down", KeyPress("down"), "Camera move down", group, true);
	addBind("me3d_camera_turn_left", KeyPress("left"), "Camera turn left", group, true);
	addBind("me3d_camera_turn_right", KeyPress("right"), "Camera turn right", group, true);

	// Map Editor 3D Light (me3d_light*)
	group = "Map Editor 3D Mode Light";
	addBind("me3d_light_up16", KeyPress("'"), "Sector light level up 16", group);
	addBind("me3d_light_up", KeyPress("'", KPM_SHIFT), "Sector light level up 1", group);
	addBind("me3d_light_down16", KeyPress(";"), "Sector light level down 16", group);
	addBind("me3d_light_down", KeyPress(";", KPM_SHIFT), "Sector light level down 1", group);
	addBind("me3d_light_toggle_link", KeyPress("L", KPM_CTRL), "Toggle linked flat light levels", group);

	// Map Editor 3D Offsets (me3d_xoff*, me3d_yoff*)
	group = "Map Editor 3D Mode Offsets";
	addBind("me3d_xoff_up8", KeyPress("num_4"), "X offset up 8", group);
	addBind("me3d_xoff_up", KeyPress("num_left"), "X offset up 1", group);
	addBind("me3d_xoff_down8", KeyPress("num_6"), "X offset down 8", group);
	addBind("me3d_xoff_down", KeyPress("num_right"), "X offset down 1", group);
	addBind("me3d_yoff_up8", KeyPress("num_8"), "Y offset up 8", group);
	addBind("me3d_yoff_up", KeyPress("num_up"), "Y offset up 1", group);
	addBind("me3d_yoff_down8", KeyPress("num_2"), "Y offset down 8", group);
	addBind("me3d_yoff_down", KeyPress("num_down"), "Y offset down 1", group);
	addBind("me3d_wall_reset", KeyPress("R"), "Reset offsets and scaling", group);
#ifdef __WXGTK__
	addBind("me3d_xoff_up", keypress_t("num_left", KPM_SHIFT));
	addBind("me3d_xoff_down", keypress_t("num_right", KPM_SHIFT));
	addBind("me3d_yoff_up", keypress_t("num_up", KPM_SHIFT));
	addBind("me3d_yoff_down", keypress_t("num_down", KPM_SHIFT));
#endif

	// Map Editor 3D Scaling (me3d_scale*)
	group = "Map Editor 3D Mode Scaling";
	addBind("me3d_scalex_up_l", KeyPress("num_4", KPM_CTRL), "X scale up (large)", group);
	addBind("me3d_scalex_up_s", KeyPress("num_left", KPM_CTRL), "X scale up (small)", group);
	addBind("me3d_scalex_down_l", KeyPress("num_6", KPM_CTRL), "X scale down (large)", group);
	addBind("me3d_scalex_down_s", KeyPress("num_right", KPM_CTRL), "X scale down (small)", group);
	addBind("me3d_scaley_up_l", KeyPress("num_8", KPM_CTRL), "Y scale up (large)", group);
	addBind("me3d_scaley_up_s", KeyPress("num_up", KPM_CTRL), "Y scale up (small)", group);
	addBind("me3d_scaley_down_l", KeyPress("num_2", KPM_CTRL), "Y scale down (large)", group);
	addBind("me3d_scaley_down_s", KeyPress("num_down", KPM_CTRL), "Y scale down (small)", group);

	// Map Editor 3D Walls (me3d_wall*)
	group = "Map Editor 3D Mode Walls";
	addBind("me3d_wall_toggle_link_ofs", KeyPress("O", KPM_CTRL), "Toggle linked wall offsets", group);
	addBind("me3d_wall_autoalign_x", KeyPress("A", KPM_CTRL), "Auto-align textures on X", group);
	addBind("me3d_wall_unpeg_lower", KeyPress("L"), "Toggle lower unpegged", group);
	addBind("me3d_wall_unpeg_upper", KeyPress("U"), "Toggle upper unpegged", group);

	// Map Editor 3D Flats (me3d_flat*)
	group = "Map Editor 3D Mode Flats";
	addBind("me3d_flat_height_up8", KeyPress("num_plus"), "Height up 8", group);
	addBind("me3d_flat_height_up8", KeyPress("mwheelup"));
	addBind("me3d_flat_height_up", KeyPress("num_plus", KPM_SHIFT), "Height up 1", group);
	addBind("me3d_flat_height_up", KeyPress("mwheelup", KPM_SHIFT));
	addBind("me3d_flat_height_down8", KeyPress("num_minus"), "Height down 8", group);
	addBind("me3d_flat_height_down8", KeyPress("mwheeldown"));
	addBind("me3d_flat_height_down", KeyPress("num_minus", KPM_SHIFT), "Height down 1", group);
	addBind("me3d_flat_height_down", KeyPress("mwheeldown", KPM_SHIFT));

	// Map Editor 3D Things (me3d_thing*)
	group = "Map Editor 3D Mode Things";
	addBind("me3d_thing_remove", KeyPress("delete"), "Remove", group);
	addBind("me3d_thing_up8", KeyPress("num_8"), "Z up 8", group);
	addBind("me3d_thing_up", KeyPress("num_up"), "Z up 1", group);
	addBind("me3d_thing_down8", KeyPress("num_2"), "Z down 8", group);
	addBind("me3d_thing_down", KeyPress("num_down"), "Z down 1", group);

	// Set above keys as defaults
	for (auto& keybind : keybinds)
	{
		for (unsigned k = 0; k < keybind.keys_.size(); k++)
			keybind.defaults_.push_back(keybind.keys_[k]);
	}

	// Create sorted list
	keybinds_sorted = keybinds;
	std::sort(keybinds_sorted.begin(), keybinds_sorted.end());
}

// -----------------------------------------------------------------------------
// Writes all keybind definitions as a string
// -----------------------------------------------------------------------------
string KeyBind::writeBinds()
{
	// Init string
	string ret;

	// Go through all keybinds
	for (auto& kb : keybinds)
	{
		// Add keybind line
		ret += "\t";
		ret += kb.name_;

		// 'unbound' indicates no binds
		if (kb.keys_.empty())
			ret += " unbound";

		// Go through all bound keys
		for (unsigned a = 0; a < kb.keys_.size(); a++)
		{
			KeyPress& kp = kb.keys_[a];
			ret += " \"";

			// Add modifiers (if any)
			if (kp.alt)
				ret += "a";
			if (kp.ctrl)
				ret += "c";
			if (kp.shift)
				ret += "s";
			if (kp.alt || kp.ctrl || kp.shift)
				ret += "|";

			// Add key
			ret += kp.key;
			ret += "\"";

			// Add comma if there are any more keys
			if (a < kb.keys_.size() - 1)
				ret += ",";
		}

		ret += "\n";
	}

	return ret;
}

// -----------------------------------------------------------------------------
// Reads keybind defeinitions from tokenizer [tz]
// -----------------------------------------------------------------------------
bool KeyBind::readBinds(Tokenizer& tz)
{
	// Parse until ending }
	while (!tz.checkOrEnd("}"))
	{
		// Clear any current binds for the key
		string name = tz.current().text;
		getBind(name).keys_.clear();

		// Read keys
		while (true)
		{
			string keystr = tz.next().text;

			// Finish if no keys are bound
			if (keystr == "unbound")
				break;

			// Parse key string
			// string key, mods;
			auto        sep = keystr.find_first_of('|');
			string_view key{ keystr }, mods;
			if (sep != string::npos)
			{
				mods = key.substr(0, sep);
				key  = key.substr(sep);
			}

			// Add the key
			addBind(
				name,
				{ key,
				  mods.find('a') != string_view::npos,
				  mods.find('c') != string_view::npos,
				  mods.find('s') != string_view::npos });

			// Check for more keys
			if (!tz.advIfNext(","))
				break;
		}

		// Next keybind
		tz.adv();
	}

	// Create sorted list
	keybinds_sorted = keybinds;
	std::sort(keybinds_sorted.begin(), keybinds_sorted.end());

	return true;
}

// -----------------------------------------------------------------------------
// Updates the sorted keybinds list
// -----------------------------------------------------------------------------
void KeyBind::updateSortedBindsList()
{
	// Create sorted list
	keybinds_sorted.clear();
	keybinds_sorted = keybinds;
	std::sort(keybinds_sorted.begin(), keybinds_sorted.end());
}


// -----------------------------------------------------------------------------
//
// KeyBindHandler Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// KeyBindHandler class constructor
// -----------------------------------------------------------------------------
KeyBindHandler::KeyBindHandler()
{
	kb_handlers.push_back(this);
}

// -----------------------------------------------------------------------------
// KeyBindHandler class destructor
// -----------------------------------------------------------------------------
KeyBindHandler::~KeyBindHandler()
{
	for (unsigned a = 0; a < kb_handlers.size(); a++)
	{
		if (kb_handlers[a] == this)
		{
			kb_handlers.erase(kb_handlers.begin() + a);
			a--;
		}
	}
}
