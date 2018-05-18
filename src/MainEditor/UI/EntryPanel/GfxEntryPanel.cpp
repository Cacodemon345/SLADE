
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    GfxEntryPanel.cpp
// Description: GfxEntryPanel class. The UI for editing gfx entries.
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
#include "Archive/Archive.h"
#include "Dialogs/GfxColouriseDialog.h"
#include "Dialogs/GfxConvDialog.h"
#include "Dialogs/GfxCropDialog.h"
#include "Dialogs/GfxTintDialog.h"
#include "Dialogs/ModifyOffsetsDialog.h"
#include "Dialogs/TranslationEditorDialog.h"
#include "General/Console/ConsoleHelpers.h"
#include "General/Misc.h"
#include "General/UI.h"
#include "GfxEntryPanel.h"
#include "Graphics/Icons.h"
#include "MainEditor/EntryOperations.h"
#include "MainEditor/MainEditor.h"
#include "MainEditor/UI/MainWindow.h"
#include "UI/Controls/PaletteChooser.h"
#include "UI/Controls/SIconButton.h"
#include "UI/Controls/SZoomSlider.h"
#include "UI/SBrush.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, gfx_arc)
EXTERN_CVAR(String, last_colour)
EXTERN_CVAR(String, last_tint_colour)
EXTERN_CVAR(Int, last_tint_amount)


// -----------------------------------------------------------------------------
//
// GfxEntryPanel Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// GfxEntryPanel class constructor
// -----------------------------------------------------------------------------
GfxEntryPanel::GfxEntryPanel(wxWindow* parent) : EntryPanel(parent, "gfx")
{
	// Init variables
	prev_translation_.addRange(TransRange::Type::Palette, 0);
	edit_translation_.addRange(TransRange::Type::Palette, 0);

	// Add gfx canvas
	gfx_canvas_ = new GfxCanvas(this, -1);
	sizer_main_->Add(gfx_canvas_->toPanel(this), 1, wxEXPAND, 0);
	gfx_canvas_->setViewType(GfxCanvas::View::Default);
	gfx_canvas_->allowDrag(true);
	gfx_canvas_->allowScroll(true);
	gfx_canvas_->setPalette(MainEditor::currentPalette());
	gfx_canvas_->setTranslation(&edit_translation_);

	// Offsets
	wxSize spinsize = { UI::px(UI::Size::SpinCtrlWidth), -1 };
	spin_xoffset_   = new wxSpinCtrl(
		this,
		-1,
		wxEmptyString,
		wxDefaultPosition,
		wxDefaultSize,
		wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER,
		SHRT_MIN,
		SHRT_MAX,
		0);
	spin_yoffset_ = new wxSpinCtrl(
		this,
		-1,
		wxEmptyString,
		wxDefaultPosition,
		wxDefaultSize,
		wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER,
		SHRT_MIN,
		SHRT_MAX,
		0);
	spin_xoffset_->SetMinSize(spinsize);
	spin_yoffset_->SetMinSize(spinsize);
	sizer_bottom_->Add(new wxStaticText(this, -1, "Offsets:"), 0, wxALIGN_CENTER_VERTICAL, 0);
	sizer_bottom_->Add(spin_xoffset_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, UI::pad());
	sizer_bottom_->Add(spin_yoffset_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, UI::pad());

	// Gfx (offset) type
	wxString offset_types[] = { "Auto", "Graphic", "Sprite", "HUD" };
	choice_offset_type_     = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, 4, offset_types);
	choice_offset_type_->SetSelection(0);
	sizer_bottom_->Add(choice_offset_type_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, UI::pad());

	// Auto offset
	btn_auto_offset_ = new SIconButton(this, "offset", "Modify Offsets...");
	sizer_bottom_->Add(btn_auto_offset_, 0, wxALIGN_CENTER_VERTICAL);

	sizer_bottom_->AddStretchSpacer();

	// Aspect ratio correction checkbox
	cb_arc_ = new wxCheckBox(this, -1, "Aspect Ratio Correction");
	cb_arc_->SetValue(gfx_arc);
	sizer_bottom_->Add(cb_arc_, 0, wxEXPAND, 0);
	sizer_bottom_->AddSpacer(UI::padLarge());

	// Tile checkbox
	cb_tile_ = new wxCheckBox(this, -1, "Tile");
	sizer_bottom_->Add(cb_tile_, 0, wxEXPAND, 0);
	sizer_bottom_->AddSpacer(UI::padLarge());

	// Image selection buttons
	btn_nextimg_ = new SIconButton(this, "right");
	btn_previmg_ = new SIconButton(this, "left");
	text_curimg_ = new wxStaticText(this, -1, "Image XX/XX");
	btn_nextimg_->Show(false);
	btn_previmg_->Show(false);
	text_curimg_->Show(false);

	// Palette chooser
	listenTo(theMainWindow->paletteChooser());

	// Custom menu
	menu_custom_ = new wxMenu();
	GfxEntryPanel::fillCustomMenu(menu_custom_);
	custom_menu_name_ = "Graphic";

	// Brushes menu
	menu_brushes_ = new wxMenu();
	fillBrushMenu(menu_brushes_);

	// Custom toolbar
	setupToolbar();

	// Bind Events
	cb_colour_->Bind(wxEVT_COLOURBOX_CHANGED, &GfxEntryPanel::onPaintColourChanged, this);
	spin_xoffset_->Bind(wxEVT_SPINCTRL, &GfxEntryPanel::onXOffsetChanged, this);
	spin_yoffset_->Bind(wxEVT_SPINCTRL, &GfxEntryPanel::onYOffsetChanged, this);
	spin_xoffset_->Bind(wxEVT_TEXT_ENTER, &GfxEntryPanel::onXOffsetChanged, this);
	spin_yoffset_->Bind(wxEVT_TEXT_ENTER, &GfxEntryPanel::onYOffsetChanged, this);
	choice_offset_type_->Bind(wxEVT_CHOICE, &GfxEntryPanel::onOffsetTypeChanged, this);
	cb_tile_->Bind(wxEVT_CHECKBOX, &GfxEntryPanel::onTileChanged, this);
	cb_arc_->Bind(wxEVT_CHECKBOX, &GfxEntryPanel::onARCChanged, this);
	Bind(wxEVT_GFXCANVAS_OFFSET_CHANGED, &GfxEntryPanel::onGfxOffsetChanged, this, gfx_canvas_->GetId());
	Bind(wxEVT_GFXCANVAS_PIXELS_CHANGED, &GfxEntryPanel::onGfxPixelsChanged, this, gfx_canvas_->GetId());
	Bind(wxEVT_GFXCANVAS_COLOUR_PICKED, &GfxEntryPanel::onColourPicked, this, gfx_canvas_->GetId());
	btn_nextimg_->Bind(wxEVT_BUTTON, &GfxEntryPanel::onBtnNextImg, this);
	btn_previmg_->Bind(wxEVT_BUTTON, &GfxEntryPanel::onBtnPrevImg, this);
	btn_auto_offset_->Bind(wxEVT_BUTTON, &GfxEntryPanel::onBtnAutoOffset, this);

	// Apply layout
	EntryPanel::Layout();
}

