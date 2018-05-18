
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    BrowserItem.cpp
// Description: A class representing a single browser item. Each item has a
//              name, index and image associated with it, and handles drawing
//              itself.
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
#include "BrowserItem.h"
#include "BrowserWindow.h"
#include "General/UI.h"
#include "OpenGL/Drawing.h"
#include "OpenGL/OpenGL.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// BrowserItem Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// BrowserItem class constructor
// -----------------------------------------------------------------------------
BrowserItem::BrowserItem(string_view name, unsigned index, string_view type) :
	type_{ type.data(), type.size() },
	name_{ name.data(), name.size() },
	index_{ index }
{
}

// -----------------------------------------------------------------------------
// Loads the item image (base class does nothing, must be overridden by child
// classes to be useful at all)
// -----------------------------------------------------------------------------
bool BrowserItem::loadImage()
{
	return false;
}

// -----------------------------------------------------------------------------
// Draws the item in a [size]x[size] box, keeping the correct aspect ratio of
// it's image
// -----------------------------------------------------------------------------
void BrowserItem::draw(
	int           size,
	int           x,
	int           y,
	Drawing::Font font,
	NameType      nametype,
	ViewType      viewtype,
	ColRGBA        colour,
	bool          text_shadow)
{
	// Determine item name string (for normal viewtype)
	string draw_name;
	if (nametype == NameType::Normal)
		draw_name = name_;
	else if (nametype == NameType::Index)
		draw_name = S_FMT("%d", index_);

	// Truncate name if needed
	if (parent_->truncateNames() && draw_name.size() > 8)
	{
		StrUtil::truncateIP(draw_name, 8);
		draw_name += "...";
	}

	// Item name
	if (viewtype == ViewType::Normal)
	{
		if (text_shadow)
			Drawing::drawText(draw_name, x + (size * 0.5 + 1), y + size + 5, ColRGBA::BLACK, font, Drawing::Align::Center);
		Drawing::drawText(draw_name, x + (size * 0.5), y + size + 4, colour, font, Drawing::Align::Center);
	}
	else if (viewtype == ViewType::Tiles)
	{
		// Create text box if needed
		if (!text_box_)
			text_box_ =
				std::make_unique<TextBox>(S_FMT("%d\n%s", index_, name_), font, UI::scalePx(144), UI::scalePx(16));

		int top = y;
		top += ((size - text_box_->getHeight()) * 0.5);

		if (text_shadow)
			text_box_->draw(x + size + 9, top + 1, ColRGBA::BLACK);
		text_box_->draw(x + size + 8, top, colour);
	}

	// If the item is blank don't bother with the image
	if (blank_)
		return;

	// Try to load image if it isn't already
	if (!image_ || (image_ && !image_->isLoaded()))
		loadImage();

	// If it still isn't just draw a red box with an X
	if (!image_ || (image_ && !image_->isLoaded()))
	{
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);

		glColor3f(1, 0, 0);
		glDisable(GL_TEXTURE_2D);

		// Outline
		glBegin(GL_LINE_LOOP);
		glVertex2i(x, y);
		glVertex2i(x, y + size);
		glVertex2i(x + size, y + size);
		glVertex2i(x + size, y);
		glEnd();

		// X
		glBegin(GL_LINES);
		glVertex2i(x, y);
		glVertex2i(x + size, y + size);
		glVertex2i(x, y + size);
		glVertex2i(x + size, y);
		glEnd();

		glPopAttrib();

		return;
	}

	// Determine texture dimensions
	double width  = image_->getWidth();
	double height = image_->getHeight();

	// Scale up if size > 128
	if (size > 128)
	{
		double scale = (double)size / 128.0;
		width *= scale;
		height *= scale;
	}

	if (width > height)
	{
		// Scale down by width
		if (width > size)
		{
			double scale = (double)size / width;
			width *= scale;
			height *= scale;
		}
	}
	else
	{
		// Scale down by height
		if (height > size)
		{
			double scale = (double)size / height;
			width *= scale;
			height *= scale;
		}
	}

	// Determine draw coords
	double top  = y + ((double)size * 0.5) - (height * 0.5);
	double left = x + ((double)size * 0.5) - (width * 0.5);

	// Draw
	image_->bind();
	OpenGL::setColour(ColRGBA::WHITE, false);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2d(left, top);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2d(left, top + height);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2d(left + width, top + height);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2d(left + width, top);
	glEnd();
}

// -----------------------------------------------------------------------------
// Clears the item image
// -----------------------------------------------------------------------------
void BrowserItem::clearImage() const
{
	if (image_)
		image_->clear();
}
