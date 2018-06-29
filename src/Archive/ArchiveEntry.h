#pragma once

#include "EntryType/EntryType.h"
#include "Utility/PropertyList/PropertyList.h"

class ArchiveTreeNode;
class Archive;
struct ArchiveFormat;

enum EncryptionModes
{
	ENC_NONE = 0,
	ENC_JAGUAR,
	ENC_BLOOD,
	ENC_SCRLE0,
	ENC_TXB,
};

class ArchiveEntry
{
	friend class ArchiveTreeNode;
	friend class Archive;

public:
	typedef std::unique_ptr<ArchiveEntry> UPtr;
	typedef std::shared_ptr<ArchiveEntry> SPtr;
	typedef std::weak_ptr<ArchiveEntry>   WPtr;

	// Constructor/Destructor
	ArchiveEntry(string_view name = "", uint32_t size = 0);
	ArchiveEntry(ArchiveEntry& copy);
	~ArchiveEntry() = default;

	// Accessors
	const string&    name() const { return name_; }
	string_view      nameNoExt() const;
	const string&    upperName() const { return upper_name_; }
	string_view      upperNameNoExt() const;
	uint32_t         size() const { return data_loaded_ ? data_.size() : size_; }
	MemChunk&        data(bool allow_load = true);
	const uint8_t*   dataRaw(bool allow_load = true) { return data(allow_load).data(); }
	ArchiveTreeNode* parentDir() const { return parent_; }
	Archive*         parent() const;
	Archive*         topParent() const;
	string           path(bool name = false) const;
	EntryType*       type() const { return type_; }
	PropertyList&    exProps() { return ex_props_; }
	Property&        exProp(const string& key) { return ex_props_[key]; }
	uint8_t          state() const { return state_; }
	bool             isLocked() const { return locked_; }
	bool             isLoaded() const { return data_loaded_; }
	int              isEncrypted() const { return encrypted_; }
	ArchiveEntry*    nextEntry() const { return next_; }
	ArchiveEntry*    prevEntry() const { return prev_; }
	SPtr             getShared();

	// Modifiers (won't change entry state, except setState of course :P)
	void setName(string_view name);
	void setLoaded(bool loaded = true) { data_loaded_ = loaded; }
	void setType(EntryType* type, int r = 0)
	{
		this->type_  = type;
		reliability_ = r;
	}
	void setState(uint8_t state, bool silent = false);
	void setEncryption(int enc) { encrypted_ = enc; }
	void unloadData();
	void lock();
	void unlock();
	void lockState() { state_locked_ = true; }
	void unlockState() { state_locked_ = false; }
	void formatName(ArchiveFormat* format);

	// Entry modification (will change entry state)
	bool rename(string_view new_name);
	bool resize(uint32_t new_size, bool preserve_data);

	// Data modification
	void clearData();

	// Data import
	bool importMem(const void* data, uint32_t size);
	bool importMemChunk(MemChunk& mc);
	bool importFile(string_view filename, uint32_t offset = 0, uint32_t size = 0);
	bool importFileStream(const SFile& file, uint32_t len = 0);
	bool importEntry(ArchiveEntry* entry);

	// Data export
	bool exportFile(string_view filename);

	// Data access
	bool     write(const void* data, uint32_t size);
	bool     read(void* buf, uint32_t size);
	bool     seek(uint32_t offset, uint32_t start) { return data_.seek(offset, start); }
	uint32_t currentPos() const { return data_.currentPos(); }

	// Misc
	string        sizeString() const;
	string        typeString() const { return type_ ? type_->name() : "Unknown"; }
	void          stateChanged();
	void          setExtensionByType();
	int           typeReliability() const { return (type_ ? (type()->reliability() * reliability_ / 255) : 0); }
	bool          isInNamespace(string_view ns);
	ArchiveEntry* relativeEntry(string_view path, bool allow_absolute_path = true) const;

private:
	// Entry Info
	string           name_;
	string           upper_name_;
	uint32_t         size_;
	MemChunk         data_;
	EntryType*       type_;
	ArchiveTreeNode* parent_ = nullptr;
	PropertyList     ex_props_;

	// Entry status
	uint8_t state_        = 2;        // 0 = unmodified, 1 = modified, 2 = newly created (not saved to disk)
	bool    state_locked_ = false;    // If true the entry state cannot be changed (used for initial loading)
	bool    locked_       = false;    // If true the entry data+info cannot be changed
	bool    data_loaded_  = true;     // True if the entry's data is currently loaded into the data MemChunk
	int     encrypted_    = ENC_NONE; // Is there some encrypting on the archive?

	// Misc stuff
	int           reliability_ = 0; // The reliability of the entry's identification
	ArchiveEntry* next_        = nullptr;
	ArchiveEntry* prev_        = nullptr;

	// For speed
	size_t index_guess_ = 0;
};