// -----------------------------------------------------------------------------
// Loads an entry into the entry panel if it is a valid image format
// -----------------------------------------------------------------------------
bool GfxEntryPanel::loadEntry(ArchiveEntry* entry)
{
	return loadEntry(entry, 0);
}
bool GfxEntryPanel::loadEntry(ArchiveEntry* entry, int index)
{
	// Check entry was given
	if (entry == nullptr)
	{
		Global::error = "no entry to load";
		return false;
	}

	// Update variables
	this->entry_ = entry;
	setModified(false);

	// Attempt to load the image
	if (!Misc::loadImageFromEntry(getImage(), this->entry_, index))
		return false;

	// Only show next/prev image buttons if the entry contains multiple images
	if (getImage()->nImages() > 1)
	{
		btn_nextimg_->Show();
		btn_previmg_->Show();
		text_curimg_->Show();
		sizer_bottom_->Add(btn_previmg_, 0, wxEXPAND | wxRIGHT, 4);
		sizer_bottom_->Add(btn_nextimg_, 0, wxEXPAND | wxRIGHT, 4);
		sizer_bottom_->Add(text_curimg_, 0, wxALIGN_CENTER, 0);
	}
	else
	{
		btn_nextimg_->Show(false);
		btn_previmg_->Show(false);
		text_curimg_->Show(false);
		sizer_bottom_->Detach(btn_nextimg_);
		sizer_bottom_->Detach(btn_previmg_);
		sizer_bottom_->Detach(text_curimg_);
	}

	// Hack for colormaps to be 256-wide
	if (StrUtil::equalCI(entry->type()->name(), "colormap"))
	{
		getImage()->setWidth(256);
	}

	// Refresh everything
	refresh();

	return true;
}

// -----------------------------------------------------------------------------
// Saves any changes to the entry
// -----------------------------------------------------------------------------
bool GfxEntryPanel::saveEntry()
{
	// Set offsets
	getImage()->setXOffset(spin_xoffset_->GetValue());
	getImage()->setYOffset(spin_yoffset_->GetValue());

	// Write new image data if modified
	bool ok = true;
	if (image_data_modified_)
	{
		SImage*   image  = getImage();
		SIFormat* format = image->format();

		string error;
		ok           = false;
		int writable = format->canWrite(*image);
		if (format == SIFormat::unknownFormat())
			error = "Image is of unknown format";
		else if (writable == SIFormat::NOTWRITABLE)
			error = S_FMT("Writing unsupported for format \"%s\"", format->name());
		else
		{
			// Convert image if necessary (using default options)
			if (writable == SIFormat::CONVERTIBLE)
			{
				format->convertWritable(*image, SIFormat::ConvertOptions());
				LOG_MESSAGE(1, "Image converted for writing");
			}

			if (format->saveImage(*image, entry_->data(), gfx_canvas_->palette()))
				ok = true;
			else
				error = "Error writing image";
		}

		if (ok)
		{
			// Set modified
			entry_->setState(1);

			// Re-detect type
			EntryType* oldtype = entry_->type();
			EntryType::detectEntryType(entry_);

			// Update extension if type changed
			if (oldtype != entry_->type())
				entry_->setExtensionByType();
		}
		else
			wxMessageBox(wxString("Cannot save changes to image: ") + error, "Error", wxICON_ERROR);
	}
	// Otherwise just set offsets
	else
		EntryOperations::setGfxOffsets(entry_, spin_xoffset_->GetValue(), spin_yoffset_->GetValue());

	// Apply alPh/tRNS options
	if (entry_->type()->formatId() == "img_png")
	{
		bool alph = EntryOperations::getalPhChunk(entry_);
		bool trns = EntryOperations::gettRNSChunk(entry_);

		if (alph != menu_custom_->IsChecked(SAction::fromId("pgfx_alph")->wxId()))
			EntryOperations::modifyalPhChunk(entry_, !alph);
		if (trns != menu_custom_->IsChecked(SAction::fromId("pgfx_trns")->wxId()))
			EntryOperations::modifytRNSChunk(entry_, !trns);
	}

	if (ok)
		setModified(false);

	return ok;
}

