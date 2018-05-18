#pragma once

#include "Archive/Archive.h"

class WolfArchive : public TreelessArchive
{
public:
	WolfArchive() : TreelessArchive("wolf") {}
	~WolfArchive() = default;

	// Wolf3D specific
	uint32_t getEntryOffset(ArchiveEntry* entry);
	void     setEntryOffset(ArchiveEntry* entry, uint32_t offset);

	// Opening
	bool open(string_view filename) override; // Open from File
	bool open(MemChunk& mc) override;         // Open from MemChunk

	bool openAudio(MemChunk& head, MemChunk& data);
	bool openGraph(MemChunk& head, MemChunk& data, MemChunk& dict);
	bool openMaps(MemChunk& head, MemChunk& data);

	// Writing/Saving
	bool write(MemChunk& mc, bool update = true) override; // Write to MemChunk

	// Misc
	bool     loadEntryData(ArchiveEntry* entry) override;
	unsigned numEntries() override { return rootDir()->numEntries(); }

	// Entry addition/removal
	ArchiveEntry* addEntry(
		ArchiveEntry*    entry,
		unsigned         position = 0xFFFFFFFF,
		ArchiveTreeNode* dir      = nullptr,
		bool             copy     = false) override;
	ArchiveEntry* addEntry(ArchiveEntry* entry, string_view add_namespace, bool copy = false) override;

	// Entry modification
	bool renameEntry(ArchiveEntry* entry, string_view name) override;

	static bool isWolfArchive(MemChunk& mc);
	static bool isWolfArchive(string_view filename);

private:
	struct WolfHandle
	{
		uint32_t offset;
		uint16_t size;
	};

	uint16_t spritestart_ = 0;
	uint16_t soundstart_  = 0;
};
