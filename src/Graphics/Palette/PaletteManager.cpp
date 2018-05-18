
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         https://slade.mancubus.net
// Filename:    PaletteManager.cpp
// Description: PaletteManager class. Manages all resource/custom palettes for
//              viewing doom gfx / flats and conversions
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
#include "PaletteManager.h"
#include "App.h"
#include "Archive/ArchiveManager.h"
#include "Archive/Formats/ZipArchive.h"
#include "General/Misc.h"


// -----------------------------------------------------------------------------
//
// PaletteManager Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Initialises the palette manager
// -----------------------------------------------------------------------------
bool PaletteManager::init()
{
	// Load palettes from SLADE.pk3
	if (!loadResourcePalettes())
		return false;

	// Load custom palettes (from <user directory>/palettes)
	loadCustomPalettes();

	return true;
}

// -----------------------------------------------------------------------------
// Adds the palette [pal] to the list of managed palettes, identified by [name].
// Returns false if the palette doesn't exist or the name is invalid, true
// otherwise
// -----------------------------------------------------------------------------
bool PaletteManager::addPalette(Palette::UPtr pal, string_view name)
{
	// Check palette and name were given
	if (!pal || name.empty())
		return false;

	// Add palette+name
	palettes_.push_back(std::move(pal));
	pal_names_.emplace_back(name.data(), name.size());

	return true;
}

// -----------------------------------------------------------------------------
// Returns the 'global' palette. This is the palette within the current base
// resource archive. If no base resource archive is loaded, the default
// greyscale palette is used
// -----------------------------------------------------------------------------
Palette* PaletteManager::globalPalette()
{
	// Check if a base resource archive is open
	if (!App::archiveManager().baseResourceArchive())
		return &pal_default_;

	// Return the palette contained in the base resource archive
	Misc::loadPaletteFromArchive(&pal_global_, App::archiveManager().baseResourceArchive());
	return &pal_global_;
}

// -----------------------------------------------------------------------------
// Returns the palette at [index], or the default palette (greyscale) if index
// is out of bounds
// -----------------------------------------------------------------------------
Palette* PaletteManager::palette(int index)
{
	if (index < 0 || index >= numPalettes())
		return &pal_default_;
	else
		return palettes_[index].get();
}

// -----------------------------------------------------------------------------
// Returns the palette matching the given name, or the default palette
// (greyscale) if no matching palette found
// -----------------------------------------------------------------------------
Palette* PaletteManager::palette(string_view name)
{
	for (uint32_t a = 0; a < pal_names_.size(); a++)
	{
		if (pal_names_[a] == name)
			return palettes_[a].get();
	}

	return &pal_default_;
}

// -----------------------------------------------------------------------------
// Returns the name of the palette at [index], or an empty string if index is
// out of bounds
// -----------------------------------------------------------------------------
string PaletteManager::paletteName(int index)
{
	if (index < 0 || index >= numPalettes())
		return "";
	else
		return pal_names_[index];
}

// -----------------------------------------------------------------------------
// Returns the name of the given palette, or an empty string if the palette
// isn't managed by the PaletteManager
// -----------------------------------------------------------------------------
string PaletteManager::paletteName(Palette* pal)
{
	for (uint32_t a = 0; a < palettes_.size(); a++)
	{
		if (palettes_[a].get() == pal)
			return pal_names_[a];
	}

	return "";
}

// -----------------------------------------------------------------------------
// Loads any entries in the 'palettes' directory of SLADE.pk3 as palettes, with
// names from the entries (minus the entry extension)
// -----------------------------------------------------------------------------
bool PaletteManager::loadResourcePalettes()
{
	// Get the 'palettes' directory of SLADE.pk3
	auto res_archive  = App::archiveManager().programResourceArchive();
	auto dir_palettes = res_archive->getDir("palettes/");

	// Check it exists
	if (!dir_palettes)
		return false;

	// Go through all entries in the directory
	for (size_t a = 0; a < dir_palettes->numEntries(); a++)
	{
		// Load palette data
		auto     pal = std::make_unique<Palette>();
		MemChunk mc(dir_palettes->entryAt(a)->dataRaw(true), dir_palettes->entryAt(a)->size());
		pal->loadMem(mc);

		// Add the palette
		addPalette(std::move(pal), dir_palettes->entryAt(a)->nameNoExt());
	}

	return true;
}

// -----------------------------------------------------------------------------
// Loads any files in the '<userdir>/palettes' directory as palettes, with names
// from the files (minus the file extension)
// -----------------------------------------------------------------------------
bool PaletteManager::loadCustomPalettes()
{
	// If the directory doesn't exist create it
	if (!wxDirExists(App::path("palettes", App::Dir::User)))
		wxMkdir(App::path("palettes", App::Dir::User));

	// Open the custom palettes directory
	wxDir res_dir;
	res_dir.Open(App::path("palettes", App::Dir::User));

	// Go through each file in the directory
	wxString filename = wxEmptyString;
	bool     files    = res_dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
	while (files)
	{
		auto pal       = std::make_unique<Palette>();
		auto full_path = res_dir.GetName() + '/' + filename;

		// Load palette data
		MemChunk mc;
		mc.importFile(full_path.ToStdString());
		pal->loadMem(mc);

		// Add the palette
		wxFileName fn(filename);
		addPalette(std::move(pal), fn.GetName().ToStdString());

		// Next file
		files = res_dir.GetNext(&filename);
	}

	return true;
}