// -----------------------------------------------------------------------------
// Adds controls to the entry panel toolbar
// -----------------------------------------------------------------------------
void GfxEntryPanel::setupToolbar()
{
	// Zoom
	SToolBarGroup* g_zoom = new SToolBarGroup(toolbar_, "Zoom");
	slider_zoom_          = new SZoomSlider(g_zoom, gfx_canvas_);
	g_zoom->addCustomControl(slider_zoom_);
	toolbar_->addGroup(g_zoom);

	// Editing operations
	SToolBarGroup* g_edit = new SToolBarGroup(toolbar_, "Editing");
	g_edit->addActionButton("pgfx_settrans", "");
	cb_colour_ = new ColourBox(g_edit, -1, ColRGBA::BLACK, false, true);
	cb_colour_->setPalette(gfx_canvas_->palette());
	button_brush_ = g_edit->addActionButton("pgfx_setbrush", "");
	g_edit->addCustomControl(cb_colour_);
	g_edit->addActionButton("pgfx_drag", "");
	g_edit->addActionButton("pgfx_draw", "");
	g_edit->addActionButton("pgfx_erase", "");
	g_edit->addActionButton("pgfx_magic", "");
	SAction::fromId("pgfx_drag")->setChecked(); // Drag offsets by default
	toolbar_->addGroup(g_edit);

	// Image operations
	SToolBarGroup* g_image = new SToolBarGroup(toolbar_, "Image");
	g_image->addActionButton("pgfx_mirror", "");
	g_image->addActionButton("pgfx_flip", "");
	g_image->addActionButton("pgfx_rotate", "");
	g_image->addActionButton("pgfx_crop", "");
	g_image->addActionButton("pgfx_convert", "");
	toolbar_->addGroup(g_image);

	// Colour operations
	SToolBarGroup* g_colour = new SToolBarGroup(toolbar_, "Colour");
	g_colour->addActionButton("pgfx_remap", "");
	g_colour->addActionButton("pgfx_colourise", "");
	g_colour->addActionButton("pgfx_tint", "");
	toolbar_->addGroup(g_colour);

	// Misc operations
	SToolBarGroup* g_png = new SToolBarGroup(toolbar_, "PNG");
	g_png->addActionButton("pgfx_pngopt", "");
	toolbar_->addGroup(g_png);
	toolbar_->enableGroup("PNG", false);
}

// -----------------------------------------------------------------------------
// Fills the brush menu with available brushes
// -----------------------------------------------------------------------------
void GfxEntryPanel::fillBrushMenu(wxMenu* bm) const
{
	SAction::fromId("pgfx_brush_sq_1")->addToMenu(bm);
	SAction::fromId("pgfx_brush_sq_3")->addToMenu(bm);
	SAction::fromId("pgfx_brush_sq_5")->addToMenu(bm);
	SAction::fromId("pgfx_brush_sq_7")->addToMenu(bm);
	SAction::fromId("pgfx_brush_sq_9")->addToMenu(bm);
	SAction::fromId("pgfx_brush_ci_5")->addToMenu(bm);
	SAction::fromId("pgfx_brush_ci_7")->addToMenu(bm);
	SAction::fromId("pgfx_brush_ci_9")->addToMenu(bm);
	SAction::fromId("pgfx_brush_di_3")->addToMenu(bm);
	SAction::fromId("pgfx_brush_di_5")->addToMenu(bm);
	SAction::fromId("pgfx_brush_di_7")->addToMenu(bm);
	SAction::fromId("pgfx_brush_di_9")->addToMenu(bm);
	auto pa = new wxMenu;
	SAction::fromId("pgfx_brush_pa_a")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_b")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_c")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_d")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_e")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_f")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_g")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_h")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_i")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_j")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_k")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_l")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_m")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_n")->addToMenu(pa);
	SAction::fromId("pgfx_brush_pa_o")->addToMenu(pa);
	bm->AppendSubMenu(pa, "Dither Patterns");
}

// -----------------------------------------------------------------------------
// Extract all sub-images as individual PNGs
// -----------------------------------------------------------------------------
bool GfxEntryPanel::extractAll() const
{
	if (getImage()->nImages() < 2)
		return false;

	// Remember where we are
	int imgindex = getImage()->imgIndex();

	Archive* parent = entry_->parent();
	if (parent == nullptr)
		return false;

	int index = parent->entryIndex(entry_, entry_->parentDir());

	// Loop through subimages and get things done
	int pos = 0;
	for (int i = 0; i < getImage()->nImages(); ++i)
	{
		string newname = S_FMT("%s_%i.png", entry_->nameNoExt(), i);
		Misc::loadImageFromEntry(getImage(), entry_, i);

		// Only process images that actually contain some pixels
		if (getImage()->width() && getImage()->height())
		{
			ArchiveEntry* newimg = parent->addNewEntry(newname, index + pos + 1, entry_->parentDir());
			if (newimg == nullptr)
				return false;
			SIFormat::getFormat("png")->saveImage(*getImage(), newimg->data(), gfx_canvas_->palette());
			EntryType::detectEntryType(newimg);
			pos++;
		}
	}

	// Reload image of where we were
	Misc::loadImageFromEntry(getImage(), entry_, imgindex);

	return true;
}

