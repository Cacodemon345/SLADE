
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    TextureXList.cpp
// Description: Handles a collection of Composite Textures (ie, encapsulates a
//              TEXTUREx entry)
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
#include "Graphics/SImage/SImage.h"
#include "MainEditor/MainEditor.h"
#include "PatchTable.h"
#include "TextureXList.h"
#include "Utility/StringUtils.h"
#include "Utility/Tokenizer.h"


// -----------------------------------------------------------------------------
//
// Structs
//
// -----------------------------------------------------------------------------
namespace
{
// Some structs for reading TEXTUREx data
struct tdef_t
{
	// Just the data relevant to SLADE3
	char     name[8];
	uint16_t flags;
	uint8_t  scale[2];
	int16_t  width;
	int16_t  height;
};

struct nltdef_t
{
	// The nameless version used by Doom Alpha 0.4
	uint16_t flags;
	uint8_t  scale[2];
	int16_t  width;
	int16_t  height;
	int16_t  columndir[2];
	int16_t  patchcount;
};

struct ftdef_t
{
	// The full version with some useless data
	char     name[8];
	uint16_t flags;
	uint8_t  scale[2];
	int16_t  width;
	int16_t  height;
	int16_t  columndir[2];
	int16_t  patchcount;
};

struct stdef_t
{
	// The Strife version with less useless data
	char     name[8];
	uint16_t flags;
	uint8_t  scale[2];
	int16_t  width;
	int16_t  height;
	int16_t  patchcount;
};
} // namespace


// -----------------------------------------------------------------------------
//
// TextureXList Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// TextureXList class constructor
// -----------------------------------------------------------------------------
TextureXList::TextureXList(Format format) : format_{ format }
{
	// Setup 'invalid' texture
	tex_invalid_.setName("INVALID_TEXTURE"); // Deliberately set the name to >8 characters
}

// -----------------------------------------------------------------------------
// Returns the texture at [index], or the 'invalid' texture if [index] is out of
// range
// -----------------------------------------------------------------------------
CTexture* TextureXList::getTexture(size_t index)
{
	// Check index
	if (index >= textures_.size())
		return &tex_invalid_;

	// Return texture at index
	return textures_[index].get();
}

// -----------------------------------------------------------------------------
// Returns the texture matching [name], or the 'invalid' texture if no match is
// found
// -----------------------------------------------------------------------------
CTexture* TextureXList::getTexture(string name)
{
	// Search for texture by name
	for (auto& texture : textures_)
	{
		if (StrUtil::equalCI(texture->name(), name))
			return texture.get();
	}

	// Not found
	return &tex_invalid_;
}

// -----------------------------------------------------------------------------
// Returns the index of the texture matching [name], or -1 if no match was found
// -----------------------------------------------------------------------------
int TextureXList::textureIndex(string name)
{
	// Search for texture by name
	for (unsigned a = 0; a < textures_.size(); a++)
	{
		if (StrUtil::equalCI(textures_[a]->name(), name))
		{
			textures_[a]->index_ = a;
			return a;
		}
	}

	// Not found
	return -1;
}

// -----------------------------------------------------------------------------
// Adds [tex] to the texture list at [position]
// -----------------------------------------------------------------------------
void TextureXList::addTexture(CTexture::UPtr tex, int position)
{
	tex->in_list_ = this;

	// Add it to the list at position if valid
	if (position >= 0 && (unsigned)position < textures_.size())
	{
		tex->index_ = position;
		textures_.insert(textures_.begin() + position, std::move(tex));
	}
	else
	{
		tex->index_ = textures_.size();
		textures_.push_back(std::move(tex));
	}
}

// -----------------------------------------------------------------------------
// Removes the texture at [index] from the list
// -----------------------------------------------------------------------------
CTexture::UPtr TextureXList::removeTexture(unsigned index)
{
	// Check index
	if (index >= textures_.size())
		return nullptr;

	auto removed = std::move(textures_[index]);
	textures_.erase(textures_.begin() + index);

	return removed;
}

