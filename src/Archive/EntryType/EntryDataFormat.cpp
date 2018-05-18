
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    EntryDataFormat.cpp
// Description: Entry data format detection system, still fairly unfinished but
//              good enough for now
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
#include "EntryDataFormat.h"
#include "MainEditor/BinaryControlLump.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace
{
std::map<string, EntryDataFormat*> data_formats;
EntryDataFormat*                   edf_any  = nullptr;
EntryDataFormat*                   edf_text = nullptr;
} // namespace


// -----------------------------------------------------------------------------
//
// EntryDataFormat Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// EntryDataFormat class constructor
// -----------------------------------------------------------------------------
EntryDataFormat::EntryDataFormat(string id) : id_{ id }, size_min_{ 0 }
{
	// Add to hash map
	data_formats[id] = this;
}

// -----------------------------------------------------------------------------
// To be overridden by specific data types, returns true if the data in [mc]
// matches the data format
// -----------------------------------------------------------------------------
int EntryDataFormat::isThisFormat(MemChunk& mc)
{
	return EDF_TRUE;
}


// -----------------------------------------------------------------------------
//
// EntryDataFormat Static Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the entry data format matching [id], or the 'any' type if no match
// found
// -----------------------------------------------------------------------------
EntryDataFormat* EntryDataFormat::getFormat(string_view id)
{
	auto format = data_formats[id.data()];
	return format ? format : edf_any;
}

// -----------------------------------------------------------------------------
// Returns the 'any' data format
// -----------------------------------------------------------------------------
EntryDataFormat* EntryDataFormat::anyFormat()
{
	return edf_any;
}

// -----------------------------------------------------------------------------
// Returns the 'text' data format
// -----------------------------------------------------------------------------
EntryDataFormat* EntryDataFormat::textFormat()
{
	return edf_text;
}

// Special format that always returns false on detection
// Used when a format is requested that doesn't exist
class AnyDataFormat : public EntryDataFormat
{
public:
	AnyDataFormat() : EntryDataFormat("any") {}
	~AnyDataFormat() = default;

	int isThisFormat(MemChunk& mc) override { return EDF_FALSE; }
};

// Format enumeration moved to separate files
#include "DataFormats/ArchiveFormats.h"
#include "DataFormats/AudioFormats.h"
#include "DataFormats/ImageFormats.h"
#include "DataFormats/LumpFormats.h"
#include "DataFormats/MiscFormats.h"
#include "DataFormats/ModelFormats.h"