// -----------------------------------------------------------------------------
// Reloads image data and force refresh
// -----------------------------------------------------------------------------
void GfxEntryPanel::refresh()
{
	// Setup palette
	theMainWindow->paletteChooser()->setGlobalFromArchive(entry_->parent(), Misc::detectPaletteHack(entry_));
	updateImagePalette();

	// Set offset text boxes
	spin_xoffset_->SetValue(getImage()->offset().x);
	spin_yoffset_->SetValue(getImage()->offset().y);

	// Get some needed menu ids
	int MENU_GFXEP_PNGOPT      = SAction::fromId("pgfx_pngopt")->wxId();
	int MENU_GFXEP_ALPH        = SAction::fromId("pgfx_alph")->wxId();
	int MENU_GFXEP_TRNS        = SAction::fromId("pgfx_trns")->wxId();
	int MENU_GFXEP_EXTRACT     = SAction::fromId("pgfx_extract")->wxId();
	int MENU_GFXEP_TRANSLATE   = SAction::fromId("pgfx_remap")->wxId();
	int MENU_ARCHGFX_EXPORTPNG = SAction::fromId("arch_gfx_exportpng")->wxId();

	// Set PNG check menus
	if (this->entry_->type() != nullptr && this->entry_->type()->formatId() == "img_png")
	{
		// Check for alph
		alph_ = EntryOperations::getalPhChunk(this->entry_);
		menu_custom_->Enable(MENU_GFXEP_ALPH, true);
		menu_custom_->Check(MENU_GFXEP_ALPH, alph_);

		// Check for trns
		trns_ = EntryOperations::gettRNSChunk(this->entry_);
		menu_custom_->Enable(MENU_GFXEP_TRNS, true);
		menu_custom_->Check(MENU_GFXEP_TRNS, trns_);

		// Disable 'Export as PNG' (it already is :P)
		menu_custom_->Enable(MENU_ARCHGFX_EXPORTPNG, false);

		// Add 'Optimize PNG' option
		menu_custom_->Enable(MENU_GFXEP_PNGOPT, true);
		toolbar_->enableGroup("PNG", true);
	}
	else
	{
		menu_custom_->Enable(MENU_GFXEP_ALPH, false);
		menu_custom_->Enable(MENU_GFXEP_TRNS, false);
		menu_custom_->Check(MENU_GFXEP_ALPH, false);
		menu_custom_->Check(MENU_GFXEP_TRNS, false);
		menu_custom_->Enable(MENU_GFXEP_PNGOPT, false);
		menu_custom_->Enable(MENU_ARCHGFX_EXPORTPNG, true);
		toolbar_->enableGroup("PNG", false);
	}

	// Set multi-image format stuff thingies
	cur_index_ = getImage()->imgIndex();
	if (getImage()->nImages() > 1)
		menu_custom_->Enable(MENU_GFXEP_EXTRACT, true);
	else
		menu_custom_->Enable(MENU_GFXEP_EXTRACT, false);
	text_curimg_->SetLabel(S_FMT("Image %d/%d", cur_index_ + 1, getImage()->nImages()));

	// Update status bar in case image dimensions changed
	updateStatus();

	// Apply offset view type
	applyViewType();

	// Reset display offsets in graphics mode
	if (gfx_canvas_->viewType() != GfxCanvas::View::Sprite)
		gfx_canvas_->resetOffsets();

	// Setup custom menu
	if (getImage()->type() == SImage::Type::RGBA)
		menu_custom_->Enable(MENU_GFXEP_TRANSLATE, false);
	else
		menu_custom_->Enable(MENU_GFXEP_TRANSLATE, true);

	// Refresh the canvas
	gfx_canvas_->Refresh();
}

// -----------------------------------------------------------------------------
// Returns a string with extended editing/entry info for the status bar
// -----------------------------------------------------------------------------
string GfxEntryPanel::statusString()
{
	// Setup status string
	SImage* image  = getImage();
	string  status = S_FMT("%dx%d", image->width(), image->height());

	// Colour format
	if (image->type() == SImage::Type::RGBA)
		status += ", 32bpp";
	else
		status += ", 8bpp";

	// PNG stuff
	if (entry_->type()->formatId() == "img_png")
	{
		// alPh
		if (EntryOperations::getalPhChunk(entry_))
			status += ", alPh";

		// tRNS
		if (EntryOperations::gettRNSChunk(entry_))
			status += ", tRNS";
	}

	return status;
}

// -----------------------------------------------------------------------------
// Redraws the panel
// -----------------------------------------------------------------------------
void GfxEntryPanel::refreshPanel()
{
	Update();
	Refresh();
}

// -----------------------------------------------------------------------------
// Sets the gfx canvas' palette to what is selected in the palette chooser, and
// refreshes the gfx canvas
// -----------------------------------------------------------------------------
void GfxEntryPanel::updateImagePalette() const
{
	gfx_canvas_->setPalette(MainEditor::currentPalette());
	gfx_canvas_->updateImageTexture();
}

// -----------------------------------------------------------------------------
// Detects the offset view type of the current entry
// -----------------------------------------------------------------------------
GfxCanvas::View GfxEntryPanel::detectOffsetType() const
{
	if (!entry_)
		return GfxCanvas::View::Default;

	if (!entry_->parent())
		return GfxCanvas::View::Default;

	// Check what section of the archive the entry is in -- only PNGs or images
	// in the sprites section can be HUD or sprite
	bool is_sprite = ("sprites" == entry_->parent()->detectNamespace(entry_));
	bool is_png    = ("img_png" == entry_->type()->formatId());
	if (!is_sprite && !is_png)
		return GfxCanvas::View::Default;

	SImage* img = getImage();
	if (is_png && img->offset().x == 0 && img->offset().y == 0)
		return GfxCanvas::View::Default;

	int width        = img->width();
	int height       = img->height();
	int left         = -img->offset().x;
	int right        = left + width;
	int top          = -img->offset().y;
	int bottom       = top + height;
	int horiz_center = (left + right) / 2;

	// Determine sprite vs. HUD with a rough heuristic: give each one a
	// penalty, measuring how far (in pixels) the offsets are from the "ideal"
	// offsets for that type.  Lowest penalty wins.
	int sprite_penalty = 0;
	int hud_penalty    = 0;
	// The HUD is drawn with the origin in the top left, so HUD offsets
	// generally put the center of the screen (160, 100) above or inside the
	// top center of the sprite.
	hud_penalty += abs(horiz_center - 160);
	hud_penalty += abs(top - 100);
	// It's extremely unusual for the bottom of the sprite to be above 168,
	// which is where the weapon is cut off in fullscreen.  Extra penalty.
	if (bottom < 168)
		hud_penalty += (168 - bottom);
	// Sprites are drawn relative to the center of an object at floor height,
	// so the offsets generally put the origin (0, 0) near the vertical center
	// line and the bottom edge.  Some sprites are vertically centered whereas
	// some use a small bottom margin for feet, so split the difference and use
	// 1/4 up from the bottom.
	int bottom_quartile = (bottom * 3 + top) / 4;
	sprite_penalty += abs(bottom_quartile - 0);
	sprite_penalty += abs(horiz_center - 0);
	// It's extremely unusual for the sprite to not contain the origin, which
	// would draw it not touching its actual position.  Extra panalty for that,
	// though allow for a sprite that floats up to its own height above the
	// floor.
	if (top > 0)
		sprite_penalty += (top - 0);
	else if (bottom < 0 - height)
		sprite_penalty += (0 - height - bottom);

	// Sprites are more common than HUD, so in case of a tie, sprite wins
	if (sprite_penalty > hud_penalty)
		return GfxCanvas::View::Hud;
	else
		return GfxCanvas::View::Sprite;
}