// -----------------------------------------------------------------------------
// Swaps the texture at [index1] with the texture at [index2]
// -----------------------------------------------------------------------------
void TextureXList::swapTextures(unsigned index1, unsigned index2)
{
	// Check indices
	if (index1 >= textures_.size() || index2 >= textures_.size())
		return;

	// Swap them
	textures_[index1].swap(textures_[index2]);

	// Swap indices
	int ti                    = textures_[index1]->index_;
	textures_[index1]->index_ = textures_[index2]->index_;
	textures_[index2]->index_ = ti;
}

// -----------------------------------------------------------------------------
// Replaces the texture at [index] with [replacement] and returns the original
// texture that was replaced (or NULL if index was invalid)
// -----------------------------------------------------------------------------
CTexture::UPtr TextureXList::replaceTexture(unsigned index, CTexture::UPtr replacement)
{
	// Check index
	if (index >= textures_.size())
		return nullptr;

	auto replaced    = std::move(textures_[index]);
	textures_[index] = std::move(replacement);

	return replaced;
}

// -----------------------------------------------------------------------------
// Clears all textures
// -----------------------------------------------------------------------------
void TextureXList::clear(bool clear_patches)
{
	// Clear textures list
	textures_.clear();
}

// -----------------------------------------------------------------------------
// Updates all textures in the list to 'remove' [patch]
// -----------------------------------------------------------------------------
void TextureXList::removePatch(string patch)
{
	// Go through all textures
	for (auto& texture : textures_)
		texture->removePatch(patch); // Remove patch from texture
}

