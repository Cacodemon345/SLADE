
#include "App.h"

class PNGChunk
{
public:
	PNGChunk(string_view name = "----")
	{
		// Init variables
		for (unsigned c = 0; c < name.size(); ++c)
			name_[c] = name[c];
	}

	~PNGChunk() = default;

	string_view name() const { return name_; }
	uint32_t    size() const { return size_; }
	uint32_t    crc() const { return crc_; }
	MemChunk&   data() { return data_; }

	void read(MemChunk& mc)
	{
		// Read size and chunk name
		mc.read(&size_, 4);
		mc.read(name_, 4);

		// Endianness correction
		size_ = wxUINT32_SWAP_ON_LE(size_);

		// Read chunk data
		data_.clear();
		mc.readMC(data_, size_);

		// Read crc
		mc.read(&crc_, 4);

		// Endianness correction
		crc_ = wxUINT32_SWAP_ON_LE(crc_);
	}

	void write(MemChunk& mc) const
	{
		// Endianness correction
		uint32_t size_swapped = wxUINT32_SWAP_ON_LE(size_);
		uint32_t crc_swapped  = wxUINT32_SWAP_ON_LE(crc_);

		// Write chunk data
		mc.write(&size_swapped, 4);
		mc.write(name_, 4);
		mc.write(data_.data(), size_);
		mc.write(&crc_swapped, 4);
	}

	void setData(const uint8_t* data, uint32_t size)
	{
		// Read given data
		this->data_.clear();
		this->data_.write(data, size);

		// Update variables
		this->size_ = size;

		// Note that the CRC is not computed just from
		// the chunk data, but also the chunk name! So
		// we need to write all that in a temporary MC
		MemChunk fulldata;
		fulldata.reSize(4 + size);
		fulldata.seek(0, SEEK_SET);
		fulldata.write(name_, 4);
		fulldata.write(data, size);
		crc_ = fulldata.crc();
		fulldata.clear();
	}

	void setData(MemChunk& mc) { setData(mc.data(), mc.size()); }

private:
	uint32_t size_    = 0;
	char     name_[4] = { '\0', '\0', '\0', '\0' };
	MemChunk data_;
	uint32_t crc_ = 0;
};

// TODO: Keep PNG chunks in SImage so they are preserved between load/save
class SIFPng : public SIFormat
{
public:
	SIFPng() : SIFormat("png", "PNG", "png") {}

	bool isThisFormat(MemChunk& mc) override
	{
		// Reset MemChunk
		mc.seek(0, SEEK_SET);

		// Check size
		if (mc.size() > 8)
		{
			// Check for PNG header
			if (mc[0] == 137 && mc[1] == 80 && mc[2] == 78 && mc[3] == 71 && mc[4] == 13 && mc[5] == 10 && mc[6] == 26
				&& mc[7] == 10)
				return true;
		}

		return false;
	}

	SImage::Info getInfo(MemChunk& mc, int index) override
	{
		SImage::Info inf;
		inf.format = "png";
		inf.width  = 0;
		inf.height = 0;

		// Read first chunk
		mc.seek(8, SEEK_SET);
		PNGChunk chunk;
		chunk.read(mc);
		// Should be IHDR
		int bpp = 32;
		if (chunk.name() == "IHDR")
		{
			// Read IHDR data
			IhdrChunk ihdr;
			chunk.data().read(&ihdr, 13, 0);

			// Set info from IHDR
			inf.width  = ihdr.width;
			inf.height = ihdr.height;
			bpp        = ihdr.bpp;
			if (ihdr.coltype == 3 && ihdr.bpp == 8)
			{
				// Only 8bpp 'indexed' pngs are counted as PalMask for now, all others will be converted to RGBA
				inf.colformat   = SImage::Type::PalMask;
				inf.has_palette = true;
			}
			else
				inf.colformat = SImage::Type::RGBA;
		}

		// Look for other info chunks (grAb or alPh)
		while (true)
		{
			chunk.read(mc);

			// Set format to alpha map if alPh present (and 8bpp)
			if (bpp == 8 && chunk.name() == "alPh")
				inf.colformat = SImage::Type::AlphaMap;

			// Set offsets if grAb present
			else if (chunk.name() == "grAb")
			{
				// Read offsets
				int32_t xoff, yoff;
				chunk.data().read(&xoff, 4, 0);
				chunk.data().read(&yoff, 4);
				inf.offset_x = wxINT32_SWAP_ON_LE(xoff);
				inf.offset_y = wxINT32_SWAP_ON_LE(yoff);
			}

			// Stop on IDAT chunk
			else if (chunk.name() == "IDAT")
				break;
		}

		return inf;
	}

