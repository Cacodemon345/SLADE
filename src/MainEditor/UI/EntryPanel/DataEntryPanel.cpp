
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    DataEntryPanel.cpp
// Description: An EntryPanel for displaying entries that can be represented as
//              a table/grid of data
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
#include "DataEntryPanel.h"
#include "General/ColourConfiguration.h"
#include "General/UI.h"
#include "MainEditor/BinaryControlLump.h"
#include "MainEditor/MainEditor.h"


// -----------------------------------------------------------------------------
//
// DataEntryTable Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Returns the number of rows contained in the data
// -----------------------------------------------------------------------------
int DataEntryTable::GetNumberRows()
{
	if (row_stride_ == 0)
		return 0;
	else
		return ((data_stop_ ? data_stop_ : data_.size()) - data_start_) / row_stride_;
}

// -----------------------------------------------------------------------------
// Returns the number of columns for the current data type
// -----------------------------------------------------------------------------
int DataEntryTable::GetNumberCols()
{
	return columns_.size();
}

// -----------------------------------------------------------------------------
// Returns the string value for the cell at [row,col]
// -----------------------------------------------------------------------------
wxString DataEntryTable::GetValue(int row, int col)
{
	if (!data_.seek(data_start_ + ((row * row_stride_) + columns_[col].row_offset), 0))
		return "INVALID";

	// Signed integer column
	if (columns_[col].type == IntSigned)
	{
		if (columns_[col].size == 1)
		{
			int8_t val;
			data_.read(&val, 1);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 2)
		{
			int16_t val;
			data_.read(&val, 2);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 4)
		{
			int32_t val;
			data_.read(&val, 4);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 8)
		{
			int64_t val;
			data_.read(&val, 8);
			return S_FMT("%d", val);
		}
		return "INVALID SIZE";
	}

	// Unsigned integer column
	else if (columns_[col].type == IntUnsigned)
	{
		if (columns_[col].size == 1)
		{
			uint8_t val;
			data_.read(&val, 1);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 2)
		{
			uint16_t val;
			data_.read(&val, 2);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 4)
		{
			uint32_t val;
			data_.read(&val, 4);
			return S_FMT("%d", val);
		}
		else if (columns_[col].size == 8)
		{
			uint64_t val;
			data_.read(&val, 8);
			return S_FMT("%d", val);
		}
		return "INVALID SIZE";
	}

	// Fixed-point float column
	else if (columns_[col].type == Fixed)
	{
		if (columns_[col].size == 4)
		{
			int32_t val;
			data_.read(&val, 4);
			return S_FMT("%1.3f", (double)val / 65536.0);
		}
		return "INVALID SIZE";
	}

	// String column
	else if (columns_[col].type == String)
	{
		return wxString::FromAscii(data_.data() + data_.currentPos(), columns_[col].size);
	}

	// Custom value column
	else if (columns_[col].type == CustomValue)
	{
		int value = 0;
		if (columns_[col].size == 1)
		{
			int8_t val;
			data_.read(&val, 1);
			value = val;
		}
		else if (columns_[col].size == 2)
		{
			int16_t val;
			data_.read(&val, 2);
			value = val;
		}
		else if (columns_[col].size == 4)
		{
			int32_t val;
			data_.read(&val, 4);
			value = val;
		}
		else if (columns_[col].size == 8)
		{
			int64_t val;
			data_.read(&val, 8);
			value = val;
		}
		return S_FMT("%d: %s", value, columns_[col].getCustomValue(value));
	}

	return "UNKNOWN TYPE";
}

// -----------------------------------------------------------------------------
// Sets the value for the cell at [row,col] to [value]
// -----------------------------------------------------------------------------
void DataEntryTable::SetValue(int row, int col, const wxString& value)
{
	// Seek to data position
	if (!data_.seek(data_start_ + (row * row_stride_) + columns_[col].row_offset, 0))
		return;

	// Signed integer or custom value column
	if (columns_[col].type == IntSigned || columns_[col].type == CustomValue)
	{
		long long longval;
		value.ToLongLong(&longval);

		if (columns_[col].size == 1)
		{
			int8_t val = longval;
			data_.write(&val, 1);
		}
		else if (columns_[col].size == 2)
		{
			int16_t val = longval;
			data_.write(&val, 2);
		}
		else if (columns_[col].size == 4)
		{
			int32_t val = longval;
			data_.write(&val, 4);
		}
		else if (columns_[col].size == 8)
		{
			int64_t val = longval;
			data_.write(&val, 8);
		}
	}

	// Unsigned integer column
	else if (columns_[col].type == IntUnsigned)
	{
		long long longval;
		value.ToLongLong(&longval);
		if (longval < 0)
			longval = 0;

		if (columns_[col].size == 1)
		{
			uint8_t val = longval;
			data_.write(&val, 1);
		}
		else if (columns_[col].size == 2)
		{
			uint16_t val = longval;
			data_.write(&val, 2);
		}
		else if (columns_[col].size == 4)
		{
			uint32_t val = longval;
			data_.write(&val, 4);
		}
		else if (columns_[col].size == 8)
		{
			uint64_t val = longval;
			data_.write(&val, 8);
		}
	}

	// String column
	else if (columns_[col].type == String)
	{
		vector<char> str(columns_[col].size, 0);
		unsigned     minsize = MIN(columns_[col].size, value.size());
		for (unsigned a = 0; a < minsize; a++)
			str[a] = value[a];
		data_.write(str.data(), columns_[col].size);
	}

	// Set cell to modified colour
	bool set = true;
	for (auto& cell : cells_modified_)
	{
		if (cell.x == row && cell.y == col)
		{
			set = false;
			break;
		}
	}
	if (set)
		cells_modified_.emplace_back(row, col);

	// Set entry modified
	parent_->setDataModified(true);
}

