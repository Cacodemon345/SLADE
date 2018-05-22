
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    SidePropsPanel.cpp
// Description: UI for editing side properties (textures and offsets)
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
#include "Game/Configuration.h"
#include "MapEditor/MapEditContext.h"
#include "MapEditor/MapEditor.h"
#include "MapEditor/MapTextureManager.h"
#include "MapEditor/SLADEMap/SLADEMap.h"
#include "MapEditor/UI/Dialogs/MapTextureBrowser.h"
#include "OpenGL/Drawing.h"
#include "OpenGL/GLTexture.h"
#include "SidePropsPanel.h"
#include "UI/Controls/NumberTextCtrl.h"
#include "UI/WxUtils.h"


// -----------------------------------------------------------------------------
// SideTexCanvas Class Functions
//
// A simple opengl canvas to display a texture
// (will have more advanced functionality later)
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// SideTexCanvas class constructor
// -----------------------------------------------------------------------------
SideTexCanvas::SideTexCanvas(wxWindow* parent) : OGLCanvas(parent, -1)
{
	wxWindow::SetWindowStyleFlag(wxBORDER_SIMPLE);
	SetInitialSize(WxUtils::scaledSize(136, 136));
}

// -----------------------------------------------------------------------------
// Sets the texture to display
// -----------------------------------------------------------------------------
void SideTexCanvas::setTexture(string_view tex)
{
	S_SET_VIEW(texname_, tex);
	if (tex == "-" || tex.empty())
		texture_ = nullptr;
	else
		texture_ = MapEditor::textureManager().texture(
			tex, Game::configuration().featureSupported(Game::Feature::MixTexFlats));

	Refresh();
}

// -----------------------------------------------------------------------------
// Draws the canvas content
// -----------------------------------------------------------------------------
void SideTexCanvas::draw()
{
	// Setup the viewport
	glViewport(0, 0, GetSize().x, GetSize().y);

	// Setup the screen projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, GetSize().x, GetSize().y, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Clear
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Translate to inside of pixel (otherwise inaccuracies can occur on certain gl implementations)
	if (OpenGL::accuracyTweak())
		glTranslatef(0.375f, 0.375f, 0);

	// Draw background
	drawCheckeredBackground();

	// Draw texture
	if (texture_ && texture_ != &(GLTexture::missingTex()))
	{
		glEnable(GL_TEXTURE_2D);
		Drawing::drawTextureWithin(texture_, 0, 0, GetSize().x, GetSize().y, 0);
	}
	else if (texture_ == &(GLTexture::missingTex()))
	{
		// Draw unknown icon
		GLTexture* tex = MapEditor::textureManager().editorImage("thing/unknown");
		glEnable(GL_TEXTURE_2D);
		OpenGL::setColour(180, 0, 0);
		Drawing::drawTextureWithin(tex, 0, 0, GetSize().x, GetSize().y, 0, 0.25);
	}

	// Swap buffers (ie show what was drawn)
	SwapBuffers();
}


// -----------------------------------------------------------------------------
// TextureComboBox Class Functions
//
// A custom combo box that will show a list of textures matching the current
// text in the control (eg. 'BIG' will list all textures beginning with BIG).
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// TextureComboBox class constructor
// -----------------------------------------------------------------------------
TextureComboBox::TextureComboBox(wxWindow* parent) : wxComboBox(parent, -1)
{
	// Init
	SetInitialSize(WxUtils::scaledSize(136, -1));
	wxArrayString list;
	list.Add("-");

	// Bind events
	Bind(wxEVT_COMBOBOX_DROPDOWN, &TextureComboBox::onDropDown, this);
	Bind(wxEVT_COMBOBOX_CLOSEUP, &TextureComboBox::onCloseUp, this);
	Bind(wxEVT_KEY_DOWN, &TextureComboBox::onKeyDown, this);

	Set(list);
}


// -----------------------------------------------------------------------------
//
// TextureComboBox Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when the dropdown list is expanded
// -----------------------------------------------------------------------------
void TextureComboBox::onDropDown(wxCommandEvent& e)
{
	// Get current value
	auto text = GetValue().Upper();
	if (text == "-")
		text.Clear();
	auto text_view = WxUtils::stringToView(text);

	// Populate dropdown with matching texture names
	auto& textures = MapEditor::textureManager().allTexturesInfo();
	wxArrayString          list;
	list.Add("-");
	for (auto& texture : textures)
	{
		if (StrUtil::startsWith(texture.name, text_view))
			list.Add(texture.name);
	}
	Set(list); // Why does this clear the text box also?
	SetValue(text);

	e.Skip();
}

// -----------------------------------------------------------------------------
// Called when the dropdown list is closed
// -----------------------------------------------------------------------------
void TextureComboBox::onCloseUp(wxCommandEvent& e)
{
	list_down_ = false;
}