// -----------------------------------------------------------------------------
// Reads in a doom-format TEXTUREx entry.
// Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool TextureXList::readTEXTUREXData(ArchiveEntry* texturex, PatchTable& patch_table, bool add)
{
	// Check entries were actually given
	if (!texturex)
		return false;

	// Clear current textures if needed
	if (!add)
		clear();

	// Update palette
	MainEditor::setGlobalPaletteFromArchive(texturex->parent());

	// Read TEXTUREx

	// Read header
	texturex->seek(0, SEEK_SET);
	int32_t n_tex = 0;

	// Number of textures
	if (!texturex->read(&n_tex, 4))
	{
		Log::info(1, "Error: TEXTUREx entry is corrupt (can't read texture count)");
		return false;
	}
	n_tex = wxINT32_SWAP_ON_BE(n_tex);

	// If it's an empty TEXTUREx entry, stop here
	if (n_tex == 0)
		return true;

	// Texture definition offsets
	int32_t* offsets = new int32_t[n_tex];
	if (!texturex->read(offsets, n_tex * 4))
	{
		Log::info(1, "Error: TEXTUREx entry is corrupt (can't read first offset)");
		return false;
	}

	// Read the first texture definition to try to identify the format
	if (!texturex->seek(wxINT32_SWAP_ON_BE(offsets[0]), SEEK_SET))
	{
		Log::info(1, "Error: TEXTUREx entry is corrupt (can't read first definition)");
		return false;
	}
	// Look at the name field. Is it present or not?
	char tempname[8];
	if (!texturex->read(&tempname, 8))
	{
		Log::info(1, "Error: TEXTUREx entry is corrupt (can't read first name)");
		return false;
	}
	// Let's pretend it is and see what happens.
	format_ = Format::Normal;

	// Only the characters A-Z (uppercase), 0-9, and [ ] - _ should be used in texture names.
	for (uint8_t a = 0; a < 8; ++a)
	{
		if (a > 0 && tempname[a] == 0)
			// We found a null-terminator for the string, so we can assume it's okay.
			break;
		if (tempname[a] >= 'a' && tempname[a] <= 'z')
		{
			format_ = Format::Jaguar;
			// Log::info(1, "Jaguar texture");
			break;
		}
		else if (!((tempname[a] >= 'A' && tempname[a] <= '[') || (tempname[a] >= '0' && tempname[a] <= '9')
				   || tempname[a] == ']' || tempname[a] == '-' || tempname[a] == '_'))
		// We're out of character range, so this is probably not a texture name.
		{
			format_ = Format::Nameless;
			// Log::info(1, "Nameless texture");
			break;
		}
	}

	// Now let's see if it is the abridged Strife format or not.
	if (format_ == Format::Normal)
	{
		// No need to test this again since it was already tested before.
		texturex->seek(offsets[0], SEEK_SET);
		ftdef_t temp;
		if (!texturex->read(&temp, 22))
		{
			Log::info(1, "Error: TEXTUREx entry is corrupt (can't test definition)");
			return false;
		}
		// Test condition adapted from ZDoom; apparently the first two bytes of columndir
		// may be set to garbage values by some editors and are therefore unreliable.
		if (wxINT16_SWAP_ON_BE(temp.patchcount <= 0) || (temp.columndir[1] != 0))
			format_ = Format::Strife11;
	}

	// Read all texture definitions
	for (int32_t a = 0; a < n_tex; a++)
	{
		// Skip to texture definition
		if (!texturex->seek(offsets[a], SEEK_SET))
		{
			Log::info(1, "Error: TEXTUREx entry is corrupt (can't find definition)");
			return false;
		}

		// Read definition
		tdef_t tdef;
		if (format_ == Format::Nameless)
		{
			nltdef_t nameless;
			// Auto-naming mechanism taken from DeuTex
			if (a > 99999)
			{
				Log::info(1, "Error: More than 100000 nameless textures");
				return false;
			}
			char temp[9] = "";
			sprintf(temp, "TEX%05d", a);
			memcpy(tdef.name, temp, 8);

			// Read texture info
			if (!texturex->read(&nameless, 8))
			{
				Log::info(fmt::format("Error: TEXTUREx entry is corrupt (can't read nameless definition #{})", a));
				return false;
			}

			// Copy data to permanent structure
			tdef.flags    = nameless.flags;
			tdef.scale[0] = nameless.scale[0];
			tdef.scale[1] = nameless.scale[1];
			tdef.width    = nameless.width;
			tdef.height   = nameless.height;
		}
		else if (!texturex->read(&tdef, 16))
		{
			Log::info(fmt::format("Error: TEXTUREx entry is corrupt, (can't read texture definition #{})", a));
			return false;
		}

		// Skip unused
		if (format_ != Format::Strife11)
		{
			if (!texturex->seek(4, SEEK_CUR))
			{
				Log::info(fmt::format("Error: TEXTUREx entry is corrupt (can't skip dummy data past #{})", a));
				return false;
			}
		}

		// Create texture
		// CTexture* tex = new CTexture();
		auto tex      = std::make_unique<CTexture>();
		tex->name_    = wxString::FromAscii(tdef.name, 8);
		tex->width_   = wxINT16_SWAP_ON_BE(tdef.width);
		tex->height_  = wxINT16_SWAP_ON_BE(tdef.height);
		tex->scale_x_ = tdef.scale[0] / 8.0;
		tex->scale_y_ = tdef.scale[1] / 8.0;

		// Set flags
		if (tdef.flags & TX_WORLDPANNING)
			tex->world_panning_ = true;

		// Read patches
		int16_t n_patches = 0;
		if (!texturex->read(&n_patches, 2))
		{
			Log::info(fmt::format("Error: TEXTUREx entry is corrupt (can't read patchcount #{})", a));
			// delete tex;
			return false;
		}

		// Log::info(1, "Texture #%d: %d patch%s", a, n_patches, n_patches == 1 ? "" : "es");

		for (uint16_t p = 0; p < n_patches; p++)
		{
			// Read patch definition
			Patch pdef;
			if (!texturex->read(&pdef, 6))
			{
				Log::info(fmt::format("Error: TEXTUREx entry is corrupt (can't read patch definition #{}:{})", a, p));
				Log::info(fmt::format("Lump size {}, offset {}", texturex->size(), texturex->currentPos()));
				return false;
			}

			// Skip unused
			if (format_ != Format::Strife11)
			{
				if (!texturex->seek(4, SEEK_CUR))
				{
					Log::info(
						fmt::format("Error: TEXTUREx entry is corrupt (can't skip dummy data past #{}:{})", a, p));
					return false;
				}
			}


			// Add it to the texture
			string patch;
			if (format_ == Format::Jaguar)
			{
				patch = StrUtil::upper(tex->name_);
			}
			else
			{
				patch = patch_table.patchName(pdef.patch);
			}
			if (patch.empty())
			{
				// Log::info(1, "Warning: Texture %s contains patch %d which is invalid - may be incorrect PNAMES
				// entry", tex->getName(), pdef.patch);
				patch = fmt::format("INVPATCH{:04d}", pdef.patch);
			}

			tex->addPatch(patch, pdef.left, pdef.top);
		}

		// Add texture to list
		addTexture(std::move(tex));
	}

	// Clean up
	delete[] offsets;

	return true;
}