// -----------------------------------------------------------------------------
// Initialises all built-in data formats (this is currently all formats, as
// externally defined formats are not implemented yet)
// -----------------------------------------------------------------------------
void EntryDataFormat::initBuiltinFormats()
{
	// Create the 'any' format
	edf_any = new AnyDataFormat();

	// Just need to create an instance of each builtin format class
	// TODO: Ugly ugly ugly, need a better way of doing this, defining each data format in a single place etc
	// TODO: OR, reimplement this stuff in lua, will need to see how much slower it will be
	new PNGDataFormat();
	new BMPDataFormat();
	new GIFDataFormat();
	new PCXDataFormat();
	new TGADataFormat();
	new TIFFDataFormat();
	new JPEGDataFormat();
	new ILBMDataFormat();
	new DoomGfxDataFormat();
	new DoomGfxAlphaDataFormat();
	new DoomGfxBetaDataFormat();
	new DoomSneaDataFormat();
	new DoomArahDataFormat();
	new DoomPSXDataFormat();
	new DoomJaguarDataFormat();
	new DoomJagTexDataFormat();
	new DoomJagSpriteDataFormat();
	new ShadowCasterSpriteFormat();
	new ShadowCasterWallFormat();
	new ShadowCasterGfxFormat();
	new AnaMipImageFormat();
	new BuildTileFormat();
	new Heretic2M8Format();
	new Heretic2M32Format();
	new HalfLifeTextureFormat();
	new IMGZDataFormat();
	new QuakeGfxDataFormat();
	new QuakeSpriteDataFormat();
	new QuakeTexDataFormat();
	new QuakeIIWalDataFormat();
	new RottGfxDataFormat();
	new RottTransGfxDataFormat();
	new RottLBMDataFormat();
	new RottRawDataFormat();
	new RottPicDataFormat();
	new WolfPicDataFormat();
	new WolfSpriteDataFormat();
	new JediBMFormat();
	new JediFMEFormat();
	new JediWAXFormat();
	new JediFNTFormat();
	new JediFONTFormat();
	// new JediDELTFormat();
	// new JediANIMFormat();
	new WadDataFormat();
	new ZipDataFormat();
	new LibDataFormat();
	new DatDataFormat();
	new ResDataFormat();
	new PakDataFormat();
	new BSPDataFormat();
	new GrpDataFormat();
	new RffDataFormat();
	new GobDataFormat();
	new LfdDataFormat();
	new HogDataFormat();
	new ADatDataFormat();
	new Wad2DataFormat();
	new WadJDataFormat();
	new WolfDataFormat();
	new GZipDataFormat();
	new BZip2DataFormat();
	new TarDataFormat();
	new DiskDataFormat();
	new PodArchiveDataFormat();
	new ChasmBinArchiveDataFormat();
	new SinArchiveDataFormat();
	new MUSDataFormat();
	new MIDIDataFormat();
	new XMIDataFormat();
	new HMIDataFormat();
	new HMPDataFormat();
	new GMIDDataFormat();
	new RMIDDataFormat();
	new ITModuleDataFormat();
	new XMModuleDataFormat();
	new S3MModuleDataFormat();
	new MODModuleDataFormat();
	new OKTModuleDataFormat();
	new DRODataFormat();
	new RAWDataFormat();
	new IMFDataFormat();
	new IMFRawDataFormat();
	new DoomSoundDataFormat();
	new WolfSoundDataFormat();
	new DoomMacSoundDataFormat();
	new DoomPCSpeakerDataFormat();
	new AudioTPCSoundDataFormat();
	new AudioTAdlibSoundDataFormat();
	new JaguarDoomSoundDataFormat();
	new VocDataFormat();
	new AYDataFormat();
	new GBSDataFormat();
	new GYMDataFormat();
	new HESDataFormat();
	new KSSDataFormat();
	new NSFDataFormat();
	new NSFEDataFormat();
	new SAPDataFormat();
	new SPCDataFormat();
	new VGMDataFormat();
	new VGZDataFormat();
	new BloodSFXDataFormat();
	new WAVDataFormat();
	new SunSoundDataFormat();
	new AIFFSoundDataFormat();
	new OggDataFormat();
	new FLACDataFormat();
	new MP2DataFormat();
	new MP3DataFormat();
	new TextureXDataFormat();
	new PNamesDataFormat();
	new ACS0DataFormat();
	new ACSEDataFormat();
	new ACSeDataFormat();
	new BoomAnimatedDataFormat();
	new BoomSwitchesDataFormat();
	new Font0DataFormat();
	new Font1DataFormat();
	new Font2DataFormat();
	new BMFontDataFormat();
	new FontWolfDataFormat();
	new ZNodesDataFormat();
	new ZGLNodesDataFormat();
	new ZGLNodes2DataFormat();
	new XNodesDataFormat();
	new XGLNodesDataFormat();
	new XGLNodes2DataFormat();
	new DMDModelDataFormat();
	new MDLModelDataFormat();
	new MD2ModelDataFormat();
	new MD3ModelDataFormat();
	new VOXVoxelDataFormat();
	new KVXVoxelDataFormat();
	new RLE0DataFormat();

	// And here are some dummy formats needed for certain image formats
	// that can't be detected by anything but size (which is done in EntryType detection anyway)
	new EntryDataFormat("img_raw");
	new EntryDataFormat("img_rottwall");
	new EntryDataFormat("img_planar");
	new EntryDataFormat("img_4bitchunk");
	new EntryDataFormat("font_mono");

	// Dummy for generic raw data format
	new EntryDataFormat("rawdata");

	// Another dummy for the generic text format
	edf_text = new EntryDataFormat("text");
}