// -----------------------------------------------------------------------------
// Called when a key is pressed within the control
// -----------------------------------------------------------------------------
void TextureComboBox::onKeyDown(wxKeyEvent& e)
{
	if (e.GetKeyCode() == WXK_DOWN && !list_down_)
	{
		list_down_ = true;
		Popup();
	}
	else
		e.Skip();
}


// -----------------------------------------------------------------------------
//
// SidePropsPanel Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// SidePropsPanel class constructor
// -----------------------------------------------------------------------------
SidePropsPanel::SidePropsPanel(wxWindow* parent) : wxPanel(parent, -1)
{
	wxBoxSizer* vbox;

	// Setup sizer
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	// --- Textures ---
	wxStaticBoxSizer* sizer_tex = new wxStaticBoxSizer(wxVERTICAL, this, "Textures");
	sizer->Add(sizer_tex, 0, wxEXPAND);

	wxGridBagSizer* gb_sizer = new wxGridBagSizer(UI::pad(), UI::pad());
	sizer_tex->Add(gb_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, UI::pad());

	// Upper
	auto pad_min = UI::px(UI::Size::PadMinimum);
	gb_sizer->Add(vbox = new wxBoxSizer(wxVERTICAL), { 0, 0 }, { 1, 1 }, wxALIGN_CENTER);
	vbox->Add(new wxStaticText(this, -1, "Upper:"), 0, wxALIGN_CENTER | wxBOTTOM, pad_min);
	vbox->Add(gfx_upper_ = new SideTexCanvas(this), 1, wxEXPAND);
	gb_sizer->Add(tcb_upper_ = new TextureComboBox(this), { 1, 0 }, { 1, 1 }, wxALIGN_CENTER);

	// Middle
	gb_sizer->Add(vbox = new wxBoxSizer(wxVERTICAL), { 0, 1 }, { 1, 1 }, wxALIGN_CENTER);
	vbox->Add(new wxStaticText(this, -1, "Middle:"), 0, wxALIGN_CENTER | wxBOTTOM, pad_min);
	vbox->Add(gfx_middle_ = new SideTexCanvas(this), 1, wxEXPAND);
	gb_sizer->Add(tcb_middle_ = new TextureComboBox(this), { 1, 1 }, { 1, 1 }, wxALIGN_CENTER);

	// Lower
	gb_sizer->Add(vbox = new wxBoxSizer(wxVERTICAL), { 0, 2 }, { 1, 1 }, wxALIGN_CENTER);
	vbox->Add(new wxStaticText(this, -1, "Lower:"), 0, wxALIGN_CENTER | wxBOTTOM, pad_min);
	vbox->Add(gfx_lower_ = new SideTexCanvas(this), 1, wxEXPAND);
	gb_sizer->Add(tcb_lower_ = new TextureComboBox(this), { 1, 2 }, { 1, 1 }, wxALIGN_CENTER);

	gb_sizer->AddGrowableCol(0, 1);
	gb_sizer->AddGrowableCol(1, 1);
	gb_sizer->AddGrowableCol(2, 1);
	gb_sizer->AddGrowableRow(0, 1);


	// --- Offsets ---
	auto layout_offsets = WxUtils::layoutVertically(
		vector<wxObject*>{ WxUtils::createLabelHBox(this, "X Offset:", text_offsetx_ = new NumberTextCtrl(this)),
						   WxUtils::createLabelHBox(this, "Y Offset:", text_offsety_ = new NumberTextCtrl(this)) });
	sizer->Add(layout_offsets, 0, wxEXPAND | wxTOP, UI::pad());

	// Bind events
	tcb_upper_->Bind(wxEVT_TEXT, &SidePropsPanel::onTextureChanged, this);
	tcb_middle_->Bind(wxEVT_TEXT, &SidePropsPanel::onTextureChanged, this);
	tcb_lower_->Bind(wxEVT_TEXT, &SidePropsPanel::onTextureChanged, this);
	gfx_upper_->Bind(wxEVT_LEFT_DOWN, &SidePropsPanel::onTextureClicked, this);
	gfx_middle_->Bind(wxEVT_LEFT_DOWN, &SidePropsPanel::onTextureClicked, this);
	gfx_lower_->Bind(wxEVT_LEFT_DOWN, &SidePropsPanel::onTextureClicked, this);
#ifdef __WXOSX__
	tcb_upper_->Bind(wxEVT_COMBOBOX, &SidePropsPanel::onTextureChanged, this);
	tcb_middle_->Bind(wxEVT_COMBOBOX, &SidePropsPanel::onTextureChanged, this);
	tcb_lower_->Bind(wxEVT_COMBOBOX, &SidePropsPanel::onTextureChanged, this);
#endif
}