// -----------------------------------------------------------------------------
// Sets the view type of the gfx canvas depending on what is selected in the
// offset type combo box
// -----------------------------------------------------------------------------
void GfxEntryPanel::applyViewType() const
{
	// Tile checkbox overrides offset type selection
	if (cb_tile_->IsChecked())
		gfx_canvas_->setViewType(GfxCanvas::View::Tiled);
	else
	{
		// Set gfx canvas view type depending on the offset combobox selection
		int sel = choice_offset_type_->GetSelection();
		switch (sel)
		{
		case 0: gfx_canvas_->setViewType(detectOffsetType()); break;
		case 1: gfx_canvas_->setViewType(GfxCanvas::View::Default); break;
		case 2: gfx_canvas_->setViewType(GfxCanvas::View::Sprite); break;
		case 3: gfx_canvas_->setViewType(GfxCanvas::View::Hud); break;
		default: break;
		}
	}

	// Refresh
	gfx_canvas_->Refresh();
}

// -----------------------------------------------------------------------------
// Handles the action [id].
// Returns true if the action was handled, false otherwise
// -----------------------------------------------------------------------------
bool GfxEntryPanel::handleAction(string_view id)
{
	// Don't handle actions if hidden
	if (!isActivePanel())
		return false;

	// We're only interested in "pgfx_" actions
	if (!StrUtil::startsWith(id, "pgfx_"))
		return false;

	// For pgfx_brush actions, the string after pgfx is a brush name
	if (StrUtil::startsWith(id, "pgfx_brush"))
	{
		gfx_canvas_->setBrush(theBrushManager->get(id));
		button_brush_->setIcon(StrUtil::afterFirst(id, '_'));
	}

	// Editing - drag mode
	else if (id == "pgfx_drag")
	{
		editing_ = false;
		gfx_canvas_->setEditingMode(GfxCanvas::EditMode::View);
	}

	// Editing - draw mode
	else if (id == "pgfx_draw")
	{
		editing_ = true;
		gfx_canvas_->setEditingMode(GfxCanvas::EditMode::Paint);
		gfx_canvas_->setPaintColour(cb_colour_->colour());
	}

	// Editing - erase mode
	else if (id == "pgfx_erase")
	{
		editing_ = true;
		gfx_canvas_->setEditingMode(GfxCanvas::EditMode::Erase);
	}

	// Editing - translate mode
	else if (id == "pgfx_magic")
	{
		editing_ = true;
		gfx_canvas_->setEditingMode(GfxCanvas::EditMode::Translate);
	}

	// Editing - set translation
	else if (id == "pgfx_settrans")
	{
		// Create translation editor dialog
		Palette*                pal = theMainWindow->paletteChooser()->selectedPalette();
		TranslationEditorDialog ted(theMainWindow, pal, " Colour Remap", getImage());

		// Create translation to edit
		ted.openTranslation(edit_translation_);

		// Show the dialog
		if (ted.ShowModal() == wxID_OK)
		{
			// Set the translation
			edit_translation_.copy(ted.getTranslation());
			gfx_canvas_->setTranslation(&edit_translation_);
		}
	}

	// Editing - set brush
	else if (id == "pgfx_setbrush")
	{
		wxPoint p = button_brush_->GetScreenPosition() -= GetScreenPosition();
		p.y += button_brush_->GetMaxHeight();
		PopupMenu(menu_brushes_, p);
	}

	// Mirror
	else if (id == "pgfx_mirror")
	{
		// Mirror X
		getImage()->mirror(false);

		// Update UI
		gfx_canvas_->updateImageTexture();
		gfx_canvas_->Refresh();

		// Update variables
		image_data_modified_ = true;
		setModified();
	}

	// Flip
	else if (id == "pgfx_flip")
	{
		// Mirror Y
		getImage()->mirror(true);

		// Update UI
		gfx_canvas_->updateImageTexture();
		gfx_canvas_->Refresh();

		// Update variables
		image_data_modified_ = true;
		setModified();
	}

	// Rotate
	else if (id == "pgfx_rotate")
	{
		// Prompt for rotation angle
		wxString angles[] = { "90", "180", "270" };
		int      choice   = wxGetSingleChoiceIndex("Select rotation angle", "Rotate", 3, angles, 0);

		// Rotate image
		switch (choice)
		{
		case 0: getImage()->rotate(90); break;
		case 1: getImage()->rotate(180); break;
		case 2: getImage()->rotate(270); break;
		default: break;
		}

		// Update UI
		gfx_canvas_->updateImageTexture();
		gfx_canvas_->Refresh();

		// Update variables
		image_data_modified_ = true;
		setModified();
	}

	// Translate
	else if (id == "pgfx_remap")
	{
		// Create translation editor dialog
		Palette*                pal = MainEditor::currentPalette();
		TranslationEditorDialog ted(theMainWindow, pal, " Colour Remap", gfx_canvas_->image());

		// Create translation to edit
		ted.openTranslation(prev_translation_);

		// Show the dialog
		if (ted.ShowModal() == wxID_OK)
		{
			// Apply translation to image
			getImage()->applyTranslation(&ted.getTranslation(), pal);

			// Update UI
			gfx_canvas_->updateImageTexture();
			gfx_canvas_->Refresh();

			// Update variables
			image_data_modified_ = true;
			gfx_canvas_->updateImageTexture();
			setModified();
			prev_translation_.copy(ted.getTranslation());
		}
	}

	// Colourise
	else if (id == "pgfx_colourise")
	{
		Palette*           pal = MainEditor::currentPalette();
		GfxColouriseDialog gcd(theMainWindow, entry_, pal);
		gcd.setColour(last_colour);

		// Show colourise dialog
		if (gcd.ShowModal() == wxID_OK)
		{
			// Colourise image
			getImage()->colourise(gcd.getColour(), pal);

			// Update UI
			gfx_canvas_->updateImageTexture();
			gfx_canvas_->Refresh();

			// Update variables
			image_data_modified_ = true;
			Refresh();
			setModified();
		}
		ColRGBA gcdcol = gcd.getColour();
		last_colour   = S_FMT("RGB(%d, %d, %d)", gcdcol.r, gcdcol.g, gcdcol.b);
	}

	// Tint
	else if (id == "pgfx_tint")
	{
		Palette*      pal = MainEditor::currentPalette();
		GfxTintDialog gtd(theMainWindow, entry_, pal);
		gtd.setValues(last_tint_colour, last_tint_amount);

		// Show tint dialog
		if (gtd.ShowModal() == wxID_OK)
		{
			// Tint image
			getImage()->tint(gtd.getColour(), gtd.getAmount(), pal);

			// Update UI
			gfx_canvas_->updateImageTexture();
			gfx_canvas_->Refresh();

			// Update variables
			image_data_modified_ = true;
			Refresh();
			setModified();
		}
		ColRGBA gtdcol    = gtd.getColour();
		last_tint_colour = S_FMT("RGB(%d, %d, %d)", gtdcol.r, gtdcol.g, gtdcol.b);
		last_tint_amount = (int)(gtd.getAmount() * 100.0);
	}

	// Crop
	else if (id == "pgfx_crop")
	{
		auto          image = getImage();
		auto          pal   = MainEditor::currentPalette();
		GfxCropDialog gcd(theMainWindow, image, pal);

		// Show crop dialog
		if (gcd.ShowModal() == wxID_OK)
		{
			// Prompt to adjust offsets
			auto crop = gcd.getCropRect();
			if (crop.tl.x > 0 || crop.tl.y > 0)
			{
				if (wxMessageBox(
						"Do you want to adjust the offsets? This will keep the graphic in the same relative "
						"position it was before cropping.",
						"Adjust Offsets?",
						wxYES_NO)
					== wxYES)
				{
					image->setXOffset(image->offset().x - crop.tl.x);
					image->setYOffset(image->offset().y - crop.tl.y);
				}
			}

			// Crop image
			image->crop(crop.x1(), crop.y1(), crop.x2(), crop.y2());

			// Update UI
			gfx_canvas_->updateImageTexture();
			gfx_canvas_->Refresh();

			// Update variables
			image_data_modified_ = true;
			Refresh();
			setModified();
		}
	}

	// alPh/tRNS
	else if (id == "pgfx_alph" || id == "pgfx_trns")
	{
		setModified();
		Refresh();
	}

	// Optimize PNG
	else if (id == "pgfx_pngopt")
	{
		// This is a special case. If we set the entry as modified, SLADE will prompt
		// to save it, rewriting the entry and cancelling the optimization done...
		if (EntryOperations::optimizePNG(entry_))
			setModified(false);
		else
			wxMessageBox(
				"Warning: Couldn't optimize this image, check console log for info",
				"Warning",
				wxOK | wxCENTRE | wxICON_WARNING);
		Refresh();
	}

	// Extract all
	else if (id == "pgfx_extract")
	{
		extractAll();
	}

	// Convert
	else if (id == "pgfx_convert")
	{
		GfxConvDialog gcd(theMainWindow);
		gcd.CenterOnParent();
		gcd.openEntry(entry_);

		gcd.ShowModal();

		if (gcd.itemModified(0))
		{
			// Get image and conversion info
			SImage*   image  = gcd.getItemImage(0);
			SIFormat* format = gcd.getItemFormat(0);

			// Write converted image back to entry
			format->saveImage(*image, entry_data_, gcd.getItemPalette(0));
			// This makes the "save" button (and the setModified stuff) redundant and confusing!
			// The alternative is to save to entry effectively (uncomment the importMemChunk line)
			// but remove the setModified and image_data_modified lines, and add a call to refresh
			// to get the PNG tRNS status back in sync.
			// entry->importMemChunk(entry_data);
			image_data_modified_ = true;
			setModified();

			// Fix tRNS status if we converted to paletted PNG
			int MENU_GFXEP_PNGOPT      = SAction::fromId("pgfx_pngopt")->wxId();
			int MENU_GFXEP_ALPH        = SAction::fromId("pgfx_alph")->wxId();
			int MENU_GFXEP_TRNS        = SAction::fromId("pgfx_trns")->wxId();
			int MENU_ARCHGFX_EXPORTPNG = SAction::fromId("arch_gfx_exportpng")->wxId();
			if (format->name() == "PNG")
			{
				ArchiveEntry temp;
				temp.importMemChunk(entry_data_);
				temp.setType(EntryType::fromId("png"));
				menu_custom_->Enable(MENU_GFXEP_ALPH, true);
				menu_custom_->Enable(MENU_GFXEP_TRNS, true);
				menu_custom_->Check(MENU_GFXEP_TRNS, EntryOperations::gettRNSChunk(&temp));
				menu_custom_->Enable(MENU_ARCHGFX_EXPORTPNG, false);
				menu_custom_->Enable(MENU_GFXEP_PNGOPT, true);
				toolbar_->enableGroup("PNG", true);
			}
			else
			{
				menu_custom_->Enable(MENU_GFXEP_ALPH, false);
				menu_custom_->Enable(MENU_GFXEP_TRNS, false);
				menu_custom_->Enable(MENU_ARCHGFX_EXPORTPNG, true);
				menu_custom_->Enable(MENU_GFXEP_PNGOPT, false);
				toolbar_->enableGroup("PNG", false);
			}

			// Refresh
			getImage()->open(entry_data_, 0, format->id());
			gfx_canvas_->Refresh();
		}
	}

	// Unknown action
	else
		return false;

	// Action handled
	return true;
}

