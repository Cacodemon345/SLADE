
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    EntryOperations.cpp
// Description: Functions that perform specific operations on entries
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
#include "App.h"
#include "Archive/ArchiveManager.h"
#include "Archive/EntryType/EntryDataFormat.h"
#include "Archive/Formats/WadArchive.h"
#include "BinaryControlLump.h"
#include "Dialogs/ExtMessageDialog.h"
#include "Dialogs/ModifyOffsetsDialog.h"
#include "Dialogs/Preferences/PreferencesDialog.h"
#include "EntryOperations.h"
#include "General/Console/Console.h"
#include "General/Misc.h"
#include "MainEditor/MainEditor.h"
#include "UI/TextureXEditor/TextureXEditor.h"
#include "Utility/FileMonitor.h"
#include "Utility/StringUtils.h"
#include "Utility/Tokenizer.h"
#include "UI/WxUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(String, path_acc, "", CVAR_SAVE);
CVAR(String, path_acc_libs, "", CVAR_SAVE);
CVAR(String, path_pngout, "", CVAR_SAVE);
CVAR(String, path_pngcrush, "", CVAR_SAVE);
CVAR(String, path_deflopt, "", CVAR_SAVE);
CVAR(String, path_db2, "", CVAR_SAVE)
CVAR(Bool, acc_always_show_output, false, CVAR_SAVE);


// -----------------------------------------------------------------------------
//
// Structs
//
// -----------------------------------------------------------------------------
namespace
{
// Define some png chunk structures
struct IHDR
{
	uint32_t id;
	uint32_t width;
	uint32_t height;
	uint8_t  cinfo[5];
};

struct GrabChunk
{
	char    name[4];
	int32_t xoff;
	int32_t yoff;
};

struct AlphChunk
{
	char name[4];
};

struct TrnsChunk
{
	char    name[4];
	uint8_t entries[256];
};

struct TransChunk
{
	char name[5];
};
} // namespace


// -----------------------------------------------------------------------------
//
// EntryOperations Namespace Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Converts the image [entry] to [target_format], using conversion options
// specified in [opt] and converting to [target_colformat] colour format if
// possible.
// Returns false if the conversion failed, true otherwise
// -----------------------------------------------------------------------------
bool EntryOperations::gfxConvert(
	ArchiveEntry*            entry,
	string                   target_format,
	SIFormat::ConvertOptions opt,
	SImage::Type             target_colformat)
{
	// Init variables
	SImage image;

	// Get target image format
	SIFormat* fmt = SIFormat::getFormat(target_format);
	if (fmt == SIFormat::unknownFormat())
		return false;

	// Check format and target colour type are compatible
	if (target_colformat != SImage::Type::Any && !fmt->canWriteType(target_colformat))
	{
		if (target_colformat == SImage::Type::RGBA)
			Log::error(fmt::sprintf("Format \"%s\" cannot be written as RGBA data", fmt->name()));
		else if (target_colformat == SImage::Type::PalMask)
			Log::error(fmt::sprintf("Format \"%s\" cannot be written as paletted data", fmt->name()));

		return false;
	}

	// Load entry to image
	Misc::loadImageFromEntry(&image, entry);

	// Check if we can write the image to the target format
	int writable = fmt->canWrite(image);
	if (writable == SIFormat::NOTWRITABLE)
	{
		Log::error(
			fmt::sprintf("Entry \"%s\" could not be converted to target format \"%s\"", entry->name(), fmt->name()));
		return false;
	}
	else if (writable == SIFormat::CONVERTIBLE)
		fmt->convertWritable(image, opt);

	// Now we apply the target colour format (if any)
	if (target_colformat == SImage::Type::PalMask)
		image.convertPaletted(opt.pal_target, opt.pal_current);
	else if (target_colformat == SImage::Type::RGBA)
		image.convertRGBA(opt.pal_current);

	// Finally, write new image data back to the entry
	fmt->saveImage(image, entry->data(), opt.pal_target);

	return true;
}