// -----------------------------------------------------------------------------
// Returns the header label text for column [col]
// -----------------------------------------------------------------------------
wxString DataEntryTable::GetColLabelValue(int col)
{
	if ((unsigned)col < columns_.size())
		return columns_[col].name;

	return S_FMT("Column%d", col);
}

// -----------------------------------------------------------------------------
// Returns the header label text for [row]
// -----------------------------------------------------------------------------
wxString DataEntryTable::GetRowLabelValue(int row)
{
	return row_prefix_ + S_FMT("%d", row_first_ + row);
}

// -----------------------------------------------------------------------------
// Deletes [num] rows, starting at [pos]
// -----------------------------------------------------------------------------
bool DataEntryTable::DeleteRows(size_t pos, size_t num)
{
	// Copy existing data
	MemChunk copy;
	copy.write(data_.data(), data_.size());

	// Write new data (excluding deleted rows)
	unsigned start = data_start_ + (row_stride_ * pos);
	unsigned end   = data_start_ + (row_stride_ * (pos + num));
	data_.clear();
	data_.write(copy.data(), start);
	data_.write(copy.data() + end, copy.size() - end);

	// Update new rows
	vector<int> new_rows_new;
	for (int row : rows_new_)
	{
		if ((unsigned)row >= pos + num)
			new_rows_new.push_back(row - num);
		else if ((unsigned)row < pos)
			new_rows_new.push_back(row);
	}
	rows_new_ = new_rows_new;

	// Update modified cells
	vector<point2_t> new_cells_modified;
	for (auto& cell : cells_modified_)
	{
		if ((unsigned)cell.x >= pos + num)
			new_cells_modified.emplace_back(cell.x - (int)num, cell.y);
		else if ((unsigned)cell.x < pos)
			new_cells_modified.emplace_back(cell.x, cell.y);
	}
	cells_modified_ = new_cells_modified;

	// Send message to grid
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, pos, num);
	GetView()->ProcessTableMessage(msg);

	return true;
}

// -----------------------------------------------------------------------------
// Inserts [num] blank rows beginning at [pos]
// -----------------------------------------------------------------------------
bool DataEntryTable::InsertRows(size_t pos, size_t num)
{
	// Copy existing data
	MemChunk copy;
	copy.write(data_.data(), data_.size());

	// Write leading rows
	unsigned start = data_start_ + (row_stride_ * pos);
	data_.clear();
	data_.write(copy.data(), start);

	// Write new (blank) rows
	auto temp = new uint8_t[row_stride_ * num];
	memset(temp, 0, row_stride_ * num);
	data_.write(temp, row_stride_ * num);
	delete[] temp;

	// Write ending rows
	data_.write(copy.data() + start, copy.size() - start);

	// Update new rows
	for (int& row : rows_new_)
		if ((unsigned)row >= pos)
			row += num;
	for (unsigned a = 0; a < num; a++)
		rows_new_.push_back(pos + a);

	// Update modified cells
	for (auto& cell : cells_modified_)
		if (cell.x >= (int)pos)
			cell.x += num;

	// Send message to grid
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_INSERTED, pos, num);
	GetView()->ProcessTableMessage(msg);

	return true;
}

// -----------------------------------------------------------------------------
// Returns the (display) attributes for the cell at [row,col]
// -----------------------------------------------------------------------------
wxGridCellAttr* DataEntryTable::GetAttr(int row, int col, wxGridCellAttr::wxAttrKind kind)
{
	auto* attr = new wxGridCellAttr();

	// Check if cell is part of a new row
	bool new_row = false;
	for (int n_row : rows_new_)
	{
		if (n_row == row)
		{
			attr->SetTextColour(WXCOL(ColourConfiguration::colour("new")));
			new_row = true;
			break;
		}
	}

	// Check if cell is modified
	if (!new_row)
	{
		for (auto& cell : cells_modified_)
		{
			if (cell.x == row && cell.y == col)
			{
				attr->SetTextColour(WXCOL(ColourConfiguration::colour("modified")));
				break;
			}
		}
	}

	return attr;
}