// -----------------------------------------------------------------------------
// Fills the given menu with the panel's custom actions. Used by both the
// constructor to create the main window's custom menu, and
// ArchivePanel::onEntryListRightClick to fill the context menu with
// context-appropriate stuff
// -----------------------------------------------------------------------------
bool GfxEntryPanel::fillCustomMenu(wxMenu* custom)
{
	SAction::fromId("pgfx_mirror")->addToMenu(custom);
	SAction::fromId("pgfx_flip")->addToMenu(custom);
	SAction::fromId("pgfx_rotate")->addToMenu(custom);
	SAction::fromId("pgfx_convert")->addToMenu(custom);
	custom->AppendSeparator();
	SAction::fromId("pgfx_remap")->addToMenu(custom);
	SAction::fromId("pgfx_colourise")->addToMenu(custom);
	SAction::fromId("pgfx_tint")->addToMenu(custom);
	SAction::fromId("pgfx_crop")->addToMenu(custom);
	custom->AppendSeparator();
	SAction::fromId("pgfx_alph")->addToMenu(custom);
	SAction::fromId("pgfx_trns")->addToMenu(custom);
	SAction::fromId("pgfx_pngopt")->addToMenu(custom);
	custom->AppendSeparator();
	SAction::fromId("arch_gfx_exportpng")->addToMenu(custom);
	SAction::fromId("pgfx_extract")->addToMenu(custom);
	custom->AppendSeparator();
	SAction::fromId("arch_gfx_addptable")->addToMenu(custom);
	SAction::fromId("arch_gfx_addtexturex")->addToMenu(custom);
	// TODO: Should change the way gfx conversion and offset modification work so I can put them in this menu

	return true;
}