// -----------------------------------------------------------------------------
// Changes the offsets of the given gfx entry, based on settings selected in
// [dialog].
// Returns false if the entry is invalid or not an offset-supported format, true
// otherwise
// -----------------------------------------------------------------------------
bool EntryOperations::modifyGfxOffsets(ArchiveEntry* entry, ModifyOffsetsDialog* dialog)
{
	if (entry == nullptr || entry->type() == nullptr)
		return false;

	// Check entry type
	EntryType* type        = entry->type();
	string     entryformat = type->formatId();
	if (!(entryformat == "img_doom" || entryformat == "img_doom_arah" || entryformat == "img_doom_alpha"
		  || entryformat == "img_doom_beta" || entryformat == "img_png"))
	{
		Log::error(fmt::sprintf(
			"Entry \"%s\" is of type \"%s\" which does not support offsets", entry->name(), entry->type()->name()));
		return false;
	}

	// Doom gfx format, normal and beta version.
	// Also arah format from alpha 0.2 because it uses the same header format.
	if (entryformat == "img_doom" || entryformat == "img_doom_beta" || entryformat == "image_doom_arah")
	{
		// Get patch header
		patch_header_t header;
		entry->seek(0, SEEK_SET);
		entry->read(&header, 8);

		// Calculate new offsets
		point2_t offsets = dialog->calculateOffsets(header.left, header.top, header.width, header.height);

		// Apply new offsets
		header.left = wxINT16_SWAP_ON_BE((int16_t)offsets.x);
		header.top  = wxINT16_SWAP_ON_BE((int16_t)offsets.y);

		// Write new header to entry
		entry->seek(0, SEEK_SET);
		entry->write(&header, 8);
	}

	// Doom alpha gfx format
	else if (entryformat == "img_doom_alpha")
	{
		// Get patch header
		entry->seek(0, SEEK_SET);
		oldpatch_header_t header;
		entry->read(&header, 4);

		// Calculate new offsets
		point2_t offsets = dialog->calculateOffsets(header.left, header.top, header.width, header.height);

		// Apply new offsets
		header.left = (int8_t)offsets.x;
		header.top  = (int8_t)offsets.y;

		// Write new header to entry
		entry->seek(0, SEEK_SET);
		entry->write(&header, 4);
	}

	// PNG format
	else if (entryformat == "img_png")
	{
		// Read width and height from IHDR chunk
		const uint8_t* data = entry->dataRaw(true);
		const IHDR*    ihdr = (IHDR*)(data + 12);
		uint32_t       w    = wxINT32_SWAP_ON_LE(ihdr->width);
		uint32_t       h    = wxINT32_SWAP_ON_LE(ihdr->height);

		// Find existing grAb chunk
		uint32_t grab_start = 0;
		int32_t  xoff       = 0;
		int32_t  yoff       = 0;
		for (uint32_t a = 0; a < entry->size(); a++)
		{
			// Check for 'grAb' header
			if (data[a] == 'g' && data[a + 1] == 'r' && data[a + 2] == 'A' && data[a + 3] == 'b')
			{
				grab_start      = a - 4;
				GrabChunk* grab = (GrabChunk*)(data + a);
				xoff            = wxINT32_SWAP_ON_LE(grab->xoff);
				yoff            = wxINT32_SWAP_ON_LE(grab->yoff);
				break;
			}

			// Stop when we get to the 'IDAT' chunk
			if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
				break;
		}

		// Calculate new offsets
		point2_t offsets = dialog->calculateOffsets(xoff, yoff, w, h);
		xoff             = offsets.x;
		yoff             = offsets.y;

		// Create new grAb chunk
		uint32_t  csize = wxUINT32_SWAP_ON_LE(8);
		GrabChunk gc    = { { 'g', 'r', 'A', 'b' }, wxINT32_SWAP_ON_LE(xoff), wxINT32_SWAP_ON_LE(yoff) };
		uint32_t  dcrc  = wxUINT32_SWAP_ON_LE(Misc::crc((uint8_t*)&gc, 12));

		// Build new PNG from the original w/ the new grAb chunk
		MemChunk npng;
		uint32_t rest_start = 33;

		// Init new png data size
		if (grab_start == 0)
			npng.reSize(entry->size() + 20);
		else
			npng.reSize(entry->size());

		// Write PNG header and IHDR chunk
		npng.write(data, 33);

		// If no existing grAb chunk was found, write new one here
		if (grab_start == 0)
		{
			npng.write(&csize, 4);
			npng.write(&gc, 12);
			npng.write(&dcrc, 4);
		}
		else
		{
			// Otherwise write any other data before the existing grAb chunk
			uint32_t to_write = grab_start - 33;
			npng.write(data + 33, to_write);
			rest_start = grab_start + 20;

			// And now write the new grAb chunk
			npng.write(&csize, 4);
			npng.write(&gc, 12);
			npng.write(&dcrc, 4);
		}

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - rest_start;
		npng.write(data + rest_start, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);

		// Set its type back to png
		entry->setType(type);
	}
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Changes the offsets of the given gfx entry.
// Returns false if the entry is invalid or not an offset-supported format, true
// otherwise
// -----------------------------------------------------------------------------
bool EntryOperations::setGfxOffsets(ArchiveEntry* entry, int x, int y)
{
	if (entry == nullptr || entry->type() == nullptr)
		return false;

	// Check entry type
	EntryType* type        = entry->type();
	string     entryformat = type->formatId();
	if (!(entryformat == "img_doom" || entryformat == "img_doom_arah" || entryformat == "img_doom_alpha"
		  || entryformat == "img_doom_beta" || entryformat == "img_png"))
	{
		Log::error(fmt::sprintf(
			"Entry \"%s\" is of type \"%s\" which does not support offsets", entry->name(), entry->type()->name()));
		return false;
	}

	// Doom gfx format, normal and beta version.
	// Also arah format from alpha 0.2 because it uses the same header format.
	if (entryformat == "img_doom" || entryformat == "img_doom_beta" || entryformat == "image_doom_arah")
	{
		// Get patch header
		patch_header_t header;
		entry->seek(0, SEEK_SET);
		entry->read(&header, 8);

		// Apply new offsets
		header.left = wxINT16_SWAP_ON_BE((int16_t)x);
		header.top  = wxINT16_SWAP_ON_BE((int16_t)y);

		// Write new header to entry
		entry->seek(0, SEEK_SET);
		entry->write(&header, 8);
	}

	// Doom alpha gfx format
	else if (entryformat == "img_doom_alpha")
	{
		// Get patch header
		entry->seek(0, SEEK_SET);
		oldpatch_header_t header;
		entry->read(&header, 4);

		// Apply new offsets
		header.left = (int8_t)x;
		header.top  = (int8_t)y;

		// Write new header to entry
		entry->seek(0, SEEK_SET);
		entry->write(&header, 4);
	}

	// PNG format
	else if (entryformat == "img_png")
	{
		// Find existing grAb chunk
		const uint8_t* data       = entry->dataRaw(true);
		uint32_t       grab_start = 0;
		for (uint32_t a = 0; a < entry->size(); a++)
		{
			// Check for 'grAb' header
			if (data[a] == 'g' && data[a + 1] == 'r' && data[a + 2] == 'A' && data[a + 3] == 'b')
			{
				grab_start = a - 4;
				break;
			}

			// Stop when we get to the 'IDAT' chunk
			if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
				break;
		}

		// Create new grAb chunk
		uint32_t  csize = wxUINT32_SWAP_ON_LE(8);
		GrabChunk gc    = { { 'g', 'r', 'A', 'b' }, wxINT32_SWAP_ON_LE(x), wxINT32_SWAP_ON_LE(y) };
		uint32_t  dcrc  = wxUINT32_SWAP_ON_LE(Misc::crc((uint8_t*)&gc, 12));

		// Build new PNG from the original w/ the new grAb chunk
		MemChunk npng;
		uint32_t rest_start = 33;

		// Init new png data size
		if (grab_start == 0)
			npng.reSize(entry->size() + 20);
		else
			npng.reSize(entry->size());

		// Write PNG header and IHDR chunk
		npng.write(data, 33);

		// If no existing grAb chunk was found, write new one here
		if (grab_start == 0)
		{
			npng.write(&csize, 4);
			npng.write(&gc, 12);
			npng.write(&dcrc, 4);
		}
		else
		{
			// Otherwise write any other data before the existing grAb chunk
			uint32_t to_write = grab_start - 33;
			npng.write(data + 33, to_write);
			rest_start = grab_start + 20;

			// And now write the new grAb chunk
			npng.write(&csize, 4);
			npng.write(&gc, 12);
			npng.write(&dcrc, 4);
		}

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - rest_start;
		npng.write(data + rest_start, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);

		// Set its type back to png
		entry->setType(type);
	}
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Opens the map at [entry] with Doom Builder 2, including all open resource
// archives. Sets up a FileMonitor to update the map in the archive if any
// changes are made to it in DB2
// -----------------------------------------------------------------------------
bool EntryOperations::openMapDB2(ArchiveEntry* entry)
{
#ifdef __WXMSW__ // Windows only
	string path = path_db2;

	if (path.empty())
	{
		// Check for DB2 location registry key
		wxRegKey key(wxRegKey::HKLM, "SOFTWARE\\CodeImp\\Doom Builder");
		wxString reg_path;
		key.QueryValue("Location", reg_path);

		// Can't proceed if DB2 isn't installed
		if (reg_path.IsEmpty())
		{
			wxMessageBox("Doom Builder 2 must be installed to use this feature.", "Doom Builder 2 Not Found");
			return false;
		}

		// Add default executable name
		path = reg_path + "\\Builder.exe";
	}

	// Get map info for entry
	auto map = entry->parent()->getMapInfo(entry);

	// Check valid map
	if (map.format == MAP_UNKNOWN)
		return false;

	// Export the map to a temp .wad file
	string filename =
		App::path(StrUtil::join(entry->parent()->filename(false), '-', entry->nameNoExt(), ".wad"), App::Dir::Temp);
	std::replace(filename.begin(), filename.end(), '/', '-');
	if (map.archive)
	{
		entry->exportFile(filename);
		entry->lock();
	}
	else
	{
		// Write map entries to temporary wad archive
		if (map.head)
		{
			WadArchive archive;

			// Add map entries to archive
			ArchiveEntry* e = map.head;
			while (true)
			{
				archive.addEntry(e, "", true);
				e->lock();
				if (e == map.end)
					break;
				e = e->nextEntry();
			}

			// Write archive to file
			archive.save(filename);
		}
	}

	// Generate Doom Builder command line
	string cmd = fmt::sprintf("%s \"%s\" -map %s", path, filename, entry->name());

	// Add base resource archive to command line
	Archive* base = App::archiveManager().baseResourceArchive();
	if (base)
	{
		if (base->formatId() == "wad")
			cmd += fmt::sprintf(" -resource wad \"%s\"", base->filename());
		else if (base->formatId() == "zip")
			cmd += fmt::sprintf(" -resource pk3 \"%s\"", base->filename());
	}

	// Add resource archives to command line
	for (int a = 0; a < App::archiveManager().numArchives(); ++a)
	{
		Archive* archive = App::archiveManager().getArchive(a);

		// Check archive type (only wad and zip supported by db2)
		if (archive->formatId() == "wad")
			cmd += fmt::sprintf(" -resource wad \"%s\"", archive->filename());
		else if (archive->formatId() == "zip")
			cmd += fmt::sprintf(" -resource pk3 \"%s\"", archive->filename());
	}

	// Run DB2
	FileMonitor* fm = new DB2MapFileMonitor(filename, entry->parent(), entry->nameNoExt());
	wxExecute(cmd, wxEXEC_ASYNC, fm->getProcess());

	return true;
#else
	return false;
#endif //__WXMSW__
}

// -----------------------------------------------------------------------------
// Add or remove the alPh chunk from a PNG entry
// -----------------------------------------------------------------------------
bool EntryOperations::modifyalPhChunk(ArchiveEntry* entry, bool value)
{
	if (!entry || !entry->type())
		return false;

	// Don't bother if the entry is locked.
	if (entry->isLocked())
		return false;

	// Check entry type
	if (!(entry->type()->formatId() == "img_png"))
	{
		Log::error(
			fmt::sprintf("Entry \"%s\" is of type \"%s\" rather than PNG", entry->name(), entry->type()->name()));
		return false;
	}

	// Read width and height from IHDR chunk
	const uint8_t* data = entry->dataRaw(true);
	const IHDR*    ihdr = (IHDR*)(data + 12);

	// Find existing alPh chunk
	uint32_t alph_start = 0;
	for (uint32_t a = 0; a < entry->size(); a++)
	{
		// Check for 'alPh' header
		if (data[a] == 'a' && data[a + 1] == 'l' && data[a + 2] == 'P' && data[a + 3] == 'h')
		{
			alph_start = a - 4;
			break;
		}

		// Stop when we get to the 'IDAT' chunk
		if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
			break;
	}

	// We want to set alPh, and it is already there: nothing to do.
	if (value && alph_start > 0)
		return false;

	// We want to unset alPh, and it is already not there: nothing to do either.
	else if (!value && alph_start == 0)
		return false;

	// We want to set alPh, which is missing: create it.
	else if (value && alph_start == 0)
	{
		// Build new PNG from the original w/ the new alPh chunk
		MemChunk npng;

		// Init new png data size
		npng.reSize(entry->size() + 12);

		// Write PNG header and IHDR chunk
		npng.write(data, 33);

		// Create new alPh chunk
		uint32_t  csize = wxUINT32_SWAP_ON_LE(0);
		AlphChunk gc    = { 'a', 'l', 'P', 'h' };
		uint32_t  dcrc  = wxUINT32_SWAP_ON_LE(Misc::crc((uint8_t*)&gc, 4));

		// Create alPh chunk
		npng.write(&csize, 4);
		npng.write(&gc, 4);
		npng.write(&dcrc, 4);

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - 33;
		npng.write(data + 33, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);
	}

	// We want to unset alPh, which is present: delete it.
	else if (!value && alph_start > 0)
	{
		// Build new PNG from the original without the alPh chunk
		MemChunk npng;
		uint32_t rest_start = alph_start + 12;

		// Init new png data size
		npng.reSize(entry->size() - 12);

		// Write PNG info before alPh chunk
		npng.write(data, alph_start);

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - rest_start;
		npng.write(data + rest_start, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);
	}

	// We don't know what we want, but it can't be good, so we do nothing.
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Add or remove the tRNS chunk from a PNG entry
// Returns true if the entry was altered
// -----------------------------------------------------------------------------
bool EntryOperations::modifytRNSChunk(ArchiveEntry* entry, bool value)
{
	// Avoid NULL pointers, they're annoying.
	if (!entry || !entry->type())
		return false;

	// Don't bother if the entry is locked.
	if (entry->isLocked())
		return false;

	// Check entry type
	if (!(entry->type()->formatId() == "img_png"))
	{
		Log::error(fmt::sprintf("Entry \"%s\" is of type \"%s\" rather than PNG", entry->name(), entry->typeString()));
		return false;
	}

	// Read width and height from IHDR chunk
	const uint8_t* data = entry->dataRaw(true);
	const IHDR*    ihdr = (IHDR*)(data + 12);

	// tRNS chunks are only valid for paletted PNGs, and must be before the first IDAT.
	// Specs say they must be after PLTE chunk as well, so to play it safe, we'll insert
	// them just before the first IDAT.
	uint32_t trns_start = 0;
	uint32_t trns_size  = 0;
	uint32_t idat_start = 0;
	for (uint32_t a = 0; a < entry->size(); a++)
	{
		// Check for 'tRNS' header
		if (data[a] == 't' && data[a + 1] == 'R' && data[a + 2] == 'N' && data[a + 3] == 'S')
		{
			trns_start       = a - 4;
			TransChunk* trns = (TransChunk*)(data + a);
			trns_size        = 12 + READ_B32(data, a - 4);
		}

		// Stop when we get to the 'IDAT' chunk
		if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
		{
			idat_start = a - 4;
			break;
		}
	}

	// The IDAT chunk starts before the header is finished, this doesn't make sense, abort.
	if (idat_start < 33)
		return false;

	// We want to set tRNS, and it is already there: nothing to do.
	if (value && trns_start > 0)
		return false;

	// We want to unset tRNS, and it is already not there: nothing to do either.
	else if (!value && trns_start == 0)
		return false;

	// We want to set tRNS, which is missing: create it. We're just going to set index 0 to 0,
	// and leave the rest of the palette indices alone.
	else if (value && trns_start == 0)
	{
		// Build new PNG from the original w/ the new tRNS chunk
		MemChunk npng;

		// Init new png data size
		npng.reSize(entry->size() + 13);

		// Write PNG header stuff up to the first IDAT chunk
		npng.write(data, idat_start);

		// Create new tRNS chunk
		uint32_t   csize = wxUINT32_SWAP_ON_LE(1);
		TransChunk gc    = { 't', 'R', 'N', 'S', '\0' };
		uint32_t   dcrc  = wxUINT32_SWAP_ON_LE(Misc::crc((uint8_t*)&gc, 5));

		// Write tRNS chunk
		npng.write(&csize, 4);
		npng.write(&gc, 5);
		npng.write(&dcrc, 4);

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - idat_start;
		npng.write(data + idat_start, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);
	}

	// We want to unset tRNS, which is present: delete it.
	else if (!value && trns_start > 0)
	{
		// Build new PNG from the original without the tRNS chunk
		MemChunk npng;
		uint32_t rest_start = trns_start + trns_size;

		// Init new png data size
		npng.reSize(entry->size() - trns_size);

		// Write PNG header and stuff up to tRNS start
		npng.write(data, trns_start);

		// Write the rest of the PNG data
		uint32_t to_write = entry->size() - rest_start;
		npng.write(data + rest_start, to_write);

		// Load new png data to the entry
		entry->importMemChunk(npng);
	}

	// We don't know what we want, but it can't be good, so we do nothing.
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Tell whether a PNG entry has an alPh chunk or not
// -----------------------------------------------------------------------------
bool EntryOperations::getalPhChunk(ArchiveEntry* entry)
{
	if (!entry || !entry->type())
		return false;

	// Check entry type
	if (entry->type()->formatId() != "img_png")
	{
		Log::error(fmt::sprintf("Entry \"%s\" is of type \"%s\" rather than PNG", entry->name(), entry->typeString()));
		return false;
	}

	// Find existing alPh chunk
	const uint8_t* data = entry->dataRaw(true);
	for (uint32_t a = 0; a < entry->size(); a++)
	{
		// Check for 'alPh' header
		if (data[a] == 'a' && data[a + 1] == 'l' && data[a + 2] == 'P' && data[a + 3] == 'h')
		{
			return true;
		}

		// Stop when we get to the 'IDAT' chunk
		if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
			break;
	}
	return false;
}

// -----------------------------------------------------------------------------
// Add or remove the tRNS chunk from a PNG entry
// -----------------------------------------------------------------------------
bool EntryOperations::gettRNSChunk(ArchiveEntry* entry)
{
	if (!entry || !entry->type())
		return false;

	// Check entry type
	if (entry->type()->formatId() != "img_png")
	{
		Log::error(fmt::sprintf("Entry \"%s\" is of type \"%s\" rather than PNG", entry->name(), entry->typeString()));
		return false;
	}

	// tRNS chunks are only valid for paletted PNGs, and the chunk must before the first IDAT.
	// Specs say it should be after a PLTE chunk, but that's not always the case (e.g., sgrna7a3.png).
	const uint8_t* data = entry->dataRaw(true);
	for (uint32_t a = 0; a < entry->size(); a++)
	{
		// Check for 'tRNS' header
		if (data[a] == 't' && data[a + 1] == 'R' && data[a + 2] == 'N' && data[a + 3] == 'S')
		{
			return true;
		}

		// Stop when we get to the 'IDAT' chunk
		if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
			break;
	}
	return false;
}

// -----------------------------------------------------------------------------
// Tell whether a PNG entry has a grAb chunk or not and loads the offset values
// in the given references
// -----------------------------------------------------------------------------
bool EntryOperations::readgrAbChunk(ArchiveEntry* entry, point2_t& offsets)
{
	if (!entry || !entry->type())
		return false;

	// Check entry type
	if (entry->type()->formatId() != "img_png")
	{
		Log::error(fmt::sprintf("Entry \"%s\" is of type \"%s\" rather than PNG", entry->name(), entry->typeString()));
		return false;
	}

	// Find existing grAb chunk
	const uint8_t* data = entry->dataRaw(true);
	for (uint32_t a = 0; a < entry->size(); a++)
	{
		// Check for 'grAb' header
		if (data[a] == 'g' && data[a + 1] == 'r' && data[a + 2] == 'A' && data[a + 3] == 'b')
		{
			offsets.x = READ_B32(data, a + 4);
			offsets.y = READ_B32(data, a + 8);
			return true;
		}

		// Stop when we get to the 'IDAT' chunk
		if (data[a] == 'I' && data[a + 1] == 'D' && data[a + 2] == 'A' && data[a + 3] == 'T')
			break;
	}
	return false;
}

// -----------------------------------------------------------------------------
// Adds all [entries] to their parent archive's patch table, if it exists.
// If not, the user is prompted to create or import texturex entries
// -----------------------------------------------------------------------------
bool EntryOperations::addToPatchTable(vector<ArchiveEntry*> entries)
{
	// Check any entries were given
	if (entries.empty())
		return true;

	// Get parent archive
	Archive* parent = entries[0]->parent();
	if (parent == nullptr)
		return true;

	// Find patch table in parent archive
	Archive::SearchOptions opt;
	opt.match_type       = EntryType::fromId("pnames");
	ArchiveEntry* pnames = parent->findLast(opt);

	// Check it exists
	if (!pnames)
	{
		// Create texture entries
		if (!TextureXEditor::setupTextureEntries(parent))
			return false;

		pnames = parent->findLast(opt);

		// If the archive already has ZDoom TEXTURES, it might still
		// not have a PNAMES lump; so create an empty one.
		if (!pnames)
		{
			pnames        = new ArchiveEntry("PNAMES.lmp", 4);
			uint32_t nada = 0;
			pnames->write(&nada, 4);
			pnames->seek(0, SEEK_SET);
			parent->addEntry(pnames);
		}
	}

	// Check it isn't locked (texturex editor open or iwad)
	if (pnames->isLocked())
	{
		if (parent->isReadOnly())
			wxMessageBox("Cannot perform this action on an IWAD", "Error", wxICON_ERROR);
		else
			wxMessageBox(
				"Cannot perform this action because one or more texture related entries is locked. Please close the "
				"archive's texture editor if it is open.",
				"Error",
				wxICON_ERROR);

		return false;
	}

	// Load to patch table
	PatchTable ptable;
	ptable.loadPNAMES(pnames);

	// Add entry names to patch table
	for (auto& entry : entries)
	{
		// Check entry type
		if (!(entry->type()->extraProps().propertyExists("image")))
		{
			Log::warning(fmt::sprintf("Entry %s is not a valid image", entry->name()));
			continue;
		}

		// Check entry name
		if (entry->nameNoExt().size() > 8)
		{
			Log::warning(fmt::sprintf(
				"Entry %s has too long a name to add to the patch table (name must be 8 characters max)",
				entry->name()));
			continue;
		}

		ptable.addPatch(entry->nameNoExt());
	}

	// Write patch table data back to pnames entry
	return ptable.writePNAMES(pnames);
}

// -----------------------------------------------------------------------------
// Same as addToPatchTable, but also creates a single-patch texture from each
// added patch
// -----------------------------------------------------------------------------
bool EntryOperations::createTexture(vector<ArchiveEntry*> entries)
{
	// Check any entries were given
	if (entries.size() == 0)
		return true;

	// Get parent archive
	Archive* parent = entries[0]->parent();

	// Create texture entries if needed
	if (!TextureXEditor::setupTextureEntries(parent))
		return false;

	// Find texturex entry to add to
	Archive::SearchOptions opt;
	opt.match_type         = EntryType::fromId("texturex");
	ArchiveEntry* texturex = parent->findFirst(opt);

	// Check it exists
	bool zdtextures = false;
	if (!texturex)
	{
		opt.match_type = EntryType::fromId("zdtextures");
		texturex       = parent->findFirst(opt);

		if (!texturex)
			return false;
		else
			zdtextures = true;
	}

	// Find patch table in parent archive
	ArchiveEntry* pnames = nullptr;
	if (!zdtextures)
	{
		opt.match_type = EntryType::fromId("pnames");
		pnames         = parent->findLast(opt);

		// Check it exists
		if (!pnames)
			return false;
	}

	// Check entries aren't locked (texture editor open or iwad)
	if ((pnames && pnames->isLocked()) || texturex->isLocked())
	{
		if (parent->isReadOnly())
			wxMessageBox("Cannot perform this action on an IWAD", "Error", wxICON_ERROR);
		else
			wxMessageBox(
				"Cannot perform this action because one or more texture related entries is locked. Please close the "
				"archive's texture editor if it is open.",
				"Error",
				wxICON_ERROR);

		return false;
	}

	TextureXList tx;
	PatchTable   ptable;
	if (zdtextures)
	{
		// Load TEXTURES
		tx.readTEXTURESData(texturex);
	}
	else
	{
		// Load patch table
		ptable.loadPNAMES(pnames);

		// Load TEXTUREx
		tx.readTEXTUREXData(texturex, ptable);
	}

	// Create textures from entries
	SImage image;
	for (auto& entry : entries)
	{
		// Check entry type
		if (!(entry->type()->extraProps().propertyExists("image")))
		{
			Log::warning(fmt::sprintf("Entry %s is not a valid image", entry->name()));
			continue;
		}

		// Check entry name
		auto name = entry->nameNoExt();
		if (name.size() > 8)
		{
			Log::warning(fmt::sprintf(
				"Entry %s has too long a name to add to the patch table (name must be 8 characters max)",
				entry->name()));
			continue;
		}

		// Add to patch table
		if (!zdtextures)
			ptable.addPatch(name);

		// Load patch to temp image
		Misc::loadImageFromEntry(&image, entry);

		// Create texture
		// CTexture* ntex = new CTexture(zdtextures);
		auto ntex = std::make_unique<CTexture>(zdtextures);
		ntex->setName(name);
		ntex->addPatch(name, 0, 0);
		ntex->setWidth(image.width());
		ntex->setHeight(image.height());

		// Setup texture scale
		if (tx.getFormat() == TextureXList::Format::Textures)
			ntex->setScale(1, 1);
		else
			ntex->setScale(0, 0);

		// Add to texture list
		tx.addTexture(std::move(ntex));
	}

	if (zdtextures)
	{
		// Write texture data back to textures entry
		tx.writeTEXTURESData(texturex);
	}
	else
	{
		// Write patch table data back to pnames entry
		ptable.writePNAMES(pnames);

		// Write texture data back to texturex entry
		tx.writeTEXTUREXData(texturex, ptable);
	}

	return true;
}

// -----------------------------------------------------------------------------
// Converts multiple TEXTURE1/2 entries to a single ZDoom text-based TEXTURES
// entry
// -----------------------------------------------------------------------------
bool EntryOperations::convertTextures(vector<ArchiveEntry*> entries)
{
	// Check any entries were given
	if (entries.empty())
		return false;

	// Get parent archive of entries
	Archive* parent = entries[0]->parent();

	// Can't do anything if entry isn't in an archive
	if (!parent)
		return false;

	// Find patch table in parent archive
	Archive::SearchOptions opt;
	opt.match_type       = EntryType::fromId("pnames");
	ArchiveEntry* pnames = parent->findLast(opt);

	// Check it exists
	if (!pnames)
		return false;

	// Load patch table
	PatchTable ptable;
	ptable.loadPNAMES(pnames);

	// Read all texture entries to a single list
	TextureXList tx;
	for (auto& entry : entries)
		tx.readTEXTUREXData(entry, ptable, true);

	// Convert to extended (TEXTURES) format
	tx.convertToTEXTURES();

	// Create new TEXTURES entry and write to it
	ArchiveEntry* textures = parent->addNewEntry("TEXTURES", parent->entryIndex(entries[0]));
	if (textures)
	{
		bool ok = tx.writeTEXTURESData(textures);
		EntryType::detectEntryType(textures);
		textures->setExtensionByType();
		return ok;
	}
	else
		return false;
}

// -----------------------------------------------------------------------------
// Detect errors in a TEXTUREx entry
// -----------------------------------------------------------------------------
bool EntryOperations::findTextureErrors(vector<ArchiveEntry*> entries)
{
	// Check any entries were given
	if (entries.empty())
		return false;

	// Get parent archive of entries
	Archive* parent = entries[0]->parent();

	// Can't do anything if entry isn't in an archive
	if (!parent)
		return false;

	// Find patch table in parent archive
	Archive::SearchOptions opt;
	opt.match_type       = EntryType::fromId("pnames");
	ArchiveEntry* pnames = parent->findLast(opt);

	// Check it exists
	if (!pnames)
		return false;

	// Load patch table
	PatchTable ptable;
	ptable.loadPNAMES(pnames);

	// Read all texture entries to a single list
	TextureXList tx;
	for (auto& entry : entries)
		tx.readTEXTUREXData(entry, ptable, true);

	// Detect errors
	tx.findErrors();

	return true;
}

// -----------------------------------------------------------------------------
// Attempts to compile [entry] as an ACS script. If the entry is named SCRIPTS,
// the compiled data is imported to the BEHAVIOR entry previous to it, otherwise
// it is imported to a same-name compiled library entry in the acs namespace
// -----------------------------------------------------------------------------
bool EntryOperations::compileACS(ArchiveEntry* entry, bool hexen, ArchiveEntry* target, wxFrame* parent)
{
	// Check entry was given
	if (!entry)
		return false;

	// Check entry has a parent (this is useless otherwise)
	if (!target && !entry->parent())
		return false;

	// Check entry is text
	if (!EntryDataFormat::getFormat("text")->isThisFormat(entry->data()))
	{
		wxMessageBox("Error: Entry does not appear to be text", "Error", wxOK | wxCENTRE | wxICON_ERROR);
		return false;
	}

	// Check if the ACC path is set up
	string accpath = path_acc;
	if (accpath.empty() || !wxFileExists(accpath))
	{
		wxMessageBox(
			"Error: ACC path not defined, please configure in SLADE preferences",
			"Error",
			wxOK | wxCENTRE | wxICON_ERROR);
		PreferencesDialog::openPreferences(parent, "ACS");
		return false;
	}

	// Setup some path strings
	string ename         = entry->nameNoExt().to_string();
	string srcfile       = App::path(ename + ".acs", App::Dir::Temp);
	string ofile         = App::path(ename + ".o", App::Dir::Temp);
	auto   include_paths = StrUtil::split(path_acc_libs, ';');

	// Setup command options
	string opt;
	if (hexen)
		opt += " -h";
	if (!include_paths.empty())
	{
		for (const auto& include_path : include_paths)
			opt += fmt::sprintf(" -i \"%s\"", include_path);
	}

	// Find/export any resource libraries
	Archive::SearchOptions sopt;
	sopt.match_type               = EntryType::fromId("acs");
	sopt.search_subdirs           = true;
	vector<ArchiveEntry*> entries = App::archiveManager().findAllResourceEntries(sopt);
	wxArrayString         lib_paths;
	for (auto& res_entry : entries)
	{
		// Ignore SCRIPTS
		if (StrUtil::equalCI(res_entry->nameNoExt(), "SCRIPTS"))
			continue;

		// Ignore entries from other archives
		if (entry->parent() && (entry->parent()->filename(true) != res_entry->parent()->filename(true)))
			continue;

		string path = App::path(StrUtil::join(res_entry->nameNoExt(), ".acs"), App::Dir::Temp);
		res_entry->exportFile(path);
		lib_paths.Add(path);
		Log::info(2, fmt::sprintf("Exporting ACS library %s", res_entry->name()));
	}

	// Export script to file
	entry->exportFile(srcfile);

	// Execute acc
	string        command = path_acc.value + ' ' + opt + " \"" + srcfile + "\" \"" + ofile + '\"';
	wxArrayString output;
	wxArrayString errout;
	wxTheApp->SetTopWindow(parent);
	wxExecute(command, output, errout, wxEXEC_SYNC);
	wxTheApp->SetTopWindow(MainEditor::windowWx());

	// Log output
	Log::console("ACS compiler output:");
	string output_log;
	if (!output.IsEmpty())
	{
		const char* title1 = "=== Log: ===\n";
		Log::console(title1);
		output_log += title1;
		for (const auto& a : output)
		{
			Log::console(WxUtils::stringToView(a));
			output_log += a;
		}
	}

	if (!errout.IsEmpty())
	{
		const char* title2 = "\n=== Error log: ===\n";
		Log::console(title2);
		output_log += title2;
		for (const auto& a : errout)
		{
			Log::console(WxUtils::stringToView(a));
			output_log += a + "\n";
		}
	}

	// Delete source file
	wxRemoveFile(srcfile);

	// Delete library files
	for (const auto& lib_path : lib_paths)
		wxRemoveFile(lib_path);

	// Check it compiled successfully
	bool success = wxFileExists(ofile);
	if (success)
	{
		// If no target entry was given, find one
		if (!target)
		{
			// Check if the script is a map script (BEHAVIOR)
			if (StrUtil::equalCI(entry->name(), "SCRIPTS"))
			{
				// Get entry before SCRIPTS
				ArchiveEntry* prev = entry->prevEntry();

				// Create a new entry there if it isn't BEHAVIOR
				if (!prev || !(StrUtil::equalCI(prev->name(), "BEHAVIOR")))
					prev = entry->parent()->addNewEntry("BEHAVIOR", entry->parent()->entryIndex(entry));

				// Import compiled script
				prev->importFile(ofile);
			}
			else
			{
				// Otherwise, treat it as a library

				// See if the compiled library already exists as an entry
				Archive::SearchOptions opt;
				opt.match_namespace = "acs";
				S_SET_VIEW(opt.match_name, entry->nameNoExt());
				if (entry->parent()->formatDesc()->names_extensions)
				{
					opt.match_name += ".o";
					opt.ignore_ext = false;
				}
				ArchiveEntry* lib = entry->parent()->findLast(opt);

				// If it doesn't exist, create it
				if (!lib)
					lib = entry->parent()->addEntry(new ArchiveEntry(entry->nameNoExt().to_string() + ".o"), "acs");

				// Import compiled script
				lib->importFile(ofile);
			}
		}
		else
			target->importFile(ofile);

		// Delete compiled script file
		wxRemoveFile(ofile);
	}

	if (!success || acc_always_show_output)
	{
		string errors;
		if (wxFileExists(App::path("acs.err", App::Dir::Temp)))
		{
			// Read acs.err to string
			wxFile file(App::path("acs.err", App::Dir::Temp));
			char*  buf = new char[file.Length()];
			file.Read(buf, file.Length());
			errors = wxString::From8BitData(buf, file.Length());
			delete[] buf;
		}
		else
			errors = output_log;

		if (!errors.empty() || !success)
		{
			ExtMessageDialog dlg(nullptr, success ? "ACC Output" : "Error Compiling");
			dlg.setMessage(
				success ? "The following errors were encountered while compiling, please fix them and recompile:" :
						  "Compiler output shown below: ");
			dlg.setExt(errors);
			dlg.ShowModal();
		}

		return success;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Converts [entry] to a PNG image (if possible) and saves the PNG data to a
// file [filename]. Does not alter the entry data itself
// -----------------------------------------------------------------------------
bool EntryOperations::exportAsPNG(ArchiveEntry* entry, string filename)
{
	// Check entry was given
	if (!entry)
		return false;

	// Create image from entry
	SImage image;
	if (!Misc::loadImageFromEntry(&image, entry))
	{
		Log::error(fmt::sprintf("Error converting %s: %s", entry->name(), Global::error));
		return false;
	}

	// Write png data
	MemChunk  png;
	SIFormat* fmt_png = SIFormat::getFormat("png");
	if (!fmt_png->saveImage(image, png, MainEditor::currentPalette(entry)))
	{
		Log::error(fmt::sprintf("Error converting %s", entry->name()));
		return false;
	}

	// Export file
	return png.exportFile(filename);
}

// -----------------------------------------------------------------------------
// Attempts to optimize [entry] using external PNG optimizers.
// -----------------------------------------------------------------------------
bool EntryOperations::optimizePNG(ArchiveEntry* entry)
{
	// Check entry was given
	if (!entry)
		return false;

	// Check entry has a parent (this is useless otherwise)
	if (!entry->parent())
		return false;

	// Check entry is PNG
	if (!EntryDataFormat::getFormat("img_png")->isThisFormat(entry->data()))
	{
		wxMessageBox("Error: Entry does not appear to be PNG", "Error", wxOK | wxCENTRE | wxICON_ERROR);
		return false;
	}

	// Check if the PNG tools path are set up, at least one of them should be
	string pngpathc = path_pngcrush;
	string pngpatho = path_pngout;
	string pngpathd = path_deflopt;
	if ((pngpathc.empty() || !wxFileExists(pngpathc)) && (pngpatho.empty() || !wxFileExists(pngpatho))
		&& (pngpathd.empty() || !wxFileExists(pngpathd)))
	{
		Log::warning(1, "PNG tool paths not defined or invalid, no optimization done.");
		return false;
	}

	// Save special chunks
	point2_t      offsets;
	bool          alphchunk = getalPhChunk(entry);
	bool          grabchunk = readgrAbChunk(entry, offsets);
	string        errormessages;
	wxArrayString output;
	wxArrayString errors;
	size_t        oldsize   = entry->size();
	size_t        crushsize = 0, outsize = 0, deflsize = 0;
	bool          crushed = false, outed = false;

	// Run PNGCrush
	if (!pngpathc.empty() && wxFileExists(pngpathc))
	{
		StrUtil::Path fn(pngpathc);
		fn.setExtension("opt");
		string pngfile = fn.fullPath();
		fn.setExtension("png");
		string optfile = fn.fullPath();
		entry->exportFile(pngfile);

		string command = path_pngcrush.value + " -brute \"" + pngfile + "\" \"" + optfile + "\"";
		output.Empty();
		errors.Empty();
		wxExecute(command, output, errors, wxEXEC_SYNC);

		if (wxFileExists(optfile))
		{
			if (optfile.size() < oldsize)
			{
				entry->importFile(optfile);
				wxRemoveFile(optfile);
				wxRemoveFile(pngfile);
			}
			else
				errormessages += "PNGCrush failed to reduce file size further.\n";
			crushed = true;
		}
		else
			errormessages += "PNGCrush failed to create optimized file.\n";
		crushsize = entry->size();

		// send app output to console if wanted
		if (0)
		{
			string crushlog = "";
			if (errors.GetCount())
			{
				crushlog += "PNGCrush error messages:\n";
				for (size_t i = 0; i < errors.GetCount(); ++i)
					crushlog += errors[i] + "\n";
				errormessages += crushlog;
			}
			if (output.GetCount())
			{
				crushlog += "PNGCrush output messages:\n";
				for (size_t i = 0; i < output.GetCount(); ++i)
					crushlog += output[i] + "\n";
			}
			Log::info(1, crushlog);
		}
	}

	// Run PNGOut
	if (!pngpatho.empty() && wxFileExists(pngpatho))
	{
		StrUtil::Path fn(pngpatho);
		fn.setExtension("opt");
		string pngfile = fn.fullPath();
		fn.setExtension("png");
		string optfile = fn.fullPath();
		entry->exportFile(pngfile);

		string command = path_pngout.value + " /y \"" + pngfile + "\" \"" + optfile + "\"";
		output.Empty();
		errors.Empty();
		wxExecute(command, output, errors, wxEXEC_SYNC);

		if (wxFileExists(optfile))
		{
			if (optfile.size() < oldsize)
			{
				entry->importFile(optfile);
				wxRemoveFile(optfile);
				wxRemoveFile(pngfile);
			}
			else
				errormessages += "PNGout failed to reduce file size further.\n";
			outed = true;
		}
		else if (!crushed)
			// Don't treat it as an error if PNGout couldn't create a smaller file than PNGCrush
			errormessages += "PNGout failed to create optimized file.\n";
		outsize = entry->size();

		// send app output to console if wanted
		if (0)
		{
			string pngoutlog = "";
			if (errors.GetCount())
			{
				pngoutlog += "PNGOut error messages:\n";
				for (size_t i = 0; i < errors.GetCount(); ++i)
					pngoutlog += errors[i] + "\n";
				errormessages += pngoutlog;
			}
			if (output.GetCount())
			{
				pngoutlog += "PNGOut output messages:\n";
				for (size_t i = 0; i < output.GetCount(); ++i)
					pngoutlog += output[i] + "\n";
			}
			Log::info(1, pngoutlog);
		}
	}

	// Run deflopt
	if (!pngpathd.empty() && wxFileExists(pngpathd))
	{
		StrUtil::Path fn(pngpathd);
		fn.setExtension("png");
		string pngfile = fn.fullPath();
		entry->exportFile(pngfile);

		string command = path_deflopt.value + " /sf \"" + pngfile + "\"";
		output.Empty();
		errors.Empty();
		wxExecute(command, output, errors, wxEXEC_SYNC);

		entry->importFile(pngfile);
		wxRemoveFile(pngfile);
		deflsize = entry->size();

		// send app output to console if wanted
		if (0)
		{
			string defloptlog = "";
			if (errors.GetCount())
			{
				defloptlog += "DeflOpt error messages:\n";
				for (size_t i = 0; i < errors.GetCount(); ++i)
					defloptlog += errors[i] + "\n";
				errormessages += defloptlog;
			}
			if (output.GetCount())
			{
				defloptlog += "DeflOpt output messages:\n";
				for (size_t i = 0; i < output.GetCount(); ++i)
					defloptlog += output[i] + "\n";
			}
			Log::info(1, defloptlog);
		}
	}
	output.Clear();
	errors.Clear();

	// Rewrite special chunks
	if (alphchunk)
		modifyalPhChunk(entry, true);
	if (grabchunk)
		setGfxOffsets(entry, offsets.x, offsets.y);

	Log::info(fmt::sprintf(
		"PNG %s size %i =PNGCrush=> %i =PNGout=> %i =DeflOpt=> %i =+grAb/alPh=> %i",
		entry->name(),
		oldsize,
		crushsize,
		outsize,
		deflsize,
		entry->size()));


	if (!crushed && !outed && !errormessages.empty())
	{
		ExtMessageDialog dlg(nullptr, "Optimizing Report");
		dlg.setMessage("The following issues were encountered while optimizing:");
		dlg.setExt(errormessages);
		dlg.ShowModal();

		return false;
	}

	return true;
}


// -----------------------------------------------------------------------------
// Converts ANIMATED data in [entry] to ANIMDEFS format, written to [animdata]
// -----------------------------------------------------------------------------
bool EntryOperations::convertAnimated(ArchiveEntry* entry, MemChunk* animdata, bool animdefs)
{
	const uint8_t*       cursor = entry->dataRaw(true);
	const uint8_t*       eodata = cursor + entry->size();
	const AnimatedEntry* animation;
	string               conversion;
	int                  lasttype = -1;

	while (cursor < eodata && *cursor != ANIM_STOP)
	{
		// reads an entry
		if (cursor + sizeof(AnimatedEntry) > eodata)
		{
			Log::error(1, "ANIMATED entry is corrupt");
			return false;
		}
		animation = (AnimatedEntry*)cursor;
		cursor += sizeof(AnimatedEntry);

		// Create animation string
		if (animdefs)
		{
			conversion = fmt::sprintf(
				"%s\tOptional\t%-8s\tRange\t%-8s\tTics %i%s",
				(animation->type ? "Texture" : "Flat"),
				animation->first,
				animation->last,
				animation->speed,
				(animation->type == ANIM_DECALS ? " AllowDecals\n" : "\n"));
		}
		else
		{
			if ((animation->type > 1 ? 1 : animation->type) != lasttype)
			{
				conversion = fmt::sprintf(
					"#animated %s, spd is number of frames between changes\n"
					"[%s]\n#spd    last        first\n",
					animation->type ? "textures" : "flats",
					animation->type ? "TEXTURES" : "FLATS");
				lasttype = animation->type;
				if (lasttype > 1)
					lasttype = 1;
				animdata->reSize(animdata->size() + conversion.length(), true);
				animdata->write(conversion.data(), conversion.length());
			}
			conversion = fmt::sprintf("%-8d%-12s%-12s\n", animation->speed, animation->last, animation->first);
		}

		// Write string to animdata
		animdata->reSize(animdata->size() + conversion.length(), true);
		animdata->write(conversion.data(), conversion.length());
	}
	return true;
}

// -----------------------------------------------------------------------------
// Converts SWITCHES data in [entry] to ANIMDEFS format, written to [animdata]
// -----------------------------------------------------------------------------
bool EntryOperations::convertSwitches(ArchiveEntry* entry, MemChunk* animdata, bool animdefs)
{
	const uint8_t*       cursor = entry->dataRaw(true);
	const uint8_t*       eodata = cursor + entry->size();
	const SwitchesEntry* switches;
	string               conversion;

	if (!animdefs)
	{
		conversion =
			"#switches usable with each IWAD, 1=SW, 2=registered DOOM, 3=DOOM2\n"
			"[SWITCHES]\n#epi    texture1        texture2\n";
		animdata->reSize(animdata->size() + conversion.length(), true);
		animdata->write(conversion.data(), conversion.length());
	}

	while (cursor < eodata && *cursor != SWCH_STOP)
	{
		// reads an entry
		if (cursor + sizeof(SwitchesEntry) > eodata)
		{
			Log::error(1, "SWITCHES entry is corrupt");
			return false;
		}
		switches = (SwitchesEntry*)cursor;
		cursor += sizeof(SwitchesEntry);

		// Create animation string
		if (animdefs)
		{
			conversion = fmt::sprintf(
				"Switch\tDoom %d\t\t%-8s\tOn Pic\t%-8s\tTics 0\n", switches->type, switches->off, switches->on);
		}
		else
		{
			conversion = fmt::sprintf("%-8d%-12s%-12s\n", switches->type, switches->off, switches->on);
		}

		// Write string to animdata
		animdata->reSize(animdata->size() + conversion.length(), true);
		animdata->write(conversion.data(), conversion.length());
	}
	return true;
}

// -----------------------------------------------------------------------------
// Converts SWANTBLS data in [entry] to binary format, written to [animdata]
// -----------------------------------------------------------------------------
bool EntryOperations::convertSwanTbls(ArchiveEntry* entry, MemChunk* animdata, bool switches)
{
	Tokenizer tz(Tokenizer::Hash);
	tz.openMem(entry->data(), entry->name());

	string token;
	char   buffer[23];
	while ((token = tz.getToken()).length())
	{
		// Animated flats or textures
		if (!switches && (token == "[FLATS]" || token == "[TEXTURES]"))
		{
			bool texture = token == "[TEXTURES]";
			do
			{
				int    speed = tz.getInteger();
				string last  = tz.getToken();
				string first = tz.getToken();
				if (last.length() > 8)
				{
					Log::error(fmt::sprintf(
						"String %s is too long for an animated %s name!", last, (texture ? "texture" : "flat")));
					return false;
				}
				if (first.length() > 8)
				{
					Log::error(fmt::sprintf(
						"String %s is too long for an animated %s name!",
						first,
						(texture ? "texture" : "flat")));
					return false;
				}

				// reset buffer
				int limit;
				memset(buffer, 0, 23);

				// Write animation type
				buffer[0] = texture;

				// Write last texture name
				limit = MIN(8, last.length());
				for (int a = 0; a < limit; ++a)
					buffer[1 + a] = last[a];

				// Write first texture name
				limit = MIN(8, first.length());
				for (int a = 0; a < limit; ++a)
					buffer[10 + a] = first[a];

				// Write animation duration
				buffer[19] = (uint8_t)(speed % 256);
				buffer[20] = (uint8_t)(speed >> 8) % 256;
				buffer[21] = (uint8_t)(speed >> 16) % 256;
				buffer[22] = (uint8_t)(speed >> 24) % 256;

				// Save buffer to MemChunk
				if (!animdata->reSize(animdata->size() + 23, true))
					return false;
				if (!animdata->write(buffer, 23))
					return false;

				// Look for possible end of loop
				token = tz.peekToken();
			} while (token.length() && token[0] != '[');
		}

		// Switches
		else if (switches && token == "[SWITCHES]")
		{
			do
			{
				int    type = tz.getInteger();
				string off  = tz.getToken();
				string on   = tz.getToken();
				if (off.length() > 8)
				{
					Log::error(fmt::sprintf("String %s is too long for a switch name!", off));
					return false;
				}
				if (on.length() > 8)
				{
					Log::error(fmt::sprintf("String %s is too long for a switch name!", on));
					return false;
				}

				// reset buffer
				int limit;
				memset(buffer, 0, 20);

				// Write off texture name
				limit = MIN(8, off.length());
				for (int a = 0; a < limit; ++a)
					buffer[0 + a] = off[a];

				// Write first texture name
				limit = MIN(8, on.length());
				for (int a = 0; a < limit; ++a)
					buffer[9 + a] = on[a];

				// Write switch type
				buffer[18] = (uint8_t)(type % 256);
				buffer[19] = (uint8_t)(type >> 8) % 256;

				// Save buffer to MemChunk
				if (!animdata->reSize(animdata->size() + 20, true))
					return false;
				if (!animdata->write(buffer, 20))
					return false;

				// Look for possible end of loop
				token = tz.peekToken();
			} while (token.length() && token[0] != '[');
		}
	}
	return true;
	// Note that we do not terminate the list here!
}


void fixpngsrc(ArchiveEntry* entry)
{
	if (!entry)
		return;
	const uint8_t* source = entry->dataRaw();
	uint8_t*       data   = new uint8_t[entry->size()];
	memcpy(data, source, entry->size());

	// Last check that it's a PNG
	uint32_t header1 = READ_B32(data, 0);
	uint32_t header2 = READ_B32(data, 4);
	if (header1 != 0x89504E47 || header2 != 0x0D0A1A0A)
		return;

	// Loop through each chunk and recompute CRC
	uint32_t pointer      = 8;
	bool     neededchange = false;
	while (pointer < entry->size())
	{
		if (pointer + 12 > entry->size())
		{
			Log::info(fmt::sprintf("Entry %s cannot be repaired.", entry->name()));
			delete[] data;
			return;
		}
		uint32_t chsz = READ_B32(data, pointer);
		if (pointer + 12 + chsz > entry->size())
		{
			Log::info(fmt::sprintf("Entry %s cannot be repaired.", entry->name()));
			delete[] data;
			return;
		}
		uint32_t crc = Misc::crc(data + pointer + 4, 4 + chsz);
		if (crc != READ_B32(data, pointer + 8 + chsz))
		{
			Log::info(fmt::sprintf(
				"Chunk %c%c%c%c has bad CRC",
				data[pointer + 4],
				data[pointer + 5],
				data[pointer + 6],
				data[pointer + 7]));
			neededchange              = true;
			data[pointer + 8 + chsz]  = crc >> 24;
			data[pointer + 9 + chsz]  = (crc & 0x00ffffff) >> 16;
			data[pointer + 10 + chsz] = (crc & 0x0000ffff) >> 8;
			data[pointer + 11 + chsz] = (crc & 0x000000ff);
		}
		pointer += (chsz + 12);
	}
	// Import new data with fixed CRC
	if (neededchange)
	{
		entry->importMem(data, entry->size());
	}
	delete[] data;
	return;
}


// -----------------------------------------------------------------------------
//
// Console Commands
//
// -----------------------------------------------------------------------------

CONSOLE_COMMAND(fixpngcrc, 0, true)
{
	vector<ArchiveEntry*> selection = MainEditor::currentEntrySelection();
	if (selection.empty())
	{
		Log::info(1, "No entry selected");
		return;
	}
	for (auto& entry : selection)
	{
		if (entry->type()->formatId() == "img_png")
			fixpngsrc(entry);
	}
}