// -----------------------------------------------------------------------------
// Determines the data structure (columns etc.) for [entry]'s type
// -----------------------------------------------------------------------------
bool DataEntryTable::setupDataStructure(ArchiveEntry* entry)
{
	// Clear existing
	data_.clear();
	data_clipboard_.clear();
	cells_modified_.clear();
	rows_new_.clear();
	columns_.clear();
	row_stride_ = 0;
	data_start_ = 0;
	data_stop_  = 0;
	row_first_  = 0;
	row_prefix_ = "";

	if (!entry)
		return true;

	// Load entry data
	data_.write(entry->dataRaw(), entry->size());

	// Setup columns
	string type = entry->type()->id();

	// VERTEXES
	if (type == "map_vertexes")
	{
		if (entry->exProp("MapFormat") == "doom64")
		{
			columns_.emplace_back("X Position", Fixed, 4, 0);
			columns_.emplace_back("Y Position", Fixed, 4, 4);
			row_stride_ = 8;
		}
		else
		{
			columns_.emplace_back("X Position", IntSigned, 2, 0);
			columns_.emplace_back("Y Position", IntSigned, 2, 2);
			row_stride_ = 4;
		}
	}

	// LINEDEFS
	else if (type == "map_linedefs")
	{
		// Doom format
		if (entry->exProp("MapFormat") == "doom")
		{
			columns_.emplace_back("Vertex 1", IntUnsigned, 2, 0);
			columns_.emplace_back("Vertex 2", IntUnsigned, 2, 2);
			columns_.emplace_back("Flags", IntUnsigned, 2, 4);
			columns_.emplace_back("Action Special", IntUnsigned, 2, 6);
			columns_.emplace_back("Sector Tag", IntUnsigned, 2, 8);
			columns_.emplace_back("Front Side", IntUnsigned, 2, 10);
			columns_.emplace_back("Back Side", IntUnsigned, 2, 12);
			row_stride_ = 14;
		}

		// Hexen format
		else if (entry->exProp("MapFormat") == "hexen")
		{
			columns_.emplace_back("Vertex 1", IntUnsigned, 2, 0);
			columns_.emplace_back("Vertex 2", IntUnsigned, 2, 2);
			columns_.emplace_back("Flags", IntUnsigned, 2, 4);
			columns_.emplace_back("Action Special", IntUnsigned, 1, 6);
			columns_.emplace_back("Arg 1", IntUnsigned, 1, 7);
			columns_.emplace_back("Arg 2", IntUnsigned, 1, 8);
			columns_.emplace_back("Arg 3", IntUnsigned, 1, 9);
			columns_.emplace_back("Arg 4", IntUnsigned, 1, 10);
			columns_.emplace_back("Arg 5", IntUnsigned, 1, 11);
			columns_.emplace_back("Front Side", IntUnsigned, 2, 12);
			columns_.emplace_back("Back Side", IntUnsigned, 2, 14);
			row_stride_ = 16;
		}

		// Doom 64 format
		else if (entry->exProp("MapFormat") == "doom64")
		{
			columns_.emplace_back("Vertex 1", IntUnsigned, 2, 0);
			columns_.emplace_back("Vertex 2", IntUnsigned, 2, 2);
			columns_.emplace_back("Flags", IntUnsigned, 4, 4);
			columns_.emplace_back("Action Special", IntUnsigned, 2, 8);
			columns_.emplace_back("Sector Tag", IntUnsigned, 2, 10);
			columns_.emplace_back("Front Side", IntUnsigned, 2, 12);
			columns_.emplace_back("Back Side", IntUnsigned, 2, 14);
			row_stride_ = 16;
		}
	}

	// SIDEDEFS
	else if (type == "map_sidedefs")
	{
		// Doom 64 format
		if (entry->exProp("MapFormat") == "doom64")
		{
			columns_.emplace_back("X Offset", IntSigned, 2, 0);
			columns_.emplace_back("Y Offset", IntSigned, 2, 2);
			columns_.emplace_back("Upper Texture", IntUnsigned, 2, 4);
			columns_.emplace_back("Lower Texture", IntUnsigned, 2, 6);
			columns_.emplace_back("Middle Texture", IntUnsigned, 2, 8);
			columns_.emplace_back("Sector", IntUnsigned, 2, 10);
			row_stride_ = 12;
		}

		// Doom/Hexen format
		else
		{
			columns_.emplace_back("X Offset", IntSigned, 2, 0);
			columns_.emplace_back("Y Offset", IntSigned, 2, 2);
			columns_.emplace_back("Upper Texture", String, 8, 4);
			columns_.emplace_back("Lower Texture", String, 8, 12);
			columns_.emplace_back("Middle Texture", String, 8, 20);
			columns_.emplace_back("Sector", IntUnsigned, 2, 28);
			row_stride_ = 30;
		}
	}

	// SECTORS
	else if (type == "map_sectors")
	{
		// Doom 64 format
		if (entry->exProp("MapFormat") == "doom64")
		{
			columns_.emplace_back("Floor Height", IntSigned, 2, 0);
			columns_.emplace_back("Ceiling Height", IntSigned, 2, 2);
			columns_.emplace_back("Floor Texture", IntUnsigned, 2, 4);
			columns_.emplace_back("Ceiling Texture", IntUnsigned, 2, 6);
			columns_.emplace_back("Floor Colour", IntUnsigned, 2, 8);
			columns_.emplace_back("Ceiling Colour", IntUnsigned, 2, 10);
			columns_.emplace_back("Thing Colour", IntUnsigned, 2, 12);
			columns_.emplace_back("Wall Top Colour", IntUnsigned, 2, 14);
			columns_.emplace_back("Wall Bottom Colour", IntUnsigned, 2, 16);
			columns_.emplace_back("Special", IntUnsigned, 2, 18);
			columns_.emplace_back("Tag", IntUnsigned, 2, 20);
			columns_.emplace_back("Flags", IntUnsigned, 2, 22);
			row_stride_ = 24;
		}

		// Doom/Hexen format
		else
		{
			columns_.emplace_back("Floor Height", IntSigned, 2, 0);
			columns_.emplace_back("Ceiling Height", IntSigned, 2, 2);
			columns_.emplace_back("Floor Texture", String, 8, 4);
			columns_.emplace_back("Ceiling Texture", String, 8, 12);
			columns_.emplace_back("Light Level", IntUnsigned, 2, 20);
			columns_.emplace_back("Special", IntUnsigned, 2, 22);
			columns_.emplace_back("Tag", IntUnsigned, 2, 24);
			row_stride_ = 26;
		}
	}

	// THINGS
	else if (type == "map_things")
	{
		// Doom format
		if (entry->exProp("MapFormat") == "doom")
		{
			columns_.emplace_back("X Position", IntSigned, 2, 0);
			columns_.emplace_back("Y Position", IntSigned, 2, 2);
			columns_.emplace_back("Direction", IntSigned, 2, 4);
			columns_.emplace_back("Type", IntUnsigned, 2, 6);
			columns_.emplace_back("Flags", IntUnsigned, 2, 8);
			row_stride_ = 10;
		}

		// Hexen format
		else if (entry->exProp("MapFormat") == "hexen")
		{
			columns_.emplace_back("ID", IntUnsigned, 2, 0);
			columns_.emplace_back("X Position", IntSigned, 2, 2);
			columns_.emplace_back("Y Position", IntSigned, 2, 5);
			columns_.emplace_back("Z Height", IntSigned, 2, 6);
			columns_.emplace_back("Direction", IntSigned, 2, 8);
			columns_.emplace_back("Type", IntUnsigned, 2, 10);
			columns_.emplace_back("Flags", IntUnsigned, 2, 12);
			columns_.emplace_back("Special", IntUnsigned, 1, 14);
			columns_.emplace_back("Arg 1", IntUnsigned, 1, 15);
			columns_.emplace_back("Arg 2", IntUnsigned, 1, 16);
			columns_.emplace_back("Arg 3", IntUnsigned, 1, 17);
			columns_.emplace_back("Arg 4", IntUnsigned, 1, 18);
			columns_.emplace_back("Arg 5", IntUnsigned, 1, 19);
			row_stride_ = 20;
		}

		// Doom64 format
		else
		{
			columns_.emplace_back("X Position", IntSigned, 2, 0);
			columns_.emplace_back("Y Position", IntSigned, 2, 2);
			columns_.emplace_back("Z Height", IntSigned, 2, 4);
			columns_.emplace_back("Direction", IntSigned, 2, 6);
			columns_.emplace_back("Type", IntSigned, 2, 8);
			columns_.emplace_back("Flags", IntSigned, 2, 10);
			columns_.emplace_back("ID", IntSigned, 2, 12);
			row_stride_ = 14;
		}
	}

	// SEGS
	else if (type == "map_segs")
	{
		columns_.emplace_back("Vertex 1", IntUnsigned, 2, 0);
		columns_.emplace_back("Vertex 2", IntUnsigned, 2, 2);
		columns_.emplace_back("Angle", IntSigned, 2, 4);
		columns_.emplace_back("Line", IntUnsigned, 2, 6);
		columns_.emplace_back("Side", CustomValue, 2, 8);
		columns_.back().addCustomValue(0, "Front");
		columns_.back().addCustomValue(1, "Back");
		columns_.emplace_back("Offset", IntSigned, 2, 10);
		row_stride_ = 12;
	}

	// SSECTORS
	else if (type == "map_ssectors")
	{
		columns_.emplace_back("Seg Count", IntUnsigned, 2, 0);
		columns_.emplace_back("First Seg", IntUnsigned, 2, 2);
		row_stride_ = 4;
	}

	// NODES
	else if (type == "map_nodes")
	{
		columns_.emplace_back("Partition X", IntSigned, 2, 0);
		columns_.emplace_back("Partition Y", IntSigned, 2, 2);
		columns_.emplace_back("Partition X Diff", IntSigned, 2, 4);
		columns_.emplace_back("Partition Y Diff", IntSigned, 2, 6);
		columns_.emplace_back("Right Box Top", IntSigned, 2, 8);
		columns_.emplace_back("Right Box Bottom", IntSigned, 2, 10);
		columns_.emplace_back("Right Box Left", IntSigned, 2, 12);
		columns_.emplace_back("Right Box Right", IntSigned, 2, 14);
		columns_.emplace_back("Left Box Top", IntSigned, 2, 16);
		columns_.emplace_back("Left Box Bottom", IntSigned, 2, 18);
		columns_.emplace_back("Left Box Left", IntSigned, 2, 20);
		columns_.emplace_back("Left Box Right", IntSigned, 2, 22);
		columns_.emplace_back("Right Child", IntUnsigned, 2, 24);
		columns_.emplace_back("Left Child", IntUnsigned, 2, 26);
		row_stride_ = 28;
	}

	// LIGHTS
	else if (type == "map_lights")
	{
		columns_.emplace_back("Red", IntUnsigned, 1, 0);
		columns_.emplace_back("Green", IntUnsigned, 1, 1);
		columns_.emplace_back("Blue", IntUnsigned, 1, 2);
		columns_.emplace_back("Pad (Unused)", IntUnsigned, 1, 3);
		columns_.emplace_back("Tag", IntUnsigned, 2, 4);
		row_stride_ = 6;
	}

	// SWITCHES
	else if (type == "switches")
	{
		columns_.emplace_back("Off Texture", String, 8, 0);
		columns_.emplace_back("On Texture", String, 8, 9);

		Column col_type("Type", CustomValue, 2, 18);
		col_type.addCustomValue(SWCH_DEMO, "Shareware");
		col_type.addCustomValue(SWCH_FULL, "Registered");
		col_type.addCustomValue(SWCH_COMM, "Commercial");
		columns_.push_back(col_type);

		row_stride_ = 20;
	}

	// ANIMATED
	else if (type == "animated")
	{
		Column col_type("Type", CustomValue, 1, 0);
		col_type.addCustomValue(0, "Flat");
		col_type.addCustomValue(1, "Texture");
		col_type.addCustomValue(2, "Flat (Decals)");
		col_type.addCustomValue(3, "Texture (Decals)");

		columns_.push_back(col_type);
		columns_.emplace_back("Last Texture", String, 8, 1);
		columns_.emplace_back("First Texture", String, 8, 10);
		columns_.emplace_back("Speed (Tics)", IntUnsigned, 4, 19);
		row_stride_ = 23;
	}

	// PNAMES
	else if (type == "pnames" || type == "notpnames")
	{
		columns_.emplace_back("Patch Name", String, 8, 0);
		row_stride_ = 8;
		data_start_ = 4;
	}

	// DIALOGUE
	else if (type == "map_dialog")
	{
		// Full version:
		columns_.emplace_back("Speaker ID", IntUnsigned, 4, 0);
		columns_.emplace_back("Drop Type", IntSigned, 4, 4);
		columns_.emplace_back("Item Check 1", IntSigned, 4, 8);
		columns_.emplace_back("Item Check 2", IntSigned, 4, 12);
		columns_.emplace_back("Item Check 3", IntSigned, 4, 16);
		columns_.emplace_back("Link", IntSigned, 4, 20);
		columns_.emplace_back("Speaker Name", String, 16, 24);
		columns_.emplace_back("Sound", String, 8, 40);
		columns_.emplace_back("Backdrop", String, 8, 48);
		columns_.emplace_back("Dialogue Text", String, 320, 56);
		unsigned offset = 320 + 56;
		row_stride_     = 1516;

		/*//Teaser version:
		columns.push_back(Column("Speaker ID", IntUnsigned, 4, 0));
		columns.push_back(Column("Drop Type", IntSigned, 4, 4));
		columns.push_back(Column("Voice Number", IntUnsigned, 4, 8));
		columns.push_back(Column("Speaker Name", String, 16, 12));
		columns.push_back(Column("Dialogue Text", String, 320, 28));
		unsigned offset = 320 + 28;
		row_stride = 1488;*/

		// Responses
		for (unsigned a = 1; a <= 5; a++)
		{
			columns_.emplace_back(S_FMT("Response %d: Give Type", a), IntSigned, 4, offset);
			columns_.emplace_back(S_FMT("Response %d: Item 1", a), IntSigned, 4, offset + 4);
			columns_.emplace_back(S_FMT("Response %d: Item 2", a), IntSigned, 4, offset + 8);
			columns_.emplace_back(S_FMT("Response %d: Item 3", a), IntSigned, 4, offset + 12);
			columns_.emplace_back(S_FMT("Response %d: Count 1", a), IntSigned, 4, offset + 16);
			columns_.emplace_back(S_FMT("Response %d: Count 2", a), IntSigned, 4, offset + 20);
			columns_.emplace_back(S_FMT("Response %d: Count 3", a), IntSigned, 4, offset + 24);
			columns_.emplace_back(S_FMT("Response %d: Choice Text", a), String, 32, offset + 28);
			columns_.emplace_back(S_FMT("Response %d: Success Text", a), String, 80, offset + 60);
			columns_.emplace_back(S_FMT("Response %d: Link", a), IntSigned, 4, offset + 140);
			columns_.emplace_back(S_FMT("Response %d: Log", a), IntUnsigned, 4, offset + 144);
			columns_.emplace_back(S_FMT("Response %d: Fail Text", a), String, 80, offset + 148);
			offset += 228;
		}
	}

	// GENMIDI
	else if (type == "genmidi")
	{
		columns_.emplace_back("Flags", IntUnsigned, 2, 0);
		columns_.emplace_back("Second Tune", IntUnsigned, 1, 2);
		columns_.emplace_back("Fixed Note", IntUnsigned, 1, 3);

		// Voice data
		unsigned offset = 4;
		for (size_t i = 1; i < 3; ++i)
		{
			columns_.emplace_back(S_FMT("V%d: Mod Multi", i), IntUnsigned, 1, offset + 0);
			columns_.emplace_back(S_FMT("V%d: Mod Attack", i), IntUnsigned, 1, offset + 1);
			columns_.emplace_back(S_FMT("V%d: Mod Sustain", i), IntUnsigned, 1, offset + 2);
			columns_.emplace_back(S_FMT("V%d: Mod Waveform", i), IntUnsigned, 1, offset + 3);
			columns_.emplace_back(S_FMT("V%d: Mod Key Scale", i), IntUnsigned, 1, offset + 4);
			columns_.emplace_back(S_FMT("V%d: Mod Output", i), IntUnsigned, 1, offset + 5);
			columns_.emplace_back(S_FMT("V%d: Feedback", i), IntUnsigned, 1, offset + 6);
			columns_.emplace_back(S_FMT("V%d: Car Multi", i), IntUnsigned, 1, offset + 7);
			columns_.emplace_back(S_FMT("V%d: Car Attack", i), IntUnsigned, 1, offset + 8);
			columns_.emplace_back(S_FMT("V%d: Car Sustain", i), IntUnsigned, 1, offset + 9);
			columns_.emplace_back(S_FMT("V%d: Car Waveform", i), IntUnsigned, 1, offset + 10);
			columns_.emplace_back(S_FMT("V%d: Car Key Scale", i), IntUnsigned, 1, offset + 11);
			columns_.emplace_back(S_FMT("V%d: Car Output", i), IntUnsigned, 1, offset + 12);
			columns_.emplace_back(S_FMT("V%d: Note Offset", i), IntSigned, 2, offset + 14);
			offset += 16;
		}
		row_stride_ = 36;
		data_start_ = 8;
		data_stop_  = 6308;

		// There are instrument names in a second table; unfortunately retrieving them would be hard
		// (there are 6300 bytes of offset between both, plus an additional row stride of 32 bytes).
		// Commenting out all the above code and uncommenting the one below allows to see the names.
		// However seeing the values is probably more generally useful, the names are standard GM.
		/*
		columns.push_back(Column("Instrument Name", String, 32, 0));
		row_stride = 32;
		data_start = 6308;
		*/
	}

	return !columns_.empty();
}