#define SAFEFUNC(x) \
	if (!(x))       \
		return false;

// -----------------------------------------------------------------------------
// Writes the texture list in TEXTUREX format to [texturex], using [patch_table]
// for patch information. Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool TextureXList::writeTEXTUREXData(ArchiveEntry* texturex, PatchTable& patch_table)
{
	// Check entry was given
	if (!texturex)
		return false;

	if (texturex->isLocked())
		return false;

	Log::info(1, "Writing " + getTextureXFormatString() + " format TEXTUREx entry");

	/* Total size of a TEXTUREx lump, in bytes:
		Header: 4 + (4 * numtextures)
		Textures:
			22 * numtextures (normal format)
			14 * numtextures (nameless format)
			18 * numtextures (Strife 1.1 format)
		Patches:
			10 * sum of patchcounts (normal and nameless formats)
			 6 * sum of patchcounts (Strife 1.1 format)
	*/
	size_t numpatchrefs = 0;
	size_t numtextures  = textures_.size();
	for (size_t i = 0; i < numtextures; ++i)
	{
		numpatchrefs += textures_[i]->nPatches();
	}
	Log::info(fmt::format("{} patch references in {} textures", numpatchrefs, numtextures));

	size_t datasize;
	// size_t headersize = 4 + (4 * numtextures);
	switch (format_)
	{
	case Format::Normal: datasize = 4 + (26 * numtextures) + (10 * numpatchrefs); break;
	case Format::Nameless: datasize = 4 + (18 * numtextures) + (10 * numpatchrefs); break;
	case Format::Strife11:
		datasize = 4 + (22 * numtextures) + (6 * numpatchrefs);
		break;
		// Some compilers insist on having default cases.
	default: return false;
	}

	MemChunk        txdata(datasize);
	vector<int32_t> offsets(numtextures);
	int32_t         foo = wxINT32_SWAP_ON_BE((signed)numtextures);

	// Write header
	txdata.seek(0, SEEK_SET);
	SAFEFUNC(txdata.write(&foo, 4));

	// Go to beginning of texture definitions
	SAFEFUNC(txdata.seek(4 + (numtextures * 4), SEEK_SET));

	// Write texture entries
	for (size_t i = 0; i < numtextures; ++i)
	{
		// Get texture to write
		const auto& tex = textures_[i];

		// Set offset
		offsets[i] = (signed)txdata.currentPos();

		// Write texture entry
		switch (format_)
		{
		case Format::Normal:
		{
			// Create 'normal' doom format texture definition
			ftdef_t txdef;
			memset(txdef.name, 0, 8); // Set texture name to all 0's (to ensure compatibility with XWE)
			auto upper_name = StrUtil::upper(tex->name());
			strncpy(txdef.name, upper_name.data(), upper_name.size());
			txdef.flags        = 0;
			txdef.scale[0]     = (tex->scaleX() * 8);
			txdef.scale[1]     = (tex->scaleY() * 8);
			txdef.width        = tex->width();
			txdef.height       = tex->height();
			txdef.columndir[0] = 0;
			txdef.columndir[1] = 0;
			txdef.patchcount   = tex->nPatches();

			// Check for WorldPanning flag
			if (tex->world_panning_)
				txdef.flags |= TX_WORLDPANNING;

			// Write texture definition
			SAFEFUNC(txdata.write(&txdef, 22));

			break;
		}
		case Format::Nameless:
		{
			// Create nameless texture definition
			nltdef_t txdef;
			txdef.flags        = 0;
			txdef.scale[0]     = (tex->scaleX() * 8);
			txdef.scale[1]     = (tex->scaleY() * 8);
			txdef.width        = tex->width();
			txdef.height       = tex->height();
			txdef.columndir[0] = 0;
			txdef.columndir[1] = 0;
			txdef.patchcount   = tex->nPatches();

			// Write texture definition
			SAFEFUNC(txdata.write(&txdef, 8));

			break;
		}
		case Format::Strife11:
		{
			// Create strife format texture definition
			stdef_t txdef;
			memset(txdef.name, 0, 8); // Set texture name to all 0's (to ensure compatibility with XWE)
			auto upper_name = StrUtil::upper(tex->name());
			strncpy(txdef.name, upper_name.data(), upper_name.size());
			txdef.flags      = 0;
			txdef.scale[0]   = (tex->scaleX() * 8);
			txdef.scale[1]   = (tex->scaleY() * 8);
			txdef.width      = tex->width();
			txdef.height     = tex->height();
			txdef.patchcount = tex->nPatches();

			// Check for WorldPanning flag
			if (tex->world_panning_)
				txdef.flags |= TX_WORLDPANNING;

			// Write texture definition
			SAFEFUNC(txdata.write(&txdef, 18));

			break;
		}
		default: return false;
		}

		// Write patch references
		for (size_t k = 0; k < tex->nPatches(); ++k)
		{
			// Get patch to write
			CTPatch* patch = tex->patch(k);

			// Create patch definition
			Patch pdef{ patch->xOffset(), patch->yOffset(), 0 };

			// Check for 'invalid' patch
			if (StrUtil::startsWith(patch->name(), "INVPATCH"))
			{
				// Get raw patch index from name
				pdef.patch = StrUtil::toInt(StrUtil::replace(patch->name(), "INVPATCH", ""));
			}
			else
				pdef.patch = patch_table.patchIndex(
					patch->name()); // Note this will be -1 if the patch doesn't exist in the patch table. This should
									// never happen with the texture editor, though.

			// Write common data
			SAFEFUNC(txdata.write(&pdef, 6));

			// In non-Strife formats, there's some added rubbish
			if (format_ != Format::Strife11)
			{
				foo = 0;
				SAFEFUNC(txdata.write(&foo, 4));
			}
		}
	}

	// Write offsets
	SAFEFUNC(txdata.seek(4, SEEK_SET));
	SAFEFUNC(txdata.write(offsets.data(), 4 * numtextures));

	// Write data to the TEXTUREx entry
	texturex->importMemChunk(txdata);

	// Update entry type
	EntryType::detectEntryType(texturex);

	return true;
}