	int canWrite(SImage& image) override
	{
		// PNG format is always writable
		return WRITABLE;
	}

	bool canWriteType(SImage::Type type) override
	{
		// PNG format is always writable
		return true;
	}

	bool convertWritable(SImage& image, ConvertOptions opt) override
	{
		// Just convert to requested colour type

		// Paletted
		if (opt.col_format == SImage::Type::PalMask)
		{
			// Convert mask
			if (opt.mask_source == Mask::Alpha)
				image.cutoffMask(opt.alpha_threshold);
			else if (opt.mask_source == Mask::Colour)
				image.maskFromColour(opt.mask_colour, opt.pal_current);
			else
				image.fillAlpha(255);

			// Convert colours
			image.convertPaletted(opt.pal_target, opt.pal_current);
		}

		// RGBA
		else if (opt.col_format == SImage::Type::RGBA)
		{
			image.convertRGBA(opt.pal_current);

			// Convert alpha channel
			if (opt.mask_source == Mask::Colour)
				image.maskFromColour(opt.mask_colour, opt.pal_current);
			else if (opt.mask_source == Mask::Brightness)
				image.maskFromBrightness(opt.pal_current);
		}

		// Alpha Map
		else if (opt.col_format == SImage::Type::AlphaMap)
		{
			if (opt.mask_source == Mask::Alpha)
				image.convertAlphaMap(SImage::AlphaSource::Alpha, opt.pal_current);
			else if (opt.mask_source == Mask::Colour)
			{
				image.maskFromColour(opt.mask_colour, opt.pal_current);
				image.convertAlphaMap(SImage::AlphaSource::Alpha, opt.pal_current);
			}
			else
				image.convertAlphaMap(SImage::AlphaSource::Brightness, opt.pal_current);
		}

		// If transparency is disabled
		if (!opt.transparency)
			image.fillAlpha(255);

		return true;
	}

