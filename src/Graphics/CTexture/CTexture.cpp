
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    CTexture.cpp
// Description: C(omposite)Texture class, represents a composite texture as
//              described in TEXTUREx entries
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
#include "CTexture.h"
#include "General/Misc.h"
#include "General/ResourceManager.h"
#include "Graphics/SImage/SImage.h"
#include "TextureXList.h"
#include "Utility/StringUtils.h"
#include "Utility/Tokenizer.h"


// -----------------------------------------------------------------------------
//
// CTPatch Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// CTPatch class constructor copying another CTPatch
// -----------------------------------------------------------------------------
CTPatch::CTPatch(CTPatch* copy)
{
	if (copy)
	{
		name_     = copy->name_;
		offset_x_ = copy->offset_x_;
		offset_y_ = copy->offset_y_;
	}
}

// -----------------------------------------------------------------------------
// Returns the entry (if any) associated with this patch via the resource
// manager. Entries in [parent] will be prioritised over entries in any other
// open archive
// -----------------------------------------------------------------------------
ArchiveEntry* CTPatch::getPatchEntry(Archive* parent)
{
	// Default patches should be in patches namespace
	ArchiveEntry* entry = theResourceManager->getPatchEntry(name_, "patches", parent);

	// Not found in patches, check in graphics namespace
	if (!entry)
		entry = theResourceManager->getPatchEntry(name_, "graphics", parent);

	// Not found in patches, check in stand-alone texture namespace
	if (!entry)
		entry = theResourceManager->getPatchEntry(name_, "textures", parent);

	return entry;
}


// -----------------------------------------------------------------------------
//
// CTPatchEx Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// CTPatchEx class constructor copying a regular CTPatch
// -----------------------------------------------------------------------------
CTPatchEx::CTPatchEx(CTPatch* copy)
{
	if (copy)
	{
		offset_x_ = copy->xOffset();
		offset_y_ = copy->yOffset();
		name_     = copy->name();
	}
}

// -----------------------------------------------------------------------------
// CTPatchEx class constructor copying another CTPatchEx
// -----------------------------------------------------------------------------
CTPatchEx::CTPatchEx(CTPatchEx* copy)
{
	if (copy)
	{
		flip_x_      = copy->flip_x_;
		flip_y_      = copy->flip_y_;
		use_offsets_ = copy->useOffsets();
		rotation_    = copy->rotation_;
		alpha_       = copy->alpha_;
		style_       = copy->style_;
		blendtype_   = copy->blendtype_;
		colour_      = copy->colour_;
		offset_x_    = copy->offset_x_;
		offset_y_    = copy->offset_y_;
		name_        = copy->name_;
		type_        = copy->type_;
		translation_.copy(copy->translation_);
	}
}

// -----------------------------------------------------------------------------
// Returns the entry (if any) associated with this patch via the resource
// manager. Entries in [parent] will be prioritised over entries in any other
// open archive
// -----------------------------------------------------------------------------
ArchiveEntry* CTPatchEx::getPatchEntry(Archive* parent)
{
	// 'Patch' type: patches > graphics
	if (type_ == Type::Patch)
	{
		ArchiveEntry* entry = theResourceManager->getPatchEntry(name_, "patches", parent);
		if (!entry)
			entry = theResourceManager->getFlatEntry(name_, parent);
		if (!entry)
			entry = theResourceManager->getPatchEntry(name_, "graphics", parent);
		return entry;
	}

	// 'Graphic' type: graphics > patches
	if (type_ == Type::Graphic)
	{
		ArchiveEntry* entry = theResourceManager->getPatchEntry(name_, "graphics", parent);
		if (!entry)
			entry = theResourceManager->getPatchEntry(name_, "patches", parent);
		if (!entry)
			entry = theResourceManager->getFlatEntry(name_, parent);
		return entry;
	}
	// Silence warnings
	return nullptr;
}