// -----------------------------------------------------------------------------
// Reads in a ZDoom-format TEXTURES entry.
// Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool TextureXList::readTEXTURESData(ArchiveEntry* entry)
{
	// Check for empty entry
	if (!entry)
	{
		Global::error = "Attempt to read texture data from NULL entry";
		return false;
	}
	if (entry->size() == 0)
	{
		format_ = Format::Textures;
		return true;
	}

	// Get text to parse
	Tokenizer tz;
	tz.openMem(entry->data(), entry->name());

	// Parsing gogo
	while (!tz.atEnd())
	{
		auto tex = std::make_unique<CTexture>();

		// Texture definition
		if (tz.checkNC("Texture"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parse(tz, "Texture"))
				addTexture(std::move(tex));
		}

		// Sprite definition
		else if (tz.checkNC("Sprite"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parse(tz, "Sprite"))
				addTexture(std::move(tex));
		}

		// Graphic definition
		else if (tz.checkNC("Graphic"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parse(tz, "Graphic"))
				addTexture(std::move(tex));
		}

		// WallTexture definition
		else if (tz.checkNC("WallTexture"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parse(tz, "WallTexture"))
				addTexture(std::move(tex));
		}

		// Flat definition
		else if (tz.checkNC("Flat"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parse(tz, "Flat"))
				addTexture(std::move(tex));
		}

		// Old HIRESTEX "Define"
		else if (tz.checkNC("Define"))
		{
			// CTexture* tex = new CTexture();
			if (tex->parseDefine(tz))
				addTexture(std::move(tex));
		}

		tz.adv();
	}

	format_ = Format::Textures;

	return true;
}

