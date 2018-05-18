
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         https://slade.mancubus.net
// Filename:    SImageFormats.cpp
// Description: SImage class - Encapsulates a paletted or 32bit image. Handles
//              loading/saving different formats, palette conversions, offsets,
//              and a bunch of other stuff
//
//              This file contains the load/save functions for font formats
//              (see SIF*.h for image formats)
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
#include "SImage.h"
#undef BOOL


// -----------------------------------------------------------------------------
//
// Font Formats
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Loads a Doom alpha HUFONT lump and displays it as a picture.
// Why "font0" when it has no FON0 header? Because alpha. ;)
// The format used is simple:
// Offset | Length | Type | Name
//  0x000 |      2 | ui16 | image height (one value for all chars)
//  0x002 |  256*1 | ui08 | characterwidth (one value per char)
//  0x102 |  256*2 | ui16 | characteroffset (one value per char)
//  0x302 |    x*1 | ui08 | pixel color index (one value per pixel)
// So, total size - 302 % value @ 0x00 must be null.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadFont0(const uint8_t* gfx_data, int size)
{
	// Check data
	if (!gfx_data)
		return false;

	if (size <= 0x302)
		return false;

	offset_ = { 0, 0 };
	size_.y = READ_L16(gfx_data, 0);

	data_size_ = size - 0x302;
	if (data_size_ % size_.y)
		return false;

	size_.x = data_size_ / size_.y;

	clearData();
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;
	num_pixels_  = data_size_;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// Create new picture and mask
	const uint8_t* r = gfx_data + 0x302;
	data_            = new uint8_t[data_size_];
	mask_            = new uint8_t[data_size_];
	memset(mask_, 0xFF, data_size_);

	// Data is in column-major format, convert to row-major
	size_t p = 0;
	for (size_t i = 0; i < data_size_; ++i)
	{
		data_[p] = r[i];

		// Index 0 is transparent
		if (data_[p] == 0)
			mask_[p] = 0;

		// Move to next column
		p += size_.x;

		// Move to next row
		if (p >= data_size_)
		{
			p -= data_size_;
			++p;
		}
	}
	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a ZDoom FON1 lump and displays it as a picture.
// Graphically-speaking, a FON1 lump is a column of 256 characters, each
// width*height as indicated by the header. Of course, it would be better to
// convert that into a 16x16 grid, which would be a lot more legible...
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadFont1(const uint8_t* gfx_data, int size)
{
	// Check data
	if (!gfx_data)
		return false;

	// Check/setup size
	size_t charwidth  = gfx_data[4] + 256 * gfx_data[5];
	size_.x           = charwidth;
	size_t charheight = gfx_data[6] + 256 * gfx_data[7];
	size_.y           = charheight << 8;

	// Setup variables
	offset_      = { 0, 0 };
	has_palette_ = false;
	type_        = Type::PalMask;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// Clear current data if it exists
	clearData();
	format_ = nullptr;

	// Read raw pixel data
	num_pixels_ = size_.x * size_.y;
	data_size_  = num_pixels_;
	data_       = new uint8_t[data_size_];
	mask_       = new uint8_t[data_size_];
	memset(mask_, 0xFF, data_size_);

	// Since gfx_data is a const pointer, we can't work on it.
	uint8_t* tempdata = new uint8_t[size];
	memcpy(tempdata, gfx_data, size);


	// We'll use wandering pointers. The original pointer is kept for cleanup.
	uint8_t* read    = tempdata + 8;
	uint8_t* readend = tempdata + size - 1;
	uint8_t* dest    = data_;
	uint8_t* destend = dest + data_size_;

	uint8_t code;
	size_t  length;

	// This uses the same RLE algorithm as compressed IMGZ.
	while (read < readend && dest < destend)
	{
		code = *read++;
		if (code < 0x80)
		{
			length = code + 1;
			memcpy(dest, read, length);
			dest += length;
			read += length;
		}
		else if (code > 0x80)
		{
			length = 0x101 - code;
			code   = *read++;
			memset(dest, code, length);
			dest += length;
		}
	}
	delete[] tempdata;
	// Add transparency to mask
	for (size_t i = 0; i < (unsigned)(data_size_); ++i)
		if (data_[i] == 0)
			mask_[i] = 0x00;

	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a ZDoom FON2 lump and displays it as a picture.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
struct Font2Char
{
	uint16_t width;
	uint8_t* data;
	Font2Char()
	{
		width = 0;
		data  = nullptr;
	}
};
struct Font2Header
{
	uint8_t  headr[4];
	uint16_t charheight;
	uint8_t  firstc;
	uint8_t  lastc;
	uint8_t  constantw;
	uint8_t  shading;
	uint8_t  palsize;
	uint8_t  kerning; // Theoretically a flag field, but with only one flag...
};
bool SImage::loadFont2(const uint8_t* gfx_data, int size)
{
	// Clear current data if it exists
	clearData();

	// Initializes some stuff
	offset_      = { 0, 0 };
	has_palette_ = true;
	type_        = Type::PalMask;
	format_      = nullptr;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	if (size < (int)sizeof(Font2Header))
		return false;

	const auto* header = (Font2Header*)gfx_data;
	size_.x            = 0;
	size_.y            = wxUINT16_SWAP_ON_BE(header->charheight);

	// We can't deal with a null height, of course
	if (size_.y == 0)
		return false;

	const uint8_t* p = gfx_data + sizeof(Font2Header);

	// Skip kerning information which do not concern us.
	if (header->kerning)
		p += 2;

	// Start building the character table.
	size_t     numchars = wxUINT16_SWAP_ON_BE(header->lastc) - wxUINT16_SWAP_ON_BE(header->firstc) + 1;
	Font2Char* chars    = new Font2Char[numchars];
	for (size_t i = 0; i < numchars; ++i)
	{
		chars[i].width = wxUINT16_SWAP_ON_BE(*(uint16_t*)p);
		// Let's increase the total width
		size_.x += chars[i].width;
		if (chars[1].width > 0) // Add spacing between characters
			size_.x++;
		// The width information is enumerated for each character only if they are
		// not constant width. Regardless, move the read pointer away after the last.
		if (!(header->constantw) || (i == numchars - 1))
			p += 2;
	}

	size_.x--; // Remove spacing after last character

	// Let's build the palette now.
	for (size_t i = 0; i < header->palsize + 1u; ++i)
	{
		ColRGBA color;
		color.r = *p++;
		color.g = *p++;
		color.b = *p++;
		palette_.setColour(i, color);
	}

	// 0 is transparent, last is border color, the rest of the palette entries should
	// be increasingly bright
	palette_.setTransIndex(0);

	// The picture data follows, using the same RLE as FON1 and IMGZ.
	for (size_t i = 0; i < numchars; ++i)
	{
		// A big font is not necessarily continuous; several characters
		// may be skipped; they are given a width of 0.
		if (chars[i].width)
		{
			size_t numpixels = chars[i].width * size_.y;
			chars[i].data    = new uint8_t[numpixels];
			uint8_t* d       = chars[i].data;
			uint8_t  code;
			size_t   length;

			while (numpixels)
			{
				code = *p++;
				if (code < 0x80)
				{
					length = code + 1;
					// Overflows shouldn't happen
					if (length > numpixels)
					{
						delete[] chars;
						return false;
					}
					memcpy(d, p, length);
					d += length;
					p += length;
					numpixels -= length;
				}
				else if (code > 0x80)
				{
					length = 0x101 - code;
					// Overflows shouldn't happen
					if (length > numpixels)
					{
						delete[] chars;
						return false;
					}
					code = *p++;
					memset(d, code, length);
					d += length;
					numpixels -= length;
				}
			}
		}
	}

	// Now let's assemble all characters together in a single picture.
	if (!size_.x)
		return false;

	// Clear current data if it exists
	clearData();

	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	memset(data_, 0, data_size_);
	uint8_t* d;
	for (size_t i = 0; i < (unsigned)size_.y; ++i)
	{
		d = data_ + i * size_.x;
		for (size_t j = 0; j < numchars; ++j)
		{
			if (chars[j].width)
			{
				memcpy(d, chars[j].data + (i * chars[j].width), chars[j].width);
				d += chars[j].width + 1;
			}
		}
	}

	// Clean up the characters once the big picture is finished
	for (size_t i = 0; i < numchars; ++i)
		delete[] chars[i].data;
	delete[] chars;

	// Now transparency for the mask
	mask_ = new uint8_t[data_size_];
	memset(mask_, 0xFF, data_size_);
	for (size_t i = 0; i < (unsigned)(data_size_); ++i)
		if (data_[i] == 0)
			mask_[i] = 0;

	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a byte map font lump and displays it as a picture.
// Specs for the format are here: http://bmf.wz.cz/bmf-format.htm
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
struct BMFChar
{
	uint8_t which;  // 0
	uint8_t width;  // 1
	uint8_t height; // 2
	int8_t  offsx;  // 3
	int8_t  offsy;  // 4
	uint8_t shift;  // 5
	// Rest is not part of the header proper
	const uint8_t* cdata;
	BMFChar()
	{
		which = width = height = offsx = offsy = shift = 0;
		cdata                                          = nullptr;
	}
};
struct BMFFont
{
	uint8_t headr[4];       //  0
	uint8_t version;        //  4
	uint8_t lineheight = 0; //  5
	int8_t  size_over  = 0; //  6
	int8_t  size_under = 0; //  7
	int8_t  add_space  = 0; //  8
	int8_t  size_inner = 0; //  9
	uint8_t num_colors = 0; // 10
	uint8_t top_color  = 0; // 11
	uint8_t reserved[4];    // 12
	uint8_t pal_size = 0;   // 16
	// Rest is not part of the header proper
	uint8_t     info_size = 0;
	uint16_t    num_chars = 0;
	const char* info      = nullptr;
	BMFChar*    chars     = nullptr;
	BMFFont(BMFFont* other)
	{
		lineheight = other->lineheight;
		size_over  = other->size_over;
		size_under = other->size_under;
		add_space  = other->add_space;
		size_inner = other->size_inner;
		num_colors = other->num_colors;
		top_color  = other->top_color;
		pal_size   = other->pal_size;
		info_size  = other->info_size;
		num_chars  = other->num_chars;
		info       = nullptr;
		chars      = nullptr;
	}
	~BMFFont() { delete[] chars; }
};
bool SImage::loadBMF(const uint8_t* gfx_data, int size)
{
	if (!gfx_data || size < 24)
		return false;
	const uint8_t* eod = gfx_data + size;

	BMFFont mf((BMFFont*)gfx_data);

	// Check for invalid data, we need at least one visible color
	if (mf.pal_size == 0)
		return false;

	// Clean up old data and set up variables
	clearData();
	type_        = Type::PalMask;
	has_palette_ = true;
	format_      = nullptr;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	const uint8_t* ofs = gfx_data + 17;

	// Setup palette -- it's a 6-bit palette (63 max) so we have to convert it to 8-bit.
	// Palette index 0 is used as the transparent color and not described at all.
	palette_.setColour(0, ColRGBA(0, 0, 0, 0));
	for (size_t i = 0; i < mf.pal_size; ++i)
	{
		palette_.setColour(
			i + 1,
			ColRGBA(
				(ofs[(i * 3)] << 2) + (ofs[(i * 3)] >> 4),
				(ofs[(i * 3) + 1] << 2) + (ofs[(i * 3)] >> 4),
				(ofs[(i * 3) + 2] << 2) + (ofs[(i * 3) + 2] >> 4),
				255));
	}

	// Move to after the palette and get amount of characters
	ofs += mf.pal_size * 3;
	if (ofs >= eod)
	{
		LOG_MESSAGE(1, "BMF aborted: no data after palette");
		return false;
	}
	mf.info_size = ofs[0];
	if (mf.info_size > 0)
	{
		mf.info = (char*)ofs + 1;
	}
	ofs += mf.info_size + 1;
	mf.num_chars = READ_L16(ofs, 0);
	if (mf.num_chars == 0)
		return false;

	// Now we are at the beginning of character data
	ofs += 2;
	if (ofs >= eod)
	{
		LOG_MESSAGE(1, "BMF aborted: no data after char size");
		return false;
	}
	// Let's create each character's data and compute the total size
	mf.chars = new BMFChar[mf.num_chars];
	int miny = ofs[4], maxy = ofs[2];
	size_.x = ofs[5] + ofs[3];
	for (size_t i = 0; i < mf.num_chars; ++i)
	{
		mf.chars[i].which  = ofs[0];
		mf.chars[i].width  = ofs[1];
		mf.chars[i].height = ofs[2];
		mf.chars[i].offsx  = ofs[3];
		mf.chars[i].offsy  = ofs[4];
		mf.chars[i].shift  = ofs[5];
		mf.chars[i].cdata  = ofs + 6;
		ofs += (6 + (mf.chars[i].width * mf.chars[i].height));
		// Sanity check, some supposedly-valid fonts do not have all the
		// characters they pretend to have (e.g., 274.bmf). Being truncated
		// does not prevent them from being considered valid, so cut them off
		if (ofs >= eod && (i + 1) < mf.num_chars)
		{
			mf.num_chars = i;
		}
		// Zap empty characters, no need to waste space displaying their void.
		if (mf.chars[i].width == 0 && mf.chars[i].height == 0)
		{
			--i;
			--mf.num_chars;
		}
		else
		{
			if (miny > mf.chars[i].offsy)
				miny = mf.chars[i].offsy;
			if (maxy < mf.chars[i].height)
				maxy = mf.chars[i].height;
			size_.x += mf.add_space + mf.chars[i].shift;
		}
	}
	size_.y = maxy - miny;

	// Create new fully transparent image
	size_t pixela = 0, pixelb = 0, pixels = size_.x * size_.y;
	data_ = new uint8_t[pixels];
	mask_ = new uint8_t[pixels];
	memset(data_, 0x00, pixels);
	memset(mask_, 0x00, pixels);

	// Start processing each character, painting it on the empty canvas
	int startx = (mf.chars[0].offsy < 0 ? 0 : mf.chars[0].offsy);
	int starty = (miny < 0 ? 0 - miny : 0);
	for (int i = 0; i < mf.num_chars; ++i)
	{
		BMFChar* mc = &mf.chars[i];
		if (mc->width && mc->height)
		{
			for (int v = 0; v < mc->height; ++v)
			{
				for (int u = 0; u < mc->width; ++u)
				{
					// Source pixel
					pixela = v * mc->width + u;
					// Destination pixel
					pixelb = (starty + v + mc->offsy) * size_.x + startx + u + mc->offsx;
					// Only paint if appropriate
					if ((mc->cdata + pixela < eod) && (mc->cdata + pixela < mf.chars[i + 1].cdata - 6)
						&& mc->cdata[pixela] && pixelb < pixels)
					{
						data_[pixelb] = mc->cdata[pixela];
						mask_[pixelb] = 0xFF;
					}
				}
			}
			startx += (mf.add_space + mc->shift);
		}
	}
	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a monochrome, monospaced font and displays it as a picture.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadFontM(const uint8_t* gfx_data, int size)
{
	// Check data
	if (!gfx_data || size % 256)
		return false;

	// Setup variables
	offset_      = { 0, 0 };
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;

	size_t charwidth  = 8;
	size_t charheight = size >> 8;
	size_.x           = charwidth;
	size_.y           = charheight << 8;

	if (size_.x * size_.y != size * 8)
		return false;

	// reset data
	clearData();
	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	memset(data_, 0xFF, data_size_);
	mask_ = new uint8_t[data_size_];
	memset(mask_, 0x00, data_size_);

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// Each pixel is described as a single bit, either on or off
	for (size_t i = 0; i < (unsigned)size; ++i)
	{
		for (size_t p = 0; p < 8; ++p)
			mask_[(i * 8) + p] = ((gfx_data[i] >> (7 - p)) & 1) * 255;
	}
	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a Wolf3D-format font.
// The format used is simple, basically like the Doom alpha HUFONT, except not
// in the same order:
// Offset | Length | Type | Name
//  0x000 |      2 | ui16 | image height (one value for all chars)
//  0x002 |  256*2 | ui16 | characteroffset (one value per char)
//  0x202 |  256*1 | ui08 | characterwidth (one value per char)
//  0x302 |    x*1 | ui08 | pixel color index (one value per pixel)
// So, total size - 302 % value @ 0x00 must be null.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadWolfFont(const uint8_t* gfx_data, int size)
{
	// Check data
	if (!gfx_data)
		return false;

	if (size <= 0x302)
		return false;

	offset_ = { 0, 0 };
	size_.y = READ_L16(gfx_data, 0);

	size_t datasize = size - 0x302;
	if (datasize % size_.y)
		return false;

	size_.x = datasize / size_.y;

	clearData();
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// Create new picture and mask
	const uint8_t* r = gfx_data + 0x302;
	data_            = new uint8_t[datasize];
	mask_            = new uint8_t[datasize];
	memset(mask_, 0xFF, datasize);
	memcpy(data_, r, datasize);

	size_t p = 0; // Previous width
	size_t w = 0; // This character's width
	// Run through each character
	for (size_t c = 0; c < 256; ++c)
	{
		p += w;                  // Add previous character width to total
		w = gfx_data[c + 0x202]; // Get this character's width
		if (!w)
			continue;
		size_t o = READ_L16(gfx_data, ((c << 1) + 2));
		for (size_t i = 0; i < w * size_.y; ++i)
		{
			// Compute source and destination offsets
			size_t s = o + i;
			size_t d = ((i / w) * size_.x) + (i % w) + p;
			data_[d] = gfx_data[s];
			// Index 0 is transparent
			if (data_[d] == 0)
				mask_[d] = 0;
		}
	}
	// Announce change and return success
	announce("image_changed");
	return true;
}


// -----------------------------------------------------------------------------
// Loads a Jedi Engine-format bitmap font.
// The header tells the height and which are the first and last characters
// described. Then the character data consists of a single byte of data for the
// width of that character, followed by a list of columns. The characters are
// listed in order.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadJediFNT(const uint8_t* gfx_data, int size)
{
	// Check data
	if (!gfx_data)
		return false;

	if (size <= 35)
		return false;

	// The character data is presented as a list of columns
	// preceded by a byte for
	offset_ = { 0, 0 };

	// Since the format is column-major, we'll use our usual cheat of
	// inverting height and width to build the picture, then rotating it.
	size_.x = gfx_data[4];

	// First and last characters
	uint8_t firstc = gfx_data[8];
	uint8_t lastc  = gfx_data[9];
	uint8_t numchr = 1 + lastc - firstc;

	// Go through each character to compute the total width (pre-rotation height)
	size_.y   = 0;
	size_t wo = 32; // Offset to width of next character
	for (uint8_t i = 0; i < numchr; ++i)
	{
		size_.y += gfx_data[wo];
		wo += 1 + (size_.x * gfx_data[wo]);
	}

	clearData();
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// Create new picture and mask
	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	mask_                    = new uint8_t[data_size_];
	memset(mask_, 0xFF, data_size_);

	// Run through each character and add the pixel data
	wo         = 32;
	size_t col = 0;
	for (uint8_t i = 0; i < numchr; ++i)
	{
		uint8_t numcols = gfx_data[wo++];
		memcpy(data_ + (col * size_.x), gfx_data + wo, numcols * size_.x);
		col += numcols;
		wo += size_.x * numcols;
	}

	// Make index 0 transparent
	for (int i = 0; i < size_.x * size_.y; ++i)
		if (data_[i] == 0)
			mask_[i] = 0;

	// Convert from column-major to row-major
	rotate(90);

	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a Jedi Engine-format monochrome font.
// Contrarily to what the DF specs claim, the first two int16 values are not the
// first and last characters as in the FNT format; instead, they are the first
// character and the number of characters! They're also mistaken about character
// width.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadJediFONT(const uint8_t* gfx_data, int size)
{
	// Check data
	if (size < 16)
		return false;

	int numchr = READ_L16(gfx_data, 2);

	// Setup variables
	offset_      = { 0, 0 };
	size_.y      = READ_L16(gfx_data, 6) * numchr;
	size_.x      = READ_L16(gfx_data, 4);
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;

	// reset data
	clearData();
	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	memset(data_, 0xFF, data_size_);
	mask_ = new uint8_t[data_size_];
	memset(mask_, 0x00, data_size_);

	// Technically each character is its own image, though.
	num_images_ = 1;
	img_index_  = 0;

	// We don't care about the character widths since
	// technically it's always eight anyway. The offset
	// to graphic data corresponds to 12 (header size)
	// plus one byte by character for width.
	int o   = 12 + numchr;
	int bpc = size_.x / 8;

	// Each pixel is described as a single bit, either on or off
	for (int i = 0; i < size_.y; ++i)
	{
		for (int p = 0; p < size_.x; ++p)
		{
			switch (bpc)
			{
			case 1: mask_[(i * size_.x) + p] = ((gfx_data[o + i] >> (7 - p)) & 1) * 255; break;
			case 2: mask_[(i * size_.x) + p] = ((READ_B16(gfx_data, o + (i * 2)) >> (15 - p)) & 1) * 255; break;
			case 3: mask_[(i * size_.x) + p] = ((READ_B24(gfx_data, o + (i * 3)) >> (23 - p)) & 1) * 255; break;
			case 4: mask_[(i * size_.x) + p] = ((READ_B32(gfx_data, o + (i * 4)) >> (31 - p)) & 1) * 255; break;
			default:
				clearData();
				Global::error = "Jedi FONT: Weird word width";
				return false;
			}
		}
	}
	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a Jaguar Doom sprite. This needs manual handling because the data is
// split in two separate lumps, one with the header and the other with raw pixel
// data. So we need to have access to both.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadJaguarSprite(const uint8_t* header, int hdr_size, const uint8_t* gfx_data, int size)
{
	if (header == nullptr || gfx_data == nullptr || hdr_size < 16 || size == 0)
	{
		Global::error = "Invalid Jaguar sprite";
		return false;
	}

	size_.x      = READ_B16(header, 0);
	size_.y      = READ_B16(header, 2);
	int16_t ofsx = READ_B16(header, 4);
	int16_t ofsy = READ_B16(header, 6);
	offset_.x    = ofsx;
	offset_.y    = ofsy;
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;
	num_images_  = 1;
	img_index_   = 0;

	// reset data
	clearData();
	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	memset(data_, 0x00, data_size_);
	mask_ = new uint8_t[data_size_];
	memset(mask_, 0x00, data_size_);

	// Read column offsets
	if (hdr_size < (8 + (size_.x * 6)))
	{
		Global::error = S_FMT(
			"Invalid Jaguar sprite: header too small (%d) for column offsets (%d)", hdr_size, (8 + (size_.x * 6)));
		return false;
	}
	vector<uint16_t> col_offsets(size_.x);
	for (int w = 0; w < size_.x; ++w)
	{
		col_offsets[w] = READ_B16(header, 8 + 2 * w);
	}
	if (hdr_size < (4 + col_offsets[size_.x - 1]))
	{
		Global::error = S_FMT(
			"Invalid Jaguar sprite: header too small (%d) for post offsets (%d)",
			hdr_size,
			4 + col_offsets[size_.x - 1]);
		return false;
	}

	// Okay, so it's finally time to read some pixel data
	for (int w = 0; w < size_.x; ++w)
	{
		int post_p = col_offsets[w];
		// Process all posts in the column
		while (READ_B16(header, post_p) != 0xFFFF)
		{
			int top     = header[post_p];
			int len     = header[post_p + 1];
			int pixel_p = READ_B16(header, post_p + 2);
			if (pixel_p + len > size)
			{
				Global::error =
					S_FMT("Invalid Jaguar sprite: body too small (%d) for pixel data (%d)", size, pixel_p + len);
				return false;
			}
			// Copy pixels
			for (int p = 0; p < len; ++p)
			{
				size_t pos = w + size_.x * (top + p);
				data_[pos] = gfx_data[pixel_p + p];
				mask_[pos] = 0xFF;
			}
			post_p += 4;
		}
	}

	// Announce change and return success
	announce("image_changed");
	return true;
}

// -----------------------------------------------------------------------------
// Loads a Jaguar Doom texture. This needs manual handling because the
// dimensions are contained in the TEXTURE1 lump instead.
// Returns false if the image data was invalid, true otherwise.
// -----------------------------------------------------------------------------
bool SImage::loadJaguarTexture(const uint8_t* gfx_data, int size, int i_width, int i_height)
{
	// Check data
	if (i_width * i_height == 0 || size < i_width * i_height + 320)
	{
		Global::error = S_FMT("Size is %d, expected %d", size, i_width * i_height + 320);
		return false;
	}

	// Setup variables
	offset_      = { 0, 0 };
	size_.x      = i_height; // Format is column-major
	size_.y      = i_width;  // We'll rotate them afterwards
	has_palette_ = false;
	type_        = Type::PalMask;
	format_      = nullptr;
	num_images_  = 1;
	img_index_   = 0;

	// reset data
	clearData();
	num_pixels_ = data_size_ = size_.x * size_.y;
	data_                    = new uint8_t[data_size_];
	memcpy(data_, gfx_data, data_size_);
	mask_ = new uint8_t[data_size_];
	memset(mask_, 0xFF, data_size_);

	// rotate and mirror image
	rotate(90);
	mirror(false);

	// Announce change and return success
	announce("image_changed");
	return true;
}
