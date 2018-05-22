
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    SectorInfoOverlay.cpp
// Description: SectorInfoOverlay class - a map editor overlay that displays
//              information about the currently highlighted sector in sectors
//              mode
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
#include "SectorInfoOverlay.h"
#include "MapEditor/SLADEMap/MapSector.h"
#include "OpenGL/Drawing.h"
#include "MapEditor/MapEditor.h"
#include "General/ColourConfiguration.h"
#include "Game/Configuration.h"
#include "OpenGL/OpenGL.h"
#include "MapEditor/MapTextureManager.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// SectorInfoOverlay Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// SectorInfoOverlay class constructor
// -----------------------------------------------------------------------------
 SectorInfoOverlay::SectorInfoOverlay() :
text_box_{ "", Drawing::Font::Condensed, 100, 16 * (Drawing::fontSize() / 12.0) }
{
}

// -----------------------------------------------------------------------------
// Updates the overlay with info from [sector]
// -----------------------------------------------------------------------------
void SectorInfoOverlay::update(MapSector* sector)
{
	if (!sector)
		return;

	string info_text;

	// Info (index + type)
	int t = sector->intProperty("special");
	string type = fmt::sprintf("%s (Type %d)", Game::configuration().sectorTypeName(t), t);
	if (Global::debug)
		info_text += fmt::sprintf("Sector #%d (%d): %s\n", sector->index(), sector->objId(), type);
	else
		info_text += fmt::sprintf("Sector #%d: %s\n", sector->index(), type);

	// Height
	int fh = sector->intProperty("heightfloor");
	int ch = sector->intProperty("heightceiling");
	info_text += fmt::sprintf("Height: %d to %d (%d total)\n", fh, ch, ch - fh);

	// Brightness
	info_text += fmt::sprintf("Brightness: %d\n", sector->intProperty("lightlevel"));

	// Tag
	info_text += fmt::sprintf("Tag: %d", sector->intProperty("id"));

	// Textures
	ftex_ = sector->floor().texture;
	ctex_ = sector->ceiling().texture;

	// Setup text box
	text_box_.setText(info_text);
}

// -----------------------------------------------------------------------------
// Draws the overlay at [bottom] from 0 to [right]
// -----------------------------------------------------------------------------
void SectorInfoOverlay::draw(int bottom, int right, float alpha)
{
	// Don't bother if invisible
	if (alpha <= 0.0f)
		return;

	// Init GL stuff
	glLineWidth(1.0f);
	glDisable(GL_LINE_SMOOTH);

	// Determine overlay height
	double scale = (Drawing::fontSize() / 12.0);
	text_box_.setLineHeight(16 * scale);
	if (last_size_ != right)
	{
		last_size_ = right;
		text_box_.setSize(right - 88 - 92);
	}
	int height = text_box_.getHeight() + 4;

	// Slide in/out animation
	float alpha_inv = 1.0f - alpha;
	bottom += height*alpha_inv*alpha_inv;

	// Get colours
	ColRGBA col_bg = ColourConfiguration::colour("map_overlay_background");
	ColRGBA col_fg = ColourConfiguration::colour("map_overlay_foreground");
	col_fg.a = col_fg.a*alpha;
	col_bg.a = col_bg.a*alpha;
	ColRGBA col_border(0, 0, 0, 140);

	// Draw overlay background
	int tex_box_size = 80 * scale;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Drawing::drawBorderedRect(0, bottom - height - 4, right - (tex_box_size * 2) - 30, bottom+2, col_bg, col_border);
	Drawing::drawBorderedRect(right - (tex_box_size * 2) - 28, bottom - height - 4, right, bottom+2, col_bg, col_border);

	// Draw info text lines
	text_box_.draw(2, bottom - height, col_fg);

	// Ceiling texture
	drawTexture(alpha, right - tex_box_size - 8, bottom - 4, ctex_, "C");

	// Floor texture
	drawTexture(alpha, right - (tex_box_size * 2) - 20, bottom - 4, ftex_, "F");

	// Done
	glEnable(GL_LINE_SMOOTH);
}

// -----------------------------------------------------------------------------
// Draws a texture box with name underneath for [texture]
// -----------------------------------------------------------------------------
void SectorInfoOverlay::drawTexture(float alpha, int x, int y, string_view texture, string_view pos) const
{
	double scale = (Drawing::fontSize() / 12.0);
	int tex_box_size = 80 * scale;
	int line_height = 16 * scale;

	// Get colours
	ColRGBA col_bg = ColourConfiguration::colour("map_overlay_background");
	ColRGBA col_fg = ColourConfiguration::colour("map_overlay_foreground");
	col_fg.a = col_fg.a*alpha;

	// Get texture
	GLTexture* tex = MapEditor::textureManager().flat(
		texture,
		Game::configuration().featureSupported(Game::Feature::MixTexFlats)
	);

	// Valid texture
	if (texture != "-" && tex != &(GLTexture::missingTex()))
	{
		// Draw background
		glEnable(GL_TEXTURE_2D);
		OpenGL::setColour(255, 255, 255, 255*alpha, 0);
		glPushMatrix();
		glTranslated(x, y - tex_box_size - line_height, 0);
		GLTexture::bgTex().draw2dTiled(tex_box_size, tex_box_size);
		glPopMatrix();

		// Draw texture
		OpenGL::setColour(255, 255, 255, 255*alpha, 0);
		Drawing::drawTextureWithin(tex, x, y - tex_box_size - line_height, x + tex_box_size, y - line_height, 0);

		glDisable(GL_TEXTURE_2D);

		// Draw outline
		OpenGL::setColour(col_fg.r, col_fg.g, col_fg.b, 255*alpha, 0);
		glDisable(GL_LINE_SMOOTH);
		Drawing::drawRect(x, y - tex_box_size - line_height, x + tex_box_size, y - line_height);
	}

	// Unknown texture
	else if (tex == &(GLTexture::missingTex()))
	{
		// Draw unknown icon
		GLTexture* icon = MapEditor::textureManager().editorImage("thing/unknown");
		glEnable(GL_TEXTURE_2D);
		OpenGL::setColour(180, 0, 0, 255*alpha, 0);
		Drawing::drawTextureWithin(icon, x, y - tex_box_size - line_height, x + tex_box_size, y - line_height, 0, 0.15);

		// Set colour to red (for text)
		col_fg = col_fg.ampf(1.0f, 0.0f, 0.0f, 1.0f);
	}

	// Draw texture name
	string texname = StrUtil::truncate(texture, 8);
	StrUtil::prependIP(texname, ":");
	StrUtil::prependIP(texname, pos);
	Drawing::drawText(texname, x + (tex_box_size * 0.5), y - line_height, col_fg, Drawing::Font::Condensed, Drawing::Align::Center);
}