// -----------------------------------------------------------------------------
// Writes the texture list in TEXTURES format to [entry].
// Returns true on success, false otherwise
// -----------------------------------------------------------------------------
bool TextureXList::writeTEXTURESData(ArchiveEntry* entry)
{
	// Check format
	if (format_ != Format::Textures)
		return false;

	Log::info(1, "Writing ZDoom text format TEXTURES entry");

	// Generate a big string :P
	string textures_data = "// Texture definitions generated by SLADE3\n// on " + wxNow().ToStdString() + "\n\n";
	for (unsigned a = 0; a < textures_.size(); a++)
		textures_data += textures_[a]->asText();
	textures_data += "// End of texture definitions\n";

	Log::info(fmt::format(
		"{} texture%s written on {} bytes", textures_.size(), textures_.size() < 2 ? "" : "s", textures_data.length()));

	// Write it to the entry
	return entry->importMem(textures_data.c_str(), textures_data.length());
}

// -----------------------------------------------------------------------------
// Returns a string representation of the texture list format
// -----------------------------------------------------------------------------
string TextureXList::getTextureXFormatString() const
{
	switch (format_)
	{
	case Format::Normal: return "Doom TEXTUREx";
	case Format::Strife11: return "Strife TEXTUREx";
	case Format::Nameless: return "Nameless (Doom Alpha)";
	case Format::Textures: return "ZDoom TEXTURES";
	default: return "Unknown";
	}
}

// -----------------------------------------------------------------------------
// Converts all textures in the list to extended TEXTURES format
// -----------------------------------------------------------------------------
bool TextureXList::convertToTEXTURES()
{
	// Check format is appropriate
	if (format_ == Format::Textures)
	{
		Global::error = "Already TEXTURES format";
		return false;
	}

	// Convert all textures to extended format
	for (auto& texture : textures_)
		texture->convertExtended();

	// First texture is null texture
	textures_[0]->null_texture_ = true;

	// Set new format
	format_ = Format::Textures;

	return true;
}

// -----------------------------------------------------------------------------
// Search for errors in texture list, return true if any are found
// -----------------------------------------------------------------------------
bool TextureXList::findErrors()
{
	bool ret = false;

	// Texture errors:
	// 1. A texture without any patch
	// 2. A texture with missing patches
	// 3. A texture with columns not covered by a patch

	for (unsigned a = 0; a < textures_.size(); a++)
	{
		if (textures_[a]->nPatches() == 0)
		{
			ret = true;
			Log::info(fmt::format("Texture {}: {} does not have any patch", a, textures_[a]->name()));
		}
		else
		{
			bool* columns = new bool[textures_[a]->width()];
			memset(columns, false, textures_[a]->width());
			for (size_t i = 0; i < textures_[a]->nPatches(); ++i)
			{
				ArchiveEntry* patch = textures_[a]->patches_[i]->getPatchEntry();
				if (patch == nullptr)
				{
					ret = true;
					Log::info(fmt::format(
						"Texture {}: {}: patch {} cannot be found in any open archive",
						a,
						textures_[a]->name(),
						textures_[a]->patches_[i]->name()));
					// Don't list missing columns when we don't know the size of the patch
					memset(columns, true, textures_[a]->width());
				}
				else
				{
					SImage img;
					img.open(patch->data());
					size_t start = std::max<int>(0, (int)textures_[a]->patches_[i]->xOffset());
					size_t end   = std::max<int>((int)textures_[a]->width(), img.width() + start);
					for (size_t c = start; c < end; ++c)
						columns[c] = true;
				}
			}
			for (size_t c = 0; c < textures_[a]->width(); ++c)
			{
				if (!columns[c])
				{
					ret = true;
					Log::info(fmt::format("Texture {}: {}: column {} without a patch", a, textures_[a]->name(), c));
					break;
				}
			}
			delete[] columns;
		}
	}
	return ret;
}