// -----------------------------------------------------------------------------
// Copies [num] rows' data beginning from [row]
// -----------------------------------------------------------------------------
void DataEntryTable::copyRows(int row, int num, bool add)
{
	if (!add)
		data_clipboard_.clear();

	data_clipboard_.write(data_.data() + data_start_ + (row * row_stride_), num * row_stride_);
}

// -----------------------------------------------------------------------------
// Inserts any previously copied rows at [row]
// -----------------------------------------------------------------------------
void DataEntryTable::pasteRows(int row)
{
	// Ignore if no copied data
	if (data_clipboard_.size() == 0)
		return;

	// Copy existing data
	MemChunk copy;
	copy.write(data_.data(), data_.size());

	// Write leading rows
	unsigned start = data_start_ + (row_stride_ * row);
	data_.clear();
	data_.write(copy.data(), start);

	// Write new rows
	data_.write(data_clipboard_.data(), data_clipboard_.size());

	// Write ending rows
	data_.write(copy.data() + start, copy.size() - start);

	// Update new rows
	int num = data_clipboard_.size() / row_stride_;
	for (int& n_row : rows_new_)
	{
		if (n_row >= row)
			n_row += num;
	}
	for (int a = 0; a < num; a++)
		rows_new_.push_back(row + a);

	// Send message to grid
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_INSERTED, row, num);
	GetView()->ProcessTableMessage(msg);
}