// -----------------------------------------------------------------------------
// Loads textures and offsets from [sides]
// -----------------------------------------------------------------------------
void SidePropsPanel::openSides(vector<MapSide*>& sides) const
{
	if (sides.empty())
		return;

	// --- Textures ---

	// Upper
	string tex_upper = sides[0]->texUpper();
	for (unsigned a = 1; a < sides.size(); a++)
	{
		if (sides[a]->texUpper() != tex_upper)
		{
			tex_upper = "";
			break;
		}
	}
	gfx_upper_->setTexture(tex_upper);
	tcb_upper_->SetValue(tex_upper);

	// Middle
	string tex_middle = sides[0]->texMiddle();
	for (unsigned a = 1; a < sides.size(); a++)
	{
		if (sides[a]->texMiddle() != tex_middle)
		{
			tex_middle = "";
			break;
		}
	}
	gfx_middle_->setTexture(tex_middle);
	tcb_middle_->SetValue(tex_middle);

	// Lower
	string tex_lower = sides[0]->texLower();
	for (unsigned a = 1; a < sides.size(); a++)
	{
		if (sides[a]->texLower() != tex_lower)
		{
			tex_lower = "";
			break;
		}
	}
	gfx_lower_->setTexture(tex_lower);
	tcb_lower_->SetValue(tex_lower);


	// --- Offsets ---

	// X
	bool multi = false;
	int  ofs   = sides[0]->offsetX();
	for (unsigned a = 1; a < sides.size(); a++)
	{
		if (sides[a]->offsetX() != ofs)
		{
			multi = true;
			break;
		}
	}
	if (!multi)
		text_offsetx_->SetValue(wxString::Format("%d", ofs));

	// Y
	multi = false;
	ofs   = sides[0]->offsetY();
	for (unsigned a = 1; a < sides.size(); a++)
	{
		if (sides[a]->offsetY() != ofs)
		{
			multi = true;
			break;
		}
	}
	if (!multi)
		text_offsety_->SetValue(wxString::Format("%d", ofs));
}

// -----------------------------------------------------------------------------
// Applies current values to [sides]
// -----------------------------------------------------------------------------
void SidePropsPanel::applyTo(vector<MapSide*>& sides) const
{
	// Get values
	string tex_upper  = tcb_upper_->GetValue().ToStdString();
	string tex_middle = tcb_middle_->GetValue().ToStdString();
	string tex_lower  = tcb_lower_->GetValue().ToStdString();

	for (auto& side : sides)
	{
		// Upper Texture
		if (!tex_upper.empty())
			side->setStringProperty("texturetop", tex_upper);

		// Middle Texture
		if (!tex_middle.empty())
			side->setStringProperty("texturemiddle", tex_middle);

		// Lower Texture
		if (!tex_lower.empty())
			side->setStringProperty("texturebottom", tex_lower);

		// X Offset
		if (!text_offsetx_->GetValue().IsEmpty())
			side->setIntProperty("offsetx", text_offsetx_->getNumber(side->offsetX()));

		// Y Offset
		if (!text_offsety_->GetValue().IsEmpty())
			side->setIntProperty("offsety", text_offsety_->getNumber(side->offsetY()));
	}
}


// -----------------------------------------------------------------------------
//
// SidePropsPanel Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when a texture name is changed
// -----------------------------------------------------------------------------
void SidePropsPanel::onTextureChanged(wxCommandEvent& e)
{
	// Upper
	if (e.GetEventObject() == tcb_upper_)
		gfx_upper_->setTexture(WxUtils::stringToView(tcb_upper_->GetValue()));

	// Middle
	else if (e.GetEventObject() == tcb_middle_)
		gfx_middle_->setTexture(WxUtils::stringToView(tcb_middle_->GetValue()));

	// Lower
	else if (e.GetEventObject() == tcb_lower_)
		gfx_lower_->setTexture(WxUtils::stringToView(tcb_lower_->GetValue()));

	e.Skip();
}

// -----------------------------------------------------------------------------
// Called when a texture canvas is clicked
// -----------------------------------------------------------------------------
void SidePropsPanel::onTextureClicked(wxMouseEvent& e)
{
	// Get canvas
	SideTexCanvas*   stc = nullptr;
	TextureComboBox* tcb = nullptr;
	if (e.GetEventObject() == gfx_upper_)
	{
		stc = gfx_upper_;
		tcb = tcb_upper_;
	}
	else if (e.GetEventObject() == gfx_middle_)
	{
		stc = gfx_middle_;
		tcb = tcb_middle_;
	}
	else if (e.GetEventObject() == gfx_lower_)
	{
		stc = gfx_lower_;
		tcb = tcb_lower_;
	}

	if (!stc)
	{
		e.Skip();
		return;
	}

	// Browse
	MapTextureBrowser browser(this, 0, stc->texName(), &(MapEditor::editContext().map()));
	if (browser.ShowModal() == wxID_OK)
		tcb->SetValue(browser.getSelectedItem()->name());
}