	bool writeOffset(SImage& image, ArchiveEntry* entry, point2_t offset) override
	{
		MemChunk mc;
		image.setXOffset(offset.x);
		image.setYOffset(offset.y);
		return (writeImage(image, mc, nullptr, 0) && entry->importMemChunk(mc));
	}

protected:
	bool readImage(SImage& image, MemChunk& data, int index) override
	{
		// Create FreeImage bitmap from entry data
		FIMEMORY*         mem = FreeImage_OpenMemory((BYTE*)data.data(), data.size());
		FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(mem, 0);
		FIBITMAP*         bm  = FreeImage_LoadFromMemory(fif, mem, 0);
		FreeImage_CloseMemory(mem);

		// Check it created/read ok
		if (!bm)
		{
			Global::error = "Error reading PNG data";
			return false;
		}

		// Get image info
		int  width  = FreeImage_GetWidth(bm);
		int  height = FreeImage_GetHeight(bm);
		int  bpp    = FreeImage_GetBPP(bm);
		auto type   = SImage::Type::RGBA;

		// Read extra info from various PNG chunks
		int32_t xoff       = 0;
		int32_t yoff       = 0;
		bool    alPh_chunk = false;
		bool    grAb_chunk = false;
		data.seek(8, SEEK_SET); // Start after PNG header
		PNGChunk chunk;
		while (true)
		{
			// Read next PNG chunk
			chunk.read(data);

			// Check for 'grAb' chunk
			if (!grAb_chunk && chunk.name() == "grAb")
			{
				// Read offsets
				chunk.data().read(&xoff, 4, 0);
				chunk.data().read(&yoff, 4);
				xoff       = wxINT32_SWAP_ON_LE(xoff);
				yoff       = wxINT32_SWAP_ON_LE(yoff);
				grAb_chunk = true;
			}

			// Check for 'alPh' chunk
			if (!alPh_chunk && chunk.name() == "alPh")
				alPh_chunk = true;

			// If both chunks are found no need to search further
			if (grAb_chunk && alPh_chunk)
				break;

			// Stop searching when we get to IDAT chunk
			if (chunk.name() == "IDAT")
				break;
		}

		// Get image palette if it exists
		RGBQUAD* bm_pal = FreeImage_GetPalette(bm);
		Palette  palette;
		if (bpp == 8 && bm_pal)
		{
			type  = SImage::Type::PalMask;
			int a = 0;
			int b = FreeImage_GetColorsUsed(bm);
			if (b > 256)
				b = 256;
			for (; a < b; a++)
				palette.setColour(a, ColRGBA(bm_pal[a].rgbRed, bm_pal[a].rgbGreen, bm_pal[a].rgbBlue, 255));
		}

		// If it's a ZDoom alpha map
		if (alPh_chunk && bpp == 8)
			type = SImage::Type::AlphaMap;

		// Create image
		if (bm_pal)
			image.create(width, height, type, &palette);
		else
			image.create(width, height, type, nullptr);

		// Load image data
		uint8_t* img_data = imageData(image);
		if (type == SImage::Type::PalMask || type == SImage::Type::AlphaMap)
		{
			// Flip vertically
			FreeImage_FlipVertical(bm);

			// Load indexed data
			unsigned c = 0;
			for (int row = 0; row < height; row++)
			{
				uint8_t* scanline = FreeImage_GetScanLine(bm, row);
				for (int x = 0; x < width; x++)
					img_data[c++] = scanline[x];
			}

			// Set mask
			if (type == SImage::Type::PalMask)
			{
				uint8_t* mask       = imageMask(image);
				uint8_t* alphatable = FreeImage_GetTransparencyTable(bm);
				if (alphatable)
				{
					for (int a = 0; a < width * height; a++)
						mask[a] = alphatable[img_data[a]];
				}
				else
					image.fillAlpha(255);
			}
		}
		else if (type == SImage::Type::RGBA)
		{
			// Convert to 32bpp & flip vertically
			FIBITMAP* rgb = FreeImage_ConvertTo32Bits(bm);
			FreeImage_FlipVertical(rgb);

			// Load raw RGBA data
			uint8_t* bits_rgb = FreeImage_GetBits(rgb);
			int      c        = 0;
			for (int a = 0; a < width * height; a++)
			{
				img_data[c++] = bits_rgb[a * 4 + 2]; // Red
				img_data[c++] = bits_rgb[a * 4 + 1]; // Green
				img_data[c++] = bits_rgb[a * 4];     // Blue
				img_data[c++] = bits_rgb[a * 4 + 3]; // Alpha
			}

			FreeImage_Unload(rgb);
		}

		// Set offsets
		image.setXOffset(xoff);
		image.setYOffset(yoff);

		// Clean up
		FreeImage_Unload(bm);

		return true;
	}