// -----------------------------------------------------------------------------
//
// GfxEntryPanel Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when the colour box's value is changed
// -----------------------------------------------------------------------------
void GfxEntryPanel::onPaintColourChanged(wxEvent& e)
{
	gfx_canvas_->setPaintColour(cb_colour_->colour());
}

// -----------------------------------------------------------------------------
// Called when the X offset value is modified
// -----------------------------------------------------------------------------
void GfxEntryPanel::onXOffsetChanged(wxCommandEvent& e)
{
	// Ignore if the value wasn't changed
	int offset = spin_xoffset_->GetValue();
	if (offset == getImage()->offset().x)
		return;

	// Update offset & refresh
	getImage()->setXOffset(offset);
	setModified();
	gfx_canvas_->Refresh();
}

// -----------------------------------------------------------------------------
// Called when the Y offset value is modified
// -----------------------------------------------------------------------------
void GfxEntryPanel::onYOffsetChanged(wxCommandEvent& e)
{
	// Ignore if the value wasn't changed
	int offset = spin_yoffset_->GetValue();
	if (offset == getImage()->offset().y)
		return;

	// Update offset & refresh
	getImage()->setYOffset(offset);
	setModified();
	gfx_canvas_->Refresh();
}

// -----------------------------------------------------------------------------
// Called when the 'type' combo box selection is changed
// -----------------------------------------------------------------------------
void GfxEntryPanel::onOffsetTypeChanged(wxCommandEvent& e)
{
	applyViewType();
}

// -----------------------------------------------------------------------------
// Called when the 'Tile' checkbox is checked/unchecked
// -----------------------------------------------------------------------------
void GfxEntryPanel::onTileChanged(wxCommandEvent& e)
{
	choice_offset_type_->Enable(!cb_tile_->IsChecked());
	applyViewType();
}

// -----------------------------------------------------------------------------
// Called when the 'Aspect Ratio' checkbox is checked/unchecked
// -----------------------------------------------------------------------------
void GfxEntryPanel::onARCChanged(wxCommandEvent& e)
{
	gfx_arc = cb_arc_->IsChecked();
	gfx_canvas_->Refresh();
}

// -----------------------------------------------------------------------------
// Called when the gfx canvas image offsets are changed
// -----------------------------------------------------------------------------
void GfxEntryPanel::onGfxOffsetChanged(wxEvent& e)
{
	// Update spin controls
	spin_xoffset_->SetValue(getImage()->offset().x);
	spin_yoffset_->SetValue(getImage()->offset().y);

	// Set changed
	setModified();
}

// -----------------------------------------------------------------------------
// Called when pixels are changed in the canvas
// -----------------------------------------------------------------------------
void GfxEntryPanel::onGfxPixelsChanged(wxEvent& e)
{
	// Set changed
	image_data_modified_ = true;
	setModified();
}

// -----------------------------------------------------------------------------
// Handles any announcements
// -----------------------------------------------------------------------------
void GfxEntryPanel::onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data)
{
	if (announcer != theMainWindow->paletteChooser())
		return;

	if (event_name == "main_palette_changed")
	{
		updateImagePalette();
		gfx_canvas_->Refresh();
	}
}

// -----------------------------------------------------------------------------
// Called when the 'next image' button is clicked
// -----------------------------------------------------------------------------
void GfxEntryPanel::onBtnNextImg(wxCommandEvent& e)
{
	int num = gfx_canvas_->image()->nImages();
	if (num > 1)
	{
		if (cur_index_ < num - 1)
			loadEntry(entry_, cur_index_ + 1);
		else
			loadEntry(entry_, 0);
	}
}