// -----------------------------------------------------------------------------
// Parses a ZDoom TEXTURES format patch definition
// -----------------------------------------------------------------------------
bool CTPatchEx::parse(Tokenizer& tz, Type type)
{
	// Read basic info
	this->type_ = type;
	name_       = StrUtil::upper(tz.next().text);
	tz.adv(); // Skip ,
	offset_x_ = tz.next().asInt();
	tz.adv(); // Skip ,
	offset_y_ = tz.next().asInt();

	// Check if there is any extended info
	if (tz.advIfNext("{", 2))
	{
		// Parse extended info
		while (!tz.checkOrEnd("}"))
		{
			// FlipX
			if (tz.checkNC("FlipX"))
				flip_x_ = true;

			// FlipY
			if (tz.checkNC("FlipY"))
				flip_y_ = true;

			// UseOffsets
			if (tz.checkNC("UseOffsets"))
				use_offsets_ = true;

			// Rotate
			if (tz.checkNC("Rotate"))
				rotation_ = tz.next().asInt();

			// Translation
			if (tz.checkNC("Translation"))
			{
				// Build translation string
				string translate;
				string temp = tz.next().text;
				if (StrUtil::contains(temp, '='))
					temp = S_FMT("\"%s\"", temp);
				translate += temp;
				while (tz.checkNext(","))
				{
					translate += tz.next().text; // add ','
					temp = tz.next().text;
					if (StrUtil::contains(temp, '='))
						temp = S_FMT("\"%s\"", temp);
					translate += temp;
				}
				// Parse whole string
				translation_.parse(translate);
				blendtype_ = 1;
			}

			// Blend
			if (tz.checkNC("Blend"))
			{
				wxColour col;
				blendtype_ = 2;

				// Read first value
				string first = tz.next().text;

				// If no second value, it's just a colour string
				if (!tz.checkNext(","))
				{
					col.Set(first);
					colour_.set(COLWX(col));
				}
				else
				{
					// Second value could be alpha or green
					tz.adv(); // Skip ,
					double second = tz.next().asFloat();

					// If no third value, it's an alpha value
					if (!tz.checkNext(","))
					{
						col.Set(first);
						colour_.set(COLWX(col), second * 255);
						blendtype_ = 3;
					}
					else
					{
						// Third value exists, must be R,G,B,A format
						// RGB are ints in the 0-255 range; A is float in the 0.0-1.0 range
						tz.adv(); // Skip ,
						colour_.r = std::stod(first);
						colour_.g = second;
						colour_.b = tz.next().asInt();
						if (!tz.checkNext(","))
						{
							Log::error(S_FMT("Invalid TEXTURES definition, expected ',', got '%s'", tz.peek().text));
							return false;
						}
						tz.adv(); // Skip ,
						colour_.a  = tz.next().asFloat() * 255;
						blendtype_ = 3;
					}
				}
			}

			// Alpha
			if (tz.checkNC("Alpha"))
				alpha_ = tz.next().asFloat();

			// Style
			if (tz.checkNC("Style"))
				style_ = tz.next().text;

			// Read next property name
			tz.adv();
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Returns a text representation of the patch in ZDoom TEXTURES format
// -----------------------------------------------------------------------------
string CTPatchEx::asText()
{
	// Init text string
	string typestring = "Patch";
	if (type_ == Type::Graphic)
		typestring = "Graphic";
	string text = S_FMT("\t%s \"%s\", %d, %d\n", typestring, name_, offset_x_, offset_y_);

	// Check if we need to write any extra properties
	if (!flip_x_ && !flip_y_ && !use_offsets_ && rotation_ == 0 && blendtype_ == 0 && alpha_ == 1.0f
		&& StrUtil::equalCI(style_, "Copy"))
		return text;
	else
		text += "\t{\n";

	// Write patch properties
	if (flip_x_)
		text += "\t\tFlipX\n";
	if (flip_y_)
		text += "\t\tFlipY\n";
	if (use_offsets_)
		text += "\t\tUseOffsets\n";
	if (rotation_ != 0)
		text += S_FMT("\t\tRotate %d\n", rotation_);
	if (blendtype_ == 1 && !translation_.isEmpty())
	{
		text += "\t\tTranslation ";
		text += translation_.asText();
		text += "\n";
	}
	if (blendtype_ >= 2)
	{
		wxColour col(colour_.r, colour_.g, colour_.b);
		text += S_FMT("\t\tBlend \"%s\"", col.GetAsString(wxC2S_HTML_SYNTAX));

		if (blendtype_ == 3)
			text += S_FMT(", %1.1f\n", (double)colour_.a / 255.0);
		else
			text += "\n";
	}
	if (alpha_ < 1.0f)
		text += S_FMT("\t\tAlpha %1.2f\n", alpha_);
	if (!(StrUtil::equalCI(style_, "Copy")))
		text += S_FMT("\t\tStyle %s\n", style_);

	// Write ending
	text += "\t}\n";

	return text;
}


// -----------------------------------------------------------------------------
//
// CTexture Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Copies the texture [tex] to this texture. If [keep_type] is true, the current
// texture type (extended/regular) will be kept, otherwise it will be converted
// to the type of [tex]
// -----------------------------------------------------------------------------
void CTexture::copyTexture(CTexture* tex, bool keep_type)
{
	// Check texture was given
	if (!tex)
		return;

	// Clear current texture
	clear();

	// Copy texture info
	this->name_          = tex->name_;
	this->width_         = tex->width_;
	this->height_        = tex->height_;
	this->def_width_     = tex->def_width_;
	this->def_height_    = tex->def_height_;
	this->scale_x_       = tex->scale_x_;
	this->scale_y_       = tex->scale_y_;
	this->world_panning_ = tex->world_panning_;
	if (!keep_type)
	{
		this->extended_ = tex->extended_;
		this->defined_  = tex->defined_;
	}
	this->optional_     = tex->optional_;
	this->no_decals_    = tex->no_decals_;
	this->null_texture_ = tex->null_texture_;
	this->offset_x_     = tex->offset_x_;
	this->offset_y_     = tex->offset_y_;
	this->type_         = tex->type_;

	// Update scaling
	if (extended_)
	{
		if (scale_x_ == 0)
			scale_x_ = 1;
		if (scale_y_ == 0)
			scale_y_ = 1;
	}
	else if (!extended_ && tex->extended_)
	{
		if (scale_x_ == 1)
			scale_x_ = 0;
		if (scale_y_ == 1)
			scale_y_ = 0;
	}

	// Copy patches
	for (unsigned a = 0; a < tex->nPatches(); a++)
	{
		CTPatch* patch = tex->patch(a);

		if (extended_)
		{
			if (tex->extended_)
				patches_.emplace_back(new CTPatchEx((CTPatchEx*)patch));
			else
				patches_.emplace_back(new CTPatchEx(patch));
		}
		else
			addPatch(patch->name(), patch->xOffset(), patch->yOffset());
	}
}

// -----------------------------------------------------------------------------
// Returns the patch at [index], or NULL if [index] is out of bounds
// -----------------------------------------------------------------------------
CTPatch* CTexture::patch(size_t index)
{
	// Check index
	if (index >= patches_.size())
		return nullptr;

	// Return patch at index
	return patches_[index].get();
}

// -----------------------------------------------------------------------------
// Returns the index of this texture within its parent list
// -----------------------------------------------------------------------------
int CTexture::index() const
{
	// Check if a parent TextureXList exists
	if (!in_list_)
		return index_;

	// Find this texture in the parent list
	return in_list_->textureIndex(this->name());
}

// -----------------------------------------------------------------------------
// Clears all texture data
// -----------------------------------------------------------------------------
void CTexture::clear()
{
	this->name_          = "";
	this->width_         = 0;
	this->height_        = 0;
	this->def_width_     = 0;
	this->def_height_    = 0;
	this->scale_x_       = 1.0;
	this->scale_y_       = 1.0;
	this->defined_       = false;
	this->world_panning_ = false;
	this->optional_      = false;
	this->no_decals_     = false;
	this->null_texture_  = false;
	this->offset_x_      = 0;
	this->offset_y_      = 0;

	// Clear patches
	this->patches_.clear();
}

// -----------------------------------------------------------------------------
// Adds a patch to the texture with the given attributes, at [index].
// If [index] is -1, the patch is added to the end of the list.
// -----------------------------------------------------------------------------
bool CTexture::addPatch(string_view patch, int16_t offset_x, int16_t offset_y, int index)
{
	// Create new patch
	std::unique_ptr<CTPatch> np;
	if (extended_)
		np = std::make_unique<CTPatchEx>(patch, offset_x, offset_y);
	else
		np = std::make_unique<CTPatch>(patch, offset_x, offset_y);

	// Add it either after [index] or at the end
	if (index >= 0 && (unsigned)index < patches_.size())
		patches_.insert(patches_.begin() + index, std::move(np));
	else
		patches_.push_back(std::move(np));

	// Cannot be a simple define anymore
	this->defined_ = false;

	// Announce
	announce("patches_modified");

	return true;
}

// -----------------------------------------------------------------------------
// Removes the patch at [index].
// Returns false if [index] is invalid, true otherwise
// -----------------------------------------------------------------------------
bool CTexture::removePatch(size_t index)
{
	// Check index
	if (index >= patches_.size())
		return false;

	// Remove the patch
	patches_.erase(patches_.begin() + index);

	// Cannot be a simple define anymore
	this->defined_ = false;

	// Announce
	announce("patches_modified");

	return true;
}

// -----------------------------------------------------------------------------
// Removes all instances of [patch] from the texture.
// Returns true if any were removed, false otherwise
// -----------------------------------------------------------------------------
bool CTexture::removePatch(string_view patch)
{
	// Go through patches
	bool removed = false;
	for (unsigned a = 0; a < patches_.size(); a++)
	{
		if (patches_[a]->name() == patch)
		{
			patches_.erase(patches_.begin() + a);
			removed = true;
			a--;
		}
	}

	// Cannot be a simple define anymore
	this->defined_ = false;

	if (removed)
		announce("patches_modified");

	return removed;
}

// -----------------------------------------------------------------------------
// Replaces the patch at [index] with [newpatch], and updates its associated
// ArchiveEntry with [newentry].
// Returns false if [index] is out of bounds, true otherwise
// -----------------------------------------------------------------------------
bool CTexture::replacePatch(size_t index, string_view newpatch)
{
	// Check index
	if (index >= patches_.size())
		return false;

	// Replace patch at [index] with new
	patches_[index]->setName(newpatch);

	// Announce
	announce("patches_modified");

	return true;
}

// -----------------------------------------------------------------------------
// Duplicates the patch at [index], placing the duplicated patch at
// [offset_x],[offset_y] from the original.
// Returns false if [index] is out of bounds, true otherwise
// -----------------------------------------------------------------------------
bool CTexture::duplicatePatch(size_t index, int16_t offset_x, int16_t offset_y)
{
	// Check index
	if (index >= patches_.size())
		return false;

	// Get patch info
	CTPatch* dp = patches_[index].get();

	// Add duplicate patch
	std::unique_ptr<CTPatch> np;
	if (extended_)
		np = std::make_unique<CTPatchEx>((CTPatchEx*)patches_[index].get());
	else
		np = std::make_unique<CTPatch>(patches_[index].get());
	patches_.insert(patches_.begin() + index, std::move(np));

	// Offset patch by given amount
	patches_[index + 1]->setOffsetX(dp->xOffset() + offset_x);
	patches_[index + 1]->setOffsetY(dp->yOffset() + offset_y);

	// Cannot be a simple define anymore
	this->defined_ = false;

	// Announce
	announce("patches_modified");

	return true;
}

// -----------------------------------------------------------------------------
// Swaps the patches at [p1] and [p2].
// Returns false if either index is invalid, true otherwise
// -----------------------------------------------------------------------------
bool CTexture::swapPatches(size_t p1, size_t p2)
{
	// Check patch indices are correct
	if (p1 >= patches_.size() || p2 >= patches_.size())
		return false;

	// Swap the patches
	patches_[p1].swap(patches_[p2]);

	// Announce
	announce("patches_modified");

	return true;
}

// -----------------------------------------------------------------------------
// Parses a TEXTURES format texture definition
// -----------------------------------------------------------------------------
bool CTexture::parse(Tokenizer& tz, string_view type)
{
	// Check if optional
	if (tz.advIfNext("optional"))
		optional_ = true;

	// Read basic info
	S_SET_VIEW(this->type_, type);
	this->extended_ = true;
	this->defined_  = false;
	name_           = StrUtil::upper(tz.next().text);
	tz.adv(); // Skip ,
	width_ = tz.next().asInt();
	tz.adv(); // Skip ,
	height_ = tz.next().asInt();

	// Check for extended info
	if (tz.advIfNext("{", 2))
	{
		// Read properties
		while (!tz.check("}"))
		{
			// Check if end of text is reached (error)
			if (tz.atEnd())
			{
				Log::error(S_FMT("Error parsing texture %s: End of text found, missing } perhaps?", name_));
				return false;
			}

			// XScale
			if (tz.checkNC("XScale"))
				scale_x_ = tz.next().asFloat();

			// YScale
			else if (tz.checkNC("YScale"))
				scale_y_ = tz.next().asFloat();

			// Offset
			else if (tz.checkNC("Offset"))
			{
				offset_x_ = tz.next().asInt();
				tz.skipToken(); // Skip ,
				offset_y_ = tz.next().asInt();
			}

			// WorldPanning
			else if (tz.checkNC("WorldPanning"))
				world_panning_ = true;

			// NoDecals
			else if (tz.checkNC("NoDecals"))
				no_decals_ = true;

			// NullTexture
			else if (tz.checkNC("NullTexture"))
				null_texture_ = true;

			// Patch
			else if (tz.checkNC("Patch"))
			{
				CTPatchEx* patch = new CTPatchEx();
				patch->parse(tz);
				patches_.emplace_back(patch);
			}

			// Graphic
			else if (tz.checkNC("Graphic"))
			{
				CTPatchEx* patch = new CTPatchEx();
				patch->parse(tz, CTPatchEx::Type::Graphic);
				patches_.emplace_back(patch);
			}

			// Read next property
			tz.adv();
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Parses a HIRESTEX define block
// -----------------------------------------------------------------------------
bool CTexture::parseDefine(Tokenizer& tz)
{
	this->type_     = "Define";
	this->extended_ = true;
	this->defined_  = true;
	name_           = StrUtil::upper(tz.next().text);
	def_width_      = tz.next().asInt();
	def_height_     = tz.next().asInt();
	width_          = def_width_;
	height_         = def_height_;

	ArchiveEntry* entry = theResourceManager->getPatchEntry(name_);
	if (entry)
	{
		SImage image;
		if (image.open(entry->data()))
		{
			width_   = image.width();
			height_  = image.height();
			scale_x_ = (double)width_ / (double)def_width_;
			scale_y_ = (double)height_ / (double)def_height_;
		}
	}

	patches_.emplace_back(new CTPatchEx(name_));

	return true;
}

// -----------------------------------------------------------------------------
// Returns a string representation of the texture, in ZDoom TEXTURES format
// -----------------------------------------------------------------------------
string CTexture::asText()
{
	// Can't write non-extended texture as text
	if (!extended_)
		return "";

	// Define block
	if (defined_)
		return S_FMT("define \"%s\" %d %d\n", name_, def_width_, def_height_);

	// Init text string
	string text;
	if (optional_)
		text = S_FMT("%s Optional \"%s\", %d, %d\n{\n", type_, name_, width_, height_);
	else
		text = S_FMT("%s \"%s\", %d, %d\n{\n", type_, name_, width_, height_);

	// Write texture properties
	if (scale_x_ != 1.0)
		text += S_FMT("\tXScale %1.3f\n", scale_x_);
	if (scale_y_ != 1.0)
		text += S_FMT("\tYScale %1.3f\n", scale_y_);
	if (offset_x_ != 0 || offset_y_ != 0)
		text += S_FMT("\tOffset %d, %d\n", offset_x_, offset_y_);
	if (world_panning_)
		text += "\tWorldPanning\n";
	if (no_decals_)
		text += "\tNoDecals\n";
	if (null_texture_)
		text += "\tNullTexture\n";

	// Write patches
	for (auto& patch : patches_)
		text += ((CTPatchEx*)patch.get())->asText();

	// Write ending
	text += "}\n\n";

	return text;
}

// -----------------------------------------------------------------------------
// Converts the texture to 'extended' (ZDoom TEXTURES) format
// -----------------------------------------------------------------------------
bool CTexture::convertExtended()
{
	// Simple conversion system for defines
	if (defined_)
		defined_ = false;

	// Don't convert if already extended
	if (extended_)
		return true;

	// Convert scale if needed
	if (scale_x_ == 0)
		scale_x_ = 1;
	if (scale_y_ == 0)
		scale_y_ = 1;

	// Convert all patches over to extended format
	for (auto& patch : patches_)
	{
		auto expatch = new CTPatchEx(patch.get());
		patch.reset(expatch);
	}

	// Set extended flag
	extended_ = true;

	return true;
}

// -----------------------------------------------------------------------------
// Converts the texture to 'regular' (TEXTURE1/2) format
// -----------------------------------------------------------------------------
bool CTexture::convertRegular()
{
	// Don't convert if already regular
	if (!extended_)
		return true;

	// Convert scale
	if (scale_x_ == 1)
		scale_x_ = 0;
	else
		scale_x_ *= 8;
	if (scale_y_ == 1)
		scale_y_ = 0;
	else
		scale_y_ *= 8;

	// Convert all patches over to normal format
	for (auto& patch : patches_)
	{
		auto npatch = new CTPatch(patch.get());
		patch.reset(npatch);
	}

	// Unset extended flag
	extended_ = false;
	defined_  = false;

	return true;
}

// -----------------------------------------------------------------------------
// Generates a SImage representation of this texture, using patches from
// [parent] primarily, and the palette [pal]
// -----------------------------------------------------------------------------
bool CTexture::toImage(SImage& image, Archive* parent, Palette* pal, bool force_rgba)
{
	// Init image
	image.clear();
	image.resize(width_, height_);

	// Add patches
	SImage            p_img(SImage::Type::PalMask);
	SImage::DrawProps dp;
	dp.src_alpha = false;
	if (defined_)
	{
		if (!loadPatchImage(0, p_img, parent, pal))
			return false;
		width_  = p_img.width();
		height_ = p_img.height();
		image.resize(width_, height_);
		scale_x_ = (double)width_ / (double)def_width_;
		scale_y_ = (double)height_ / (double)def_height_;
		image.drawImage(p_img, 0, 0, dp, pal, pal);
	}
	else if (extended_)
	{
		// Extended texture

		// Add each patch to image
		for (unsigned a = 0; a < patches_.size(); a++)
		{
			auto patch = (CTPatchEx*)patches_[a].get();

			// Load patch entry
			if (!loadPatchImage(a, p_img, parent, pal))
				continue;

			// Handle offsets
			int ofs_x = patch->xOffset();
			int ofs_y = patch->yOffset();
			if (patch->useOffsets())
			{
				ofs_x -= p_img.offset().x;
				ofs_y -= p_img.offset().y;
			}

			// Apply translation before anything in case we're forcing rgba (can't translate rgba images)
			if (patch->blendType() == 1)
				p_img.applyTranslation(&(patch->translation()), pal, force_rgba);

			// Convert to RGBA if forced
			if (force_rgba)
				p_img.convertRGBA(pal);

			// Flip/rotate if needed
			if (patch->flipX())
				p_img.mirror(false);
			if (patch->flipY())
				p_img.mirror(true);
			if (patch->rotation() != 0)
				p_img.rotate(patch->rotation());

			// Setup transparency blending
			dp.blend     = SImage::Blend::Normal;
			dp.alpha     = 1.0f;
			dp.src_alpha = false;
			if (patch->style() == "CopyAlpha" || patch->style() == "Overlay")
				dp.src_alpha = true;
			else if (patch->style() == "Translucent" || patch->style() == "CopyNewAlpha")
				dp.alpha = patch->alpha();
			else if (patch->style() == "Add")
			{
				dp.blend = SImage::Blend::Add;
				dp.alpha = patch->alpha();
			}
			else if (patch->style() == "Subtract")
			{
				dp.blend = SImage::Blend::Subtract;
				dp.alpha = patch->alpha();
			}
			else if (patch->style() == "ReverseSubtract")
			{
				dp.blend = SImage::Blend::ReverseSubtract;
				dp.alpha = patch->alpha();
			}
			else if (patch->style() == "Modulate")
			{
				dp.blend = SImage::Blend::Modulate;
				dp.alpha = patch->alpha();
			}

			// Setup patch colour
			if (patch->blendType() == 2)
				p_img.colourise(patch->colour(), pal);
			else if (patch->blendType() == 3)
				p_img.tint(patch->colour(), patch->colour().fa(), pal);


			// Add patch to texture image
			image.drawImage(p_img, ofs_x, ofs_y, dp, pal, pal);
		}
	}
	else
	{
		// Normal texture

		// Add each patch to image
		for (auto& patch : patches_)
		{
			if (Misc::loadImageFromEntry(&p_img, patch->getPatchEntry(parent)))
				image.drawImage(p_img, patch->xOffset(), patch->yOffset(), dp, pal, pal);
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Loads the image for the patch at [pindex] into [image].
// Can deal with textures-as-patches
// -----------------------------------------------------------------------------
bool CTexture::loadPatchImage(unsigned pindex, SImage& image, Archive* parent, Palette* pal)
{
	// Check patch index
	if (pindex >= patches_.size())
		return false;

	const auto& patch = patches_[pindex];

	// If the texture is extended, search for textures-as-patches first
	// (as long as the patch name is different from this texture's name)
	if (extended_ && !(StrUtil::equalCI(patch->name(), name_)))
	{
		// Search the texture list we're in first
		if (in_list_)
		{
			for (unsigned a = 0; a < in_list_->nTextures(); a++)
			{
				CTexture* tex = in_list_->getTexture(a);

				// Don't look past this texture in the list
				if (tex->name() == name_)
					break;

				// Check for name match
				if (StrUtil::equalCI(tex->name(), patch->name()))
				{
					// Load texture to image
					return tex->toImage(image, parent, pal);
				}
			}
		}

		// Otherwise, try the resource manager
		// TODO: Something has to be ignored here. The entire archive or just the current list?
		CTexture* tex = theResourceManager->getTexture(patch->name(), parent);
		if (tex)
			return tex->toImage(image, parent, pal);
	}

	// Get patch entry
	ArchiveEntry* entry = patch->getPatchEntry(parent);

	// Load entry to image if valid
	if (entry)
		return Misc::loadImageFromEntry(&image, entry);
	else
		return false;
}