// -----------------------------------------------------------------------------
//
// DataEntryPanel Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// DataEntryPanel class constructor
// -----------------------------------------------------------------------------
DataEntryPanel::DataEntryPanel(wxWindow* parent) : EntryPanel(parent, "data")
{
	// Cell value combo box
	auto vbox = new wxBoxSizer(wxVERTICAL);
	sizer_main_->Add(vbox, 1, wxEXPAND);
	combo_cell_value_ = new wxComboBox(this, -1, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxTE_PROCESS_ENTER);
	vbox->Add(combo_cell_value_, 0, wxEXPAND | wxBOTTOM, UI::pad());

	// Create grid
	grid_data_ = new wxGrid(this, -1);
	vbox->Add(grid_data_, 1, wxEXPAND | wxBOTTOM, UI::pad());

	// Add actions to toolbar
	toolbar_->addActionGroup(
		"Data", { "data_add_row", "data_delete_row", "data_cut_row", "data_copy_row", "data_paste_row" });

	// Bind events
	Bind(wxEVT_KEY_DOWN, &DataEntryPanel::onKeyDown, this);
	grid_data_->Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &DataEntryPanel::onGridRightClick, this);
	grid_data_->Bind(wxEVT_GRID_SELECT_CELL, &DataEntryPanel::onGridCursorChanged, this);
	combo_cell_value_->Bind(wxEVT_COMBOBOX, &DataEntryPanel::onComboCellValueSet, this);
	combo_cell_value_->Bind(wxEVT_TEXT_ENTER, &DataEntryPanel::onComboCellValueSet, this);
}

