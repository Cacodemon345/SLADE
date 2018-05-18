
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    SFont.cpp
// Description: SFont class, encapsulates a bitmap font, with functions to read
//              various bitmap font formats and draw characters and strings with
//              the font in opengl
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
#include "SFont.h"
#include "Archive/ArchiveManager.h"
#include "Graphics/SImage/SImage.h"
#include "OpenGL/OpenGL.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
SFont SFont::font_vga_;
SFont SFont::font_slade_;


// -----------------------------------------------------------------------------
//
// SFont Class Functions
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Loads a Doom alpha HUFONT font. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool SFont::loadFont0(MemChunk& mc)
{
	return false;
}

// -----------------------------------------------------------------------------
// Loads a ZDoom FON1 font. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool SFont::loadFont1(MemChunk& mc)
{
	return false;
}

// -----------------------------------------------------------------------------
// Loads a ZDoom FON2 font. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool SFont::loadFont2(MemChunk& mc)
{
	return false;
}

// -----------------------------------------------------------------------------
// Loads a monochrome vga font. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool SFont::loadFontM(MemChunk& mc)
{
	// Check data
	if (mc.size() == 0 || mc.size() % 256)
		return false;

	// Init variables
	unsigned width  = 256;
	unsigned height = 256;
	SImage   image;

	// Determine character size
	unsigned charwidth  = 8;
	unsigned charheight = mc.size() >> 8;

	// Setup font properties
	line_height_ = charheight + 1;
	spacing_     = 1;

	// Setup image
	image.resize(width, height);

	// Draw characters to image
	unsigned xoff = 0;
	unsigned yoff = 0;
	unsigned i    = 0;
	bool     test = true;
	for (auto& character : characters_)
	{
		// Setup character info
		character.tex_bounds.set(xoff, yoff, xoff + charwidth, yoff + charheight);
		character.width  = charwidth;
		character.height = charheight;

		for (unsigned row = 0; row < charheight; row++)
		{
			for (unsigned p = 0; p < charwidth; ++p)
			{
				uint8_t val = ((mc[i] >> (7 - p)) & 1) * 255;
				image.setPixel(xoff + p, yoff + row, ColRGBA(val, val, val, val));
			}
			i++;
		}

		// Go to next character
		xoff += charwidth;
		if (xoff >= 256)
		{
			xoff = 0;
			yoff += charheight;
		}

		test = !test;
	}

	// Load the generated image to the font texture
	texture_.loadImage(&image);

	return true;
}

// -----------------------------------------------------------------------------
// Loads a BMF font. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool SFont::loadBMF(MemChunk& mc)
{
	return false;
}

// -----------------------------------------------------------------------------
// Draws the character [c] with the font, in [colour]
// -----------------------------------------------------------------------------
void SFont::drawCharacter(char c, ColRGBA colour)
{
	// Bind texture
	if (!texture_.bind())
		return;

	// Set colour
	OpenGL::setColour(colour);

	// Get character to draw
	auto& ch = characters_[(uint8_t)c];
	if (!ch.valid())
		return;

	// Draw it
	frect_t tex_rect;
	tex_rect.tl.set(
		(double)ch.tex_bounds.x1() / (double)texture_.getWidth(),
		(double)ch.tex_bounds.y1() / (double)texture_.getHeight());
	tex_rect.br.set(
		(double)ch.tex_bounds.x2() / (double)texture_.getWidth(),
		(double)ch.tex_bounds.y2() / (double)texture_.getHeight());
	glBegin(GL_QUADS);
	glTexCoord2d(tex_rect.x1(), tex_rect.y1());
	glVertex2d(0, 0);
	glTexCoord2d(tex_rect.x1(), tex_rect.y2());
	glVertex2d(0, ch.height);
	glTexCoord2d(tex_rect.x2(), tex_rect.y2());
	glVertex2d(ch.width, ch.height);
	glTexCoord2d(tex_rect.x2(), tex_rect.y1());
	glVertex2d(ch.width, 0);
	glEnd();
}

// -----------------------------------------------------------------------------
// Draws the string [srt] with the font, in [colour], [align]ed
// -----------------------------------------------------------------------------
void SFont::drawString(string_view str, ColRGBA colour, Align align)
{
	// Bind texture
	if (!texture_.bind())
		return;

	// Set colour
	OpenGL::setColour(colour);

	// Determine string length (for alignment)
	int total_width = 0;
	for (char c : str)
	{
		// Get character
		auto& ch = characters_[(uint8_t)c];

		// Increment total width
		if (ch.valid())
			total_width += ch.width + spacing_;
		else
			total_width += spacing_;
	}

	// Translate for alignment
	glPushMatrix();
	if (align == Align::Right)
		glTranslated(-total_width, 0, 0);
	else if (align == Align::Center)
		glTranslated(-(total_width * 0.5), 0, 0);

	// Draw the string
	unsigned xoff = 0;
	for (char c : str)
	{
		// Get character
		auto& ch = characters_[(uint8_t)c];
		if (!ch.valid())
		{
			xoff += spacing_;
			continue;
		}

		// Draw it
		frect_t tex_rect;
		tex_rect.tl.set(
			(double)ch.tex_bounds.x1() / (double)texture_.getWidth(),
			(double)ch.tex_bounds.y1() / (double)texture_.getHeight());
		tex_rect.br.set(
			(double)ch.tex_bounds.x2() / (double)texture_.getWidth(),
			(double)ch.tex_bounds.y2() / (double)texture_.getHeight());
		glBegin(GL_QUADS);
		glTexCoord2d(tex_rect.x1(), tex_rect.y1());
		glVertex2d(xoff, 0);
		glTexCoord2d(tex_rect.x1(), tex_rect.y2());
		glVertex2d(xoff, ch.height);
		glTexCoord2d(tex_rect.x2(), tex_rect.y2());
		glVertex2d(xoff + ch.width, ch.height);
		glTexCoord2d(tex_rect.x2(), tex_rect.y1());
		glVertex2d(xoff + ch.width, 0);
		glEnd();

		// Increment x to next character (1 pixel spacing)
		xoff += ch.width + spacing_;
	}

	glPopMatrix();
}


// -----------------------------------------------------------------------------
//
// SFont Class Static Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the global SLADE font
// -----------------------------------------------------------------------------
SFont& SFont::sladeFont()
{
	if (!font_slade_.texture_.isLoaded())
	{
		// Load slade font
	}

	return font_slade_;
}

// -----------------------------------------------------------------------------
// Returns the global VGA font
// -----------------------------------------------------------------------------
SFont& SFont::vgaFont()
{
	if (!font_vga_.texture_.isLoaded())
	{
		// Get vga font entry
		ArchiveEntry* entry_vgafont = App::archiveManager().programResourceArchive()->entryAtPath("vga-rom-font.16");

		// Load font
		if (entry_vgafont)
			font_vga_.loadFontM(entry_vgafont->data());
	}

	return font_vga_;
}
