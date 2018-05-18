#pragma once

#include "Archive/Archive.h"
#include "General/ListenerAnnouncer.h"
#include "Graphics/CTexture/CTexture.h"
#include "common.h"

class ResourceManager;

// This base class is probably not really needed
class Resource
{
	friend class ResourceManager;

public:
	Resource(string_view type) : type_{ type.data(), type.size() } {}
	virtual ~Resource() = default;

	virtual int length() { return 0; }

private:
	string type_;
};

class EntryResource : public Resource
{
	friend class ResourceManager;

public:
	EntryResource() : Resource("entry") {}
	virtual ~EntryResource() = default;

	void add(ArchiveEntry::SPtr& entry);
	void remove(ArchiveEntry::SPtr& entry);

	int length() override { return entries_.size(); }

	ArchiveEntry* getEntry(Archive* priority = nullptr, string_view nspace = "", bool ns_required = false);

private:
	vector<std::weak_ptr<ArchiveEntry>> entries_;
};

class TextureResource : public Resource
{
	friend class ResourceManager;

public:
	struct Texture
	{
		CTexture tex;
		Archive* parent;

		Texture(CTexture* tex_copy, Archive* parent) : parent(parent)
		{
			if (tex_copy)
				tex.copyTexture(tex_copy);
		}
	};

	TextureResource() : Resource("texture") {}
	virtual ~TextureResource() = default;

	void add(CTexture* tex, Archive* parent);
	void remove(Archive* parent);

	int length() override { return textures_.size(); }

private:
	vector<std::unique_ptr<Texture>> textures_;
};

typedef std::map<string, EntryResource>   EntryResourceMap;
typedef std::map<string, TextureResource> TextureResourceMap;

class ResourceManager : public Listener, public Announcer
{
public:
	ResourceManager()  = default;
	~ResourceManager() = default;

	static ResourceManager* getInstance()
	{
		if (!instance_)
			instance_ = new ResourceManager();

		return instance_;
	}

	void addArchive(Archive* archive);
	void removeArchive(Archive* archive);

	void addEntry(ArchiveEntry::SPtr& entry);
	void removeEntry(ArchiveEntry::SPtr& entry);

	void listAllPatches();
	void getAllPatchEntries(vector<ArchiveEntry*>& list, Archive* priority);

	void getAllTextures(vector<TextureResource::Texture*>& list, Archive* priority, Archive* ignore = nullptr);
	void getAllTextureNames(vector<string>& list);

	void getAllFlatEntries(vector<ArchiveEntry*>& list, Archive* priority);
	void getAllFlatNames(vector<string>& list);

	ArchiveEntry*   getPaletteEntry(string_view palette, Archive* priority = nullptr);
	ArchiveEntry*   getPatchEntry(string_view patch, string_view nspace = "patches", Archive* priority = nullptr);
	ArchiveEntry*   getFlatEntry(string_view flat, Archive* priority = nullptr);
	ArchiveEntry*   getTextureEntry(string_view texture, string_view nspace = "textures", Archive* priority = nullptr);
	CTexture*       getTexture(string_view texture, Archive* priority = nullptr, Archive* ignore = nullptr);
	static uint16_t getTextureHash(string_view name);

	void onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data) override;

	static string getTextureName(uint16_t hash) { return doom64_hash_table_[hash]; }

private:
	EntryResourceMap   palettes_;
	EntryResourceMap   patches_;
	EntryResourceMap   graphics_;
	EntryResourceMap   flats_;
	EntryResourceMap   satextures_; // Stand Alone textures (e.g., between TX_ or T_ markers)
	TextureResourceMap textures_;   // Composite textures (defined in a TEXTUREx/TEXTURES lump)

	static ResourceManager* instance_;
	static string           doom64_hash_table_[65536];
};

// Define for less cumbersome ResourceManager::getInstance()
#define theResourceManager ResourceManager::getInstance()