// -----------------------------------------------------------------------------
// Loads data from [entry] into the panel
// -----------------------------------------------------------------------------
bool DataEntryPanel::loadEntry(ArchiveEntry* entry)
{
	// Load data table
	if (!table_data_)
		table_data_ = new DataEntryTable(this);
	table_data_->setupDataStructure(entry);
	grid_data_->ClearGrid();
	grid_data_->SetTable(table_data_);
	combo_cell_value_->Clear();

	// Set column widths
	grid_data_->SetColMinimalAcceptableWidth(UI::scalePx(64));
	for (int a = 0; a < table_data_->GetNumberCols(); a++)
		grid_data_->AutoSizeColLabelSize(a);
	grid_data_->ForceRefresh();

	Layout();

	return true;
}

// -----------------------------------------------------------------------------
// Saves any changes to the entry
// -----------------------------------------------------------------------------
bool DataEntryPanel::saveEntry()
{
	entry_->importMemChunk(table_data_->data());
	setModified(false);
	return true;
}

// -----------------------------------------------------------------------------
// Deletes currently selected row(s)
// -----------------------------------------------------------------------------
void DataEntryPanel::deleteRow()
{
	// Get selected rows
	wxArrayInt selected_rows = grid_data_->GetSelectedRows();

	// Delete row(s)
	if (selected_rows.empty() && grid_data_->GetGridCursorRow() >= 0)
		grid_data_->DeleteRows(grid_data_->GetGridCursorRow(), 1);
	else if (!selected_rows.empty())
	{
		// Go through selected rows
		for (unsigned a = 0; a < selected_rows.size(); a++)
		{
			// Delete row
			grid_data_->DeleteRows(selected_rows[a], 1);

			// Adjust remaining selection
			for (unsigned b = a; b < selected_rows.size(); b++)
			{
				if (selected_rows[b] > selected_rows[a])
					selected_rows[b] = selected_rows[b] - 1;
			}
		}
	}

	// Update grid
	// grid_data->UpdateDimensions();
	grid_data_->ClearSelection();
	grid_data_->ForceRefresh();
	setModified(true);
}