// -----------------------------------------------------------------------------
// Called when the 'previous image' button is clicked
// -----------------------------------------------------------------------------
void GfxEntryPanel::onBtnPrevImg(wxCommandEvent& e)
{
	int num = gfx_canvas_->image()->nImages();
	if (num > 1)
	{
		if (cur_index_ > 0)
			loadEntry(entry_, cur_index_ - 1);
		else
			loadEntry(entry_, num - 1);
	}
}

// -----------------------------------------------------------------------------
// Called when the 'modify offsets' button is clicked
// -----------------------------------------------------------------------------
void GfxEntryPanel::onBtnAutoOffset(wxCommandEvent& e)
{
	ModifyOffsetsDialog dlg;
	dlg.SetParent(theMainWindow);
	dlg.CenterOnParent();
	if (dlg.ShowModal() == wxID_OK)
	{
		// Calculate new offsets
		point2_t offsets = dlg.calculateOffsets(
			spin_xoffset_->GetValue(),
			spin_yoffset_->GetValue(),
			gfx_canvas_->image()->width(),
			gfx_canvas_->image()->height());

		// Change offsets
		spin_xoffset_->SetValue(offsets.x);
		spin_yoffset_->SetValue(offsets.y);
		getImage()->setXOffset(offsets.x);
		getImage()->setYOffset(offsets.y);
		refreshPanel();

		// Set changed
		setModified();
	}
}

// -----------------------------------------------------------------------------
// Called when a pixel's colour has been picked on the canvas
// -----------------------------------------------------------------------------
void GfxEntryPanel::onColourPicked(wxEvent& e)
{
	cb_colour_->setColour(gfx_canvas_->paintColour());
}



// -----------------------------------------------------------------------------
//
// Console Commands
//
// -----------------------------------------------------------------------------

// I'd love to put them in their own file, but attempting to do so
// results in a circular include nightmare and nothing works anymore.
#include "General/Console/Console.h"
#include "MainEditor/UI/ArchivePanel.h"

GfxEntryPanel* CH::getCurrentGfxPanel()
{
	EntryPanel* panel = MainEditor::currentEntryPanel();
	if (panel)
	{
		if (StrUtil::equalCI(panel->name(), "gfx"))
		{
			return (GfxEntryPanel*)panel;
		}
	}
	return nullptr;
}

CONSOLE_COMMAND(rotate, 1, true)
{
	double   val;
	wxString bluh = args[0];
	if (!bluh.ToDouble(&val))
	{
		if (!bluh.CmpNoCase("l") || !bluh.CmpNoCase("left"))
			val = 90.;
		else if (!bluh.CmpNoCase("f") || !bluh.CmpNoCase("flip"))
			val = 180.;
		else if (!bluh.CmpNoCase("r") || !bluh.CmpNoCase("right"))
			val = 270.;
		else
		{
			LOG_MESSAGE(1, "Invalid parameter: %s is not a number.", bluh.mb_str());
			return;
		}
	}
	int angle = (int)val;
	if (angle % 90)
	{
		LOG_MESSAGE(1, "Invalid parameter: %i is not a multiple of 90.", angle);
		return;
	}

	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}

	// Get current entry
	ArchiveEntry* entry = MainEditor::currentEntry();

	if (meep->getImage())
	{
		meep->getImage()->rotate(angle);
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
}

CONSOLE_COMMAND(mirror, 1, true)
{
	bool     vertical;
	wxString bluh = args[0];
	if (!bluh.CmpNoCase("y") || !bluh.CmpNoCase("v") || !bluh.CmpNoCase("vert") || !bluh.CmpNoCase("vertical"))
		vertical = true;
	else if (!bluh.CmpNoCase("x") || !bluh.CmpNoCase("h") || !bluh.CmpNoCase("horz") || !bluh.CmpNoCase("horizontal"))
		vertical = false;
	else
	{
		LOG_MESSAGE(1, "Invalid parameter: %s is not a known value.", bluh.mb_str());
		return;
	}
	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}
	if (meep->getImage())
	{
		meep->getImage()->mirror(vertical);
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
}

CONSOLE_COMMAND(crop, 4, true)
{
	// long x1, y1, x2, y2;

	int x1, y1, x2, y2;
	try
	{
		x1 = std::stoi(args[0]);
		y1 = std::stoi(args[1]);
		x2 = std::stoi(args[2]);
		y2 = std::stoi(args[3]);
	}
	catch (std::exception& ex)
	{
		return;
	}

	// if (args[0].ToLong(&x1) && args[1].ToLong(&y1) && args[2].ToLong(&x2) && args[3].ToLong(&y2))
	//{
	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	if (meep->getImage())
	{
		meep->getImage()->crop(x1, y1, x2, y2);
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
	//}
}

CONSOLE_COMMAND(adjust, 0, true)
{
	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	if (meep->getImage())
	{
		meep->getImage()->adjust();
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
}

CONSOLE_COMMAND(mirrorpad, 0, true)
{
	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	if (meep->getImage())
	{
		meep->getImage()->mirrorpad();
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
}

CONSOLE_COMMAND(imgconv, 0, true)
{
	ArchivePanel* foo = CH::getCurrentArchivePanel();
	if (!foo)
	{
		LOG_MESSAGE(1, "No active panel.");
		return;
	}
	ArchiveEntry* bar = foo->currentEntry();
	if (!bar)
	{
		LOG_MESSAGE(1, "No active entry.");
		return;
	}
	GfxEntryPanel* meep = CH::getCurrentGfxPanel();
	if (!meep)
	{
		LOG_MESSAGE(1, "No image selected.");
		return;
	}
	if (meep->getImage())
	{
		meep->getImage()->imgconv();
		meep->refresh();
		MemChunk mc;
		if (meep->getImage()->format()->saveImage(*meep->getImage(), mc))
			bar->importMemChunk(mc);
	}
}
