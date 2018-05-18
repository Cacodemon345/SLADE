
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    ResourceManager.cpp
// Description: The ResourceManager class. Manages all editing resources
//              (patches, gfx, etc) in all open archives and the base resource
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
#include "ResourceManager.h"
#include "Archive/ArchiveManager.h"
#include "General/Console/Console.h"
#include "Graphics/CTexture/CTexture.h"
#include "Graphics/CTexture/PatchTable.h"
#include "Graphics/CTexture/TextureXList.h"
#include "Utility/StringUtils.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
ResourceManager* ResourceManager::instance_ = nullptr;
string           ResourceManager::doom64_hash_table_[65536];


// -----------------------------------------------------------------------------
//
// EntryResource Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Adds matching [entry] to the resource
// -----------------------------------------------------------------------------
void EntryResource::add(ArchiveEntry::SPtr& entry)
{
	if (entry->parent())
		entries_.push_back(entry);
}

// -----------------------------------------------------------------------------
// Removes matching [entry] from the resource
// -----------------------------------------------------------------------------
void EntryResource::remove(ArchiveEntry::SPtr& entry)
{
	unsigned a = 0;
	while (a < entries_.size())
	{
		if (entries_[a].lock() == entry)
			entries_.erase(entries_.begin() + a);
		else
			++a;
	}
}

// -----------------------------------------------------------------------------
// Gets the most relevant entry for this resource, depending on [priority] and
// [nspace]. If [priority] is set, this will prioritize entries from the
// priority archive. If [nspace] is not empty, this will prioritize entries
// within that namespace, or if [ns_required] is true, ignore anything not in
// [nspace]
// -----------------------------------------------------------------------------
ArchiveEntry* EntryResource::getEntry(Archive* priority, string_view nspace, bool ns_required)
{
	// Check resoure has any entries
	if (entries_.empty())
		return nullptr;

	ArchiveEntry::SPtr best;
	auto               i = entries_.begin();
	while (i != entries_.end())
	{
		// Check if expired
		if (i->expired())
		{
			i = entries_.erase(i);
			continue;
		}

		auto entry = i->lock();
		++i;

		if (!best)
			best = entry;

		// Check namespace if required
		if (ns_required && !nspace.empty())
			if (!entry->isInNamespace(nspace))
				continue;

		// Check if in priority archive (or its parent)
		if (priority && (entry->parent() == priority || entry->parent()->parentArchive() == priority))
		{
			best = entry;
			break;
		}

		// Check namespace
		if (!ns_required && !nspace.empty() && !best->isInNamespace(nspace) && entry->isInNamespace(nspace))
		{
			best = entry;
			continue;
		}

		// Otherwise, if it's in a 'later' archive than the current resource entry, set it
		if (App::archiveManager().archiveIndex(best->parent()) <= App::archiveManager().archiveIndex(entry->parent()))
			best = entry;
	}

	return best.get();
}


// -----------------------------------------------------------------------------
//
// TextureResource Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Adds a texture to this resource
// -----------------------------------------------------------------------------
void TextureResource::add(CTexture* tex, Archive* parent)
{
	// Check args
	if (!tex || !parent)
		return;

	textures_.push_back(std::make_unique<Texture>(tex, parent));
}

// -----------------------------------------------------------------------------
// Removes any textures in this resource that are part of [parent] archive
// -----------------------------------------------------------------------------
void TextureResource::remove(Archive* parent)
{
	// Remove any textures with matching parent
	auto i = textures_.begin();
	while (i != textures_.end())
	{
		if (i->get()->parent == parent)
			i = textures_.erase(i);
		else
			++i;
	}
}


// -----------------------------------------------------------------------------
//
// ResourceManager Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Adds an archive to be managed
// -----------------------------------------------------------------------------
void ResourceManager::addArchive(Archive* archive)
{
	// Check archive was given
	if (!archive)
		return;

	// Go through entries
	vector<ArchiveEntry::SPtr> entries;
	archive->getEntryTreeAsList(entries);
	for (auto& entry : entries)
		addEntry(entry);

	// Listen to the archive
	listenTo(archive);

	// Announce resource update
	announce("resources_updated");
}

// -----------------------------------------------------------------------------
// Removes a managed archive
// -----------------------------------------------------------------------------
void ResourceManager::removeArchive(Archive* archive)
{
	// Check archive was given
	if (!archive)
		return;

	// Go through entries
	vector<ArchiveEntry::SPtr> entries;
	archive->getEntryTreeAsList(entries);
	for (auto& entry : entries)
		removeEntry(entry);

	// Announce resource update
	announce("resources_updated");
}