// -----------------------------------------------------------------------------
// Adds an empty row at the current selection cursor position
// -----------------------------------------------------------------------------
void DataEntryPanel::addRow()
{
	auto row = grid_data_->GetGridCursorRow();
	grid_data_->InsertRows(row < 0 ? 0 : row, 1);
	// grid_data->UpdateDimensions();
	grid_data_->ClearSelection();
	grid_data_->ForceRefresh();
	setModified(true);
}

// -----------------------------------------------------------------------------
// Copies data from the currently selected row(s)
// -----------------------------------------------------------------------------
void DataEntryPanel::copyRow(bool cut)
{
	// Get selected rows
	wxArrayInt selected_rows = grid_data_->GetSelectedRows();

	// Copy row(s)
	if (selected_rows.empty() && grid_data_->GetGridCursorRow() >= 0)
	{
		// Copy
		table_data_->copyRows(grid_data_->GetGridCursorRow(), 1);

		// Delete if cutting
		if (cut)
			grid_data_->DeleteRows(grid_data_->GetGridCursorRow(), 1);
	}
	else if (!selected_rows.empty())
	{
		// Copy
		table_data_->copyRows(selected_rows[0], 1);
		for (unsigned a = 1; a < selected_rows.size(); a++)
			table_data_->copyRows(selected_rows[a], 1, true);

		// Delete if cutting
		if (cut)
		{
			// Go through selected rows
			for (unsigned a = 0; a < selected_rows.size(); a++)
			{
				// Delete row
				grid_data_->DeleteRows(selected_rows[a], 1);

				// Adjust remaining selection
				for (unsigned b = a; b < selected_rows.size(); b++)
				{
					if (selected_rows[b] > selected_rows[a])
						selected_rows[b] = selected_rows[b] - 1;
				}
			}
		}
	}

	// Update grid
	// grid_data->UpdateDimensions();
	grid_data_->ClearSelection();
	grid_data_->ForceRefresh();
	setModified(true);
}

// -----------------------------------------------------------------------------
// Pastes previously copied row data at the current cursor position
// -----------------------------------------------------------------------------
void DataEntryPanel::pasteRow()
{
	table_data_->pasteRows(grid_data_->GetGridCursorRow());
	// grid_data->UpdateDimensions();
	grid_data_->ClearSelection();
	grid_data_->ForceRefresh();
	setModified(true);
}