	bool writeImage(SImage& image, MemChunk& data, Palette* pal, int index) override
	{
		// Variables
		FIBITMAP* bm;
		uint8_t*  img_data = imageData(image);
		uint8_t*  img_mask = imageMask(image);
		auto      type     = image.type();
		int       width    = image.width();
		int       height   = image.height();

		if (type == SImage::Type::RGBA)
		{
			// Init 32bpp FIBITMAP
			bm = FreeImage_Allocate(width, height, 32, 0x0000FF00, 0x00FF0000, 0x000000FF);

			// Write image data
			uint8_t* bits = FreeImage_GetBits(bm);
			uint32_t c    = 0;
			for (int a = 0; a < width * height * 4; a += 4)
			{
				bits[c++] = img_data[a + 2];
				bits[c++] = img_data[a + 1];
				bits[c++] = img_data[a];
				bits[c++] = img_data[a + 3];
			}
		}
		else if (type == SImage::Type::PalMask)
		{
			// Init 8bpp FIBITMAP
			bm = FreeImage_Allocate(width, height, 8);

			// Get palette to use
			Palette usepal;
			if (image.hasPalette())
				usepal.copyPalette(&imagePalette(image));
			else if (pal)
				usepal.copyPalette(pal);

			// Set palette
			RGBQUAD* bm_pal = FreeImage_GetPalette(bm);
			for (int a = 0; a < 256; a++)
			{
				bm_pal[a].rgbRed   = usepal.colour(a).r;
				bm_pal[a].rgbGreen = usepal.colour(a).g;
				bm_pal[a].rgbBlue  = usepal.colour(a).b;
			}

			// Handle transparency if needed
			if (img_mask)
			{
				if (usepal.transIndex() < 0)
				{
					// Find unused colour (for transparency)
					short unused = image.findUnusedColour();

					// Set any transparent pixels to this colour (if we found an unused colour)
					bool has_trans = false;
					if (unused >= 0)
					{
						for (int a = 0; a < width * height; a++)
						{
							if (img_mask[a] == 0)
							{
								img_data[a] = unused;
								has_trans   = true;
							}
						}

						// Set palette transparency
						if (has_trans)
							usepal.setTransIndex(unused);
					}
				}

				// Set freeimage palette transparency if needed
				if (usepal.transIndex() >= 0)
					FreeImage_SetTransparentIndex(bm, usepal.transIndex());
			}

			// Write image data
			for (int row = 0; row < height; row++)
			{
				uint8_t* scanline = FreeImage_GetScanLine(bm, row);
				memcpy(scanline, img_data + (row * width), width);
			}
		}
		else if (type == SImage::Type::AlphaMap)
		{
			// Init 8bpp FIBITMAP
			bm = FreeImage_Allocate(width, height, 8);

			// Set palette (greyscale)
			RGBQUAD* bm_pal = FreeImage_GetPalette(bm);
			for (int a = 0; a < 256; a++)
			{
				bm_pal[a].rgbRed   = a;
				bm_pal[a].rgbGreen = a;
				bm_pal[a].rgbBlue  = a;
			}

			// Write image data
			for (int row = 0; row < height; row++)
			{
				uint8_t* scanline = FreeImage_GetScanLine(bm, row);
				memcpy(scanline, img_data + (row * width), width);
			}
		}
		else
			return false;

		// Flip the image
		FreeImage_FlipVertical(bm);

		// Write the image to a temp file
		FreeImage_Save(FIF_PNG, bm, App::path("temp.png", App::Dir::Temp).c_str());

		// Load it into a memchunk
		MemChunk png;
		png.importFile(App::path("temp.png", App::Dir::Temp));

		// Check it loaded ok
		if (png.size() == 0)
		{
			LOG_MESSAGE(1, "Error reading temporary file");
			return false;
		}

		// Write PNG header and IHDR
		const uint8_t* png_data = png.data();
		data.clear();
		data.write(png_data, 33);

		// Create grAb chunk with offsets (only if offsets exist)
		if (image.offset().x != 0 || image.offset().y != 0)
		{
			PNGChunk  grAb("grAb");
			GrabChunk gc = { wxINT32_SWAP_ON_LE((int32_t)image.offset().x),
							 wxINT32_SWAP_ON_LE((int32_t)image.offset().y) };
			grAb.setData((const uint8_t*)&gc, 8);
			grAb.write(data);
		}

		// Create alPh chunk if it's an alpha map
		if (type == SImage::Type::AlphaMap)
		{
			PNGChunk alPh("alPh");
			alPh.write(data);
		}

		// Write remaining PNG data
		data.write(png_data + 33, png.size() - 33);

		// Clean up
		wxRemoveFile(App::path("temp.png", App::Dir::Temp));

		// Success
		return true;
	}

private:
	// grAb chunk struct
	struct GrabChunk
	{
		int32_t xoff;
		int32_t yoff;
	};

	// IHDR chunk struct
	struct IhdrChunk
	{
		uint32_t width;
		uint32_t height;
		uint8_t  bpp;
		uint8_t  coltype;
		uint8_t  compression;
		uint8_t  filter;
		uint8_t  interlace;
	};
};
