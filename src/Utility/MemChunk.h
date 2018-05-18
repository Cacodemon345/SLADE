#pragma once

class MemChunk
{
public:
	MemChunk(uint32_t size = 0);
	MemChunk(const uint8_t* data, uint32_t size);
	~MemChunk();

	uint8_t& operator[](int a) const { return data_[a]; }

	// Accessors
	const uint8_t* data() const { return data_; }
	uint32_t       size() const { return size_; }

	bool hasData() const { return (size_ > 0 && data_); }

	bool clear();
	bool reSize(uint32_t new_size, bool preserve_data = true);

	// Data import
	bool importFile(string_view filename, uint32_t offset = 0, uint32_t len = 0);
	bool importFileStream(wxFile& file, uint32_t len = 0);
	bool importMem(const uint8_t* start, uint32_t len);

	// Data export
	bool exportFile(string_view filename, uint32_t start = 0, uint32_t size = 0) const;
	bool exportMemChunk(MemChunk& mc, uint32_t start = 0, uint32_t size = 0) const;

	// C-style reading/writing
	bool     write(const void* data, uint32_t size);
	bool     write(const void* data, uint32_t size, uint32_t start);
	bool     read(void* buf, uint32_t size);
	bool     read(void* buf, uint32_t size, uint32_t start);
	bool     seek(uint32_t offset, uint32_t start);
	uint32_t currentPos() const { return cur_ptr_; }

	// Extended C-style reading/writing
	bool readMC(MemChunk& mc, uint32_t size);

	// Misc
	bool     fillData(uint8_t val) const;
	uint32_t crc() const;

protected:
	uint8_t* data_    = nullptr;
	uint32_t cur_ptr_ = 0;
	uint32_t size_    = 0;

	uint8_t* allocData(uint32_t size, bool set_data = true);
};