// -----------------------------------------------------------------------------
// Shows a dialog to change the value of currently selected cells
// (single-column selection only)
// -----------------------------------------------------------------------------
void DataEntryPanel::changeValue() const
{
	// Get selection
	vector<point2_t> selection = getSelection();

	// Determine common value (if any)
	wxString initial_val;
	for (auto& cell : selection)
	{
		auto cell_value = grid_data_->GetCellValue(cell.x, cell.y);
		if (initial_val.empty())
			initial_val = cell_value;
		else if (initial_val != cell_value)
		{
			initial_val = "";
			break;
		}
	}

	// Create dialog
	wxDialog dlg(MainEditor::windowWx(), -1, "Change Value");

	auto          ci = table_data_->columnInfo(selection[0].y);
	wxArrayString choices;
	for (auto& custom_value : ci.custom_values)
		choices.Add(S_FMT("%d: %s", custom_value.first, custom_value.second));
	wxComboBox* combo = new wxComboBox(&dlg, -1, initial_val, wxDefaultPosition, wxDefaultSize, choices);

	auto* vbox = new wxBoxSizer(wxVERTICAL);
	dlg.SetSizer(vbox);
	vbox->Add(combo, 0, wxEXPAND | wxALL, 10);
	vbox->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);

	// Show dialog
	dlg.Fit();
	dlg.CenterOnParent();
	if (dlg.ShowModal() == wxID_OK)
	{
		// Get entered value
		wxString val = combo->GetValue();
		long     lval;
		if (!val.ToLong(&lval))
		{
			// Invalid number, check for option value
			wxString numpart = val.BeforeFirst(':');
			if (!numpart.ToLong(&lval))
				return;
		}

		// Apply value to selected cells
		for (auto& cell : selection)
			grid_data_->SetCellValue(cell.x, cell.y, S_FMT("%d", lval));
		grid_data_->ForceRefresh();
	}
}

// -----------------------------------------------------------------------------
// Handles any SAction messages (from the panel toolbar)
// -----------------------------------------------------------------------------
bool DataEntryPanel::handleAction(string_view action_id)
{
	if (action_id == "data_add_row")
		addRow();
	else if (action_id == "data_delete_row")
		deleteRow();
	else if (action_id == "data_copy_row")
		copyRow(false);
	else if (action_id == "data_cut_row")
		copyRow(true);
	else if (action_id == "data_paste_row")
		pasteRow();
	else if (action_id == "data_change_value")
		changeValue();
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Returns the column of the current selection (-1 if selection spans multiple
// columns)
// -----------------------------------------------------------------------------
int DataEntryPanel::colWithSelection() const
{
	vector<point2_t> selection = getSelection();

	if (selection.empty())
		return -1;

	int col = -1;
	for (auto& cell : selection)
	{
		if (col < 0)
			col = cell.y;
		else if (col != cell.y)
			return -1;
	}

	return col;
}

// -----------------------------------------------------------------------------
// Gets the positions of the currently selected cells
// -----------------------------------------------------------------------------
vector<point2_t> DataEntryPanel::getSelection() const
{
	vector<point2_t> selection;

	// Just go through entire grid
	int rows = table_data_->GetNumberRows();
	int cols = table_data_->GetNumberCols();
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			if (grid_data_->IsInSelection(r, c))
				selection.emplace_back(r, c);
		}
	}

	// If no selection, add current cursor cell
	if (selection.empty())
		selection.emplace_back(grid_data_->GetGridCursorRow(), grid_data_->GetGridCursorCol());

	return selection;
}


// -----------------------------------------------------------------------------
//
// DataEntryPanel Class Events
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Called when a key is pressed in the panel
// -----------------------------------------------------------------------------
void DataEntryPanel::onKeyDown(wxKeyEvent& e)
{
	// Cut
	if (e.CmdDown() && e.GetKeyCode() == 'X')
		copyRow(true);

	// Copy
	else if (e.CmdDown() && e.GetKeyCode() == 'C')
		copyRow(false);

	// Paste
	else if (e.CmdDown() && e.GetKeyCode() == 'V')
		pasteRow();

	else
		e.Skip();
}

// -----------------------------------------------------------------------------
// Called when the right mouse button is clicked on the grid
// -----------------------------------------------------------------------------
void DataEntryPanel::onGridRightClick(wxGridEvent& e)
{
	// Check if only one column is selected
	int col = colWithSelection();
	LOG_MESSAGE(2, "Column %d", col);

	wxMenu menu;
	SAction::fromId("data_add_row")->addToMenu(&menu);
	SAction::fromId("data_delete_row")->addToMenu(&menu);
	menu.AppendSeparator();
	SAction::fromId("data_cut_row")->addToMenu(&menu);
	SAction::fromId("data_copy_row")->addToMenu(&menu);
	SAction::fromId("data_paste_row")->addToMenu(&menu);
	if (col >= 0)
	{
		menu.AppendSeparator();
		SAction::fromId("data_change_value")->addToMenu(&menu);
	}
	PopupMenu(&menu);
}

// -----------------------------------------------------------------------------
// Called when the grid cursor changes cell
// -----------------------------------------------------------------------------
void DataEntryPanel::onGridCursorChanged(wxGridEvent& e)
{
	combo_cell_value_->Clear();
	auto col = table_data_->columnInfo(e.GetCol());
	for (auto& custom_value : col.custom_values)
		combo_cell_value_->AppendString(S_FMT("%d: %s", custom_value.first, custom_value.second));

	combo_cell_value_->SetValue(grid_data_->GetCellValue(e.GetRow(), e.GetCol()));
}

// -----------------------------------------------------------------------------
// Called when the cell value combo is changed (enter pressed or an option
// selected from the dropdown)
// -----------------------------------------------------------------------------
void DataEntryPanel::onComboCellValueSet(wxCommandEvent& e)
{
	int row = grid_data_->GetGridCursorRow();
	int col = grid_data_->GetGridCursorCol();
	grid_data_->SetCellValue(row, col, combo_cell_value_->GetValue());
	combo_cell_value_->SetValue(grid_data_->GetCellValue(row, col));
}