// -----------------------------------------------------------------------------
// Returns the Doom64 hash of a given texture name, computed using the same
// hash algorithm as Doom64 EX itself
// -----------------------------------------------------------------------------
uint16_t ResourceManager::getTextureHash(string_view name)
{
	char str[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	for (size_t c = 0; c < name.length() && c < 8; c++)
		str[c] = name[c];

	uint32_t hash = 1315423911;
	for (uint32_t i = 0; i < 8 && str[i] != '\0'; ++i)
		hash ^= ((hash << 5) + toupper((int)str[i]) + (hash >> 2));
	hash %= 65536;
	return (uint16_t)hash;
}

// -----------------------------------------------------------------------------
// Adds an entry to be managed
// -----------------------------------------------------------------------------
void ResourceManager::addEntry(ArchiveEntry::SPtr& entry)
{
	if (!entry.get())
		return;

	// Detect type if unknown
	if (entry->type() == EntryType::unknownType())
		EntryType::detectEntryType(entry.get());

	// Get entry type
	EntryType* type = entry->type();

	// Get resource name (extension cut, uppercase)
	string name = entry->upperNameNoExt().to_string();
	// Talon1024 - Get resource path (uppercase, without leading slash)
	string path = entry->path(true);
	StrUtil::upperIP(path);
	path.erase(0, 1);

	// Check for palette entry
	if (type->id() == "palette")
		palettes_[name].add(entry);

	// Check for various image entries, so only accept images
	if (type->editor() == "gfx")
	{
		// Reject graphics that are not in a valid namespace:
		// Patches in wads can be in the global namespace as well, and
		// ZDoom textures can use sprites and graphics as patches
		if (!entry->isInNamespace("global") && !entry->isInNamespace("patches") && !entry->isInNamespace("sprites")
			&& !entry->isInNamespace("graphics") &&
			// Stand-alone textures can also be found in the hires namespace
			!entry->isInNamespace("hires") && !entry->isInNamespace("textures") &&
			// Flats are kinda boring in comparison
			!entry->isInNamespace("flats"))
			return;

		// Check for patch entry
		if (type->extraProps().propertyExists("patch") || entry->isInNamespace("patches"))
		{
			patches_[name].add(entry);
			/*
			if (name.Length() > 8) patches[name.Left(8)].add(entry);
			if (!entry->getParent()->isTreeless())
				patches[path].add(entry);
			*/
		}

		// Check for flat entry
		if (type->id() == "gfx_flat" || entry->isInNamespace("flats"))
		{
			flats_[name].add(entry);
			// if (name.Length() > 8) flats[name.Left(8)].add(entry);
			if (!entry->parent()->isTreeless())
				flats_[path].add(entry);
		}

		// Check for stand-alone texture entry
		if (entry->isInNamespace("textures") || entry->isInNamespace("hires"))
		{
			satextures_[name].add(entry);
			// if (name.Length() > 8) satextures[name.Left(8)].add(entry);
			if (!entry->parent()->isTreeless())
				satextures_[path].add(entry);

			// Add name to hash table
			ResourceManager::doom64_hash_table_[getTextureHash(name)] = name;
		}
	}

	// Check for TEXTUREx entry
	int txentry = 0;
	if (type->id() == "texturex")
		txentry = 1;
	else if (type->id() == "zdtextures")
		txentry = 2;
	if (txentry > 0)
	{
		// Load patch table if needed
		PatchTable ptable;
		if (txentry == 1)
		{
			Archive::SearchOptions opt;
			opt.match_type       = EntryType::fromId("pnames");
			ArchiveEntry* pnames = entry->parent()->findLast(opt);
			ptable.loadPNAMES(pnames, entry->parent());
		}

		// Read texture list
		TextureXList tx;
		if (txentry == 1)
			tx.readTEXTUREXData(entry.get(), ptable);
		else
			tx.readTEXTURESData(entry.get());

		// Add all textures to resources
		CTexture* tex;
		for (unsigned a = 0; a < tx.nTextures(); a++)
		{
			tex = tx.getTexture(a);
			textures_[tex->name()].add(tex, entry->parent());
		}
	}
}

// -----------------------------------------------------------------------------
// Removes a managed entry
// -----------------------------------------------------------------------------
void ResourceManager::removeEntry(ArchiveEntry::SPtr& entry)
{
	if (!entry.get())
		return;

	// Get resource name (extension cut, uppercase)
	string name = entry->upperNameNoExt().to_string();
	string path = entry->path(true);
	StrUtil::upperIP(path);
	path.erase(0, 1);

	// Remove from palettes
	palettes_[name].remove(entry);

	// Remove from patches
	patches_[name].remove(entry);

	// Remove from flats
	flats_[name].remove(entry);
	flats_[path].remove(entry);

	// Remove from stand-alone textures
	satextures_[name].remove(entry);
	satextures_[path].remove(entry);

	// Check for TEXTUREx entry
	int txentry = 0;
	if (entry->type()->id() == "texturex")
		txentry = 1;
	else if (entry->type()->id() == "zdtextures")
		txentry = 2;
	if (txentry > 0)
	{
		// Read texture list
		TextureXList tx;
		PatchTable   ptable;
		if (txentry == 1)
			tx.readTEXTUREXData(entry.get(), ptable);
		else
			tx.readTEXTURESData(entry.get());

		// Remove all texture resources
		for (unsigned a = 0; a < tx.nTextures(); a++)
			textures_[tx.getTexture(a)->name()].remove(entry->parent());
	}
}

// -----------------------------------------------------------------------------
// Dumps all patch names and the number of matching entries for each
// -----------------------------------------------------------------------------
void ResourceManager::listAllPatches()
{
	EntryResourceMap::iterator i = patches_.begin();
	while (i != patches_.end())
	{
		LOG_MESSAGE(1, "%s (%d)", i->first, i->second.length());
		++i;
	}
}

// -----------------------------------------------------------------------------
// Adds all current patch entries to [list]
// -----------------------------------------------------------------------------
void ResourceManager::getAllPatchEntries(vector<ArchiveEntry*>& list, Archive* priority)
{
	for (auto& i : patches_)
	{
		auto entry = i.second.getEntry(priority);
		if (entry)
			list.push_back(entry);
	}
}

// -----------------------------------------------------------------------------
// Adds all current textures to [list]
// -----------------------------------------------------------------------------
void ResourceManager::getAllTextures(vector<TextureResource::Texture*>& list, Archive* priority, Archive* ignore)
{
	// Add all primary textures to the list
	for (auto& i : textures_)
	{
		// Skip if no entries
		if (i.second.length() == 0)
			continue;

		// Go through resource textures
		TextureResource::Texture* res = i.second.textures_[0].get();
		for (int a = 0; a < i.second.length(); a++)
		{
			res = i.second.textures_[a].get();

			// Skip if it's in the 'ignore' archive
			if (res->parent == ignore)
				continue;

			// If it's in the 'priority' archive, exit loop
			if (priority && i.second.textures_[a]->parent == priority)
				break;

			// Otherwise, if it's in a 'later' archive than the current resource, set it
			if (App::archiveManager().archiveIndex(res->parent)
				<= App::archiveManager().archiveIndex(i.second.textures_[a]->parent))
				res = i.second.textures_[a].get();
		}

		// Add texture resource to the list
		if (res->parent != ignore)
			list.push_back(res);
	}
}

// -----------------------------------------------------------------------------
// Adds all current texture names to [list]
// -----------------------------------------------------------------------------
void ResourceManager::getAllTextureNames(vector<string>& list)
{
	// Add all primary textures to the list
	for (auto& i : textures_)
		if (i.second.length() > 0) // Ignore if no entries
			list.push_back(i.first);
}

// -----------------------------------------------------------------------------
// Adds all current flat entries to [list]
// -----------------------------------------------------------------------------
void ResourceManager::getAllFlatEntries(vector<ArchiveEntry*>& list, Archive* priority)
{
	for (auto& i : flats_)
	{
		auto entry = i.second.getEntry(priority);
		if (entry)
			list.push_back(entry);
	}
}

// -----------------------------------------------------------------------------
// Adds all current flat names to [list]
// -----------------------------------------------------------------------------
void ResourceManager::getAllFlatNames(vector<string>& list)
{
	// Add all primary flats to the list
	for (auto& i : flats_)
		if (i.second.length() > 0) // Ignore if no entries
			list.push_back(i.first);
}

// -----------------------------------------------------------------------------
// Returns the most appropriate managed resource entry for [palette], or
// nullptr if no match found
// -----------------------------------------------------------------------------
ArchiveEntry* ResourceManager::getPaletteEntry(string_view palette, Archive* priority)
{
	return palettes_[StrUtil::upper(palette)].getEntry(priority);
}

// -----------------------------------------------------------------------------
// Returns the most appropriate managed resource entry for [patch],
// or nullptr if no match found
// -----------------------------------------------------------------------------
ArchiveEntry* ResourceManager::getPatchEntry(string_view patch, string_view nspace, Archive* priority)
{
	// Are we wanting to use a flat as a patch?
	if (StrUtil::equalCI(nspace, "flats"))
		return getFlatEntry(patch, priority);

	// Are we wanting to use a stand-alone texture as a patch?
	if (StrUtil::equalCI(nspace, "textures"))
		return getTextureEntry(patch, "textures", priority);

	return patches_[StrUtil::upper(patch)].getEntry(priority, nspace, true);
}

// -----------------------------------------------------------------------------
// Returns the most appropriate managed resource entry for [flat], or nullptr
// if no match found
// -----------------------------------------------------------------------------
ArchiveEntry* ResourceManager::getFlatEntry(string_view flat, Archive* priority)
{
	// Check resource with matching name exists
	EntryResource& res = flats_[StrUtil::upper(flat)];
	if (res.entries_.empty())
		return nullptr;

	// Return most relevant entry
	return res.getEntry(priority);
}

// -----------------------------------------------------------------------------
// Returns the most appropriate managed resource entry for [texture], or
// nullptr if no match found
// -----------------------------------------------------------------------------
ArchiveEntry* ResourceManager::getTextureEntry(string_view texture, string_view nspace, Archive* priority)
{
	return satextures_[StrUtil::upper(texture)].getEntry(priority, nspace, true);
}

// -----------------------------------------------------------------------------
// Returns the most appropriate managed texture for [texture], or nullptr if no
// match found
// -----------------------------------------------------------------------------
CTexture* ResourceManager::getTexture(string_view texture, Archive* priority, Archive* ignore)
{
	// Check texture resource with matching name exists
	TextureResource& res = textures_[StrUtil::upper(texture)];
	if (res.textures_.empty())
		return nullptr;

	// Go through resource textures
	CTexture* tex    = &res.textures_[0].get()->tex;
	Archive*  parent = res.textures_[0].get()->parent;
	for (auto& res_tex : res.textures_)
	{
		// Skip if it's in the 'ignore' archive
		if (res_tex->parent == ignore)
			continue;

		// If it's in the 'priority' archive, return it
		if (priority && res_tex->parent == priority)
			return &res_tex->tex;

		// Otherwise, if it's in a 'later' archive than the current resource entry, set it
		if (App::archiveManager().archiveIndex(parent) <= App::archiveManager().archiveIndex(res_tex->parent))
		{
			tex    = &res_tex->tex;
			parent = res_tex->parent;
		}
	}

	// Return the most relevant texture
	if (parent != ignore)
		return tex;
	else
		return nullptr;
}

// -----------------------------------------------------------------------------
// Called when an announcement is recieved from any managed archive
// -----------------------------------------------------------------------------
void ResourceManager::onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data)
{
	event_data.seek(0, SEEK_SET);

	// An entry is modified
	if (event_name == "entry_state_changed")
	{
		wxUIntPtr ptr;
		event_data.read(&ptr, sizeof(wxUIntPtr), 4);
		ArchiveEntry* entry = (ArchiveEntry*)wxUIntToPtr(ptr);
		auto          esp   = entry->parent()->entryAtPathShared(entry->path(true));
		removeEntry(esp);
		addEntry(esp);
		announce("resources_updated");
	}

	// An entry is removed or renamed
	if (event_name == "entry_removing" || event_name == "entry_renaming")
	{
		wxUIntPtr ptr;
		event_data.read(&ptr, sizeof(wxUIntPtr), sizeof(int));
		ArchiveEntry* entry = (ArchiveEntry*)wxUIntToPtr(ptr);
		auto          esp   = entry->parent()->entryAtPathShared(entry->path(true));
		removeEntry(esp);
		announce("resources_updated");
	}

	// An entry is added
	if (event_name == "entry_added")
	{
		wxUIntPtr ptr;
		event_data.read(&ptr, sizeof(wxUIntPtr), 4);
		ArchiveEntry* entry = (ArchiveEntry*)wxUIntToPtr(ptr);
		auto          esp   = entry->parent()->entryAtPathShared(entry->path(true));
		addEntry(esp);
		announce("resources_updated");
	}
}





CONSOLE_COMMAND(list_res_patches, 0, false)
{
	theResourceManager->listAllPatches();
}

#include "App.h"
CONSOLE_COMMAND(test_res_speed, 0, false)
{
	vector<ArchiveEntry*> list;

	Log::console("Testing...");

	long times[5];

	for (long& time : times)
	{
		auto start = App::runTimer();
		for (unsigned a = 0; a < 100; a++)
		{
			theResourceManager->getAllPatchEntries(list, nullptr);
			list.clear();
		}
		for (unsigned a = 0; a < 100; a++)
		{
			theResourceManager->getAllFlatEntries(list, nullptr);
			list.clear();
		}
		auto end = App::runTimer();
		time     = end - start;
	}

	float avg = float(times[0] + times[1] + times[2] + times[3] + times[4]) / 5.0f;
	Log::console(S_FMT("Test took %dms avg", (int)avg));
}
