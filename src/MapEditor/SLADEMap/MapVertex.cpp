
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapVertex.cpp
// Description: MapVertex class, represents a vertex object in a map
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
#include "MapVertex.h"
#include "MapLine.h"


// -----------------------------------------------------------------------------
//
// MapVertex Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// MapVertex class constructor
// -----------------------------------------------------------------------------
MapVertex::MapVertex(double x, double y, SLADEMap* parent) : MapObject{ Type::Vertex, parent }, position_{ x, y } {}

// -----------------------------------------------------------------------------
// Returns the object point [point].
// Currently for vertices this is always the vertex position
// -----------------------------------------------------------------------------
fpoint2_t MapVertex::point(Point point)
{
	return position_;
}

// -----------------------------------------------------------------------------
// Returns the value of the integer property matching [key]
// -----------------------------------------------------------------------------
int MapVertex::intProperty(const string& key)
{
	if (key == "x")
		return (int)position_.x;
	else if (key == "y")
		return (int)position_.y;
	else
		return MapObject::intProperty(key);
}

// -----------------------------------------------------------------------------
// Returns the value of the float property matching [key]
// -----------------------------------------------------------------------------
double MapVertex::floatProperty(const string& key)
{
	if (key == "x")
		return position_.x;
	else if (key == "y")
		return position_.y;
	else
		return MapObject::floatProperty(key);
}

// -----------------------------------------------------------------------------
// Sets the integer value of the property [key] to [value]
// -----------------------------------------------------------------------------
void MapVertex::setIntProperty(const string& key, int value)
{
	// Update modified time
	setModified();

	if (key == "x")
	{
		position_.x = value;
		for (auto& connected_line : connected_lines_)
			connected_line->resetInternals();
	}
	else if (key == "y")
	{
		position_.y = value;
		for (auto& connected_line : connected_lines_)
			connected_line->resetInternals();
	}
	else
		return MapObject::setIntProperty(key, value);
}

// -----------------------------------------------------------------------------
// Sets the float value of the property [key] to [value]
// -----------------------------------------------------------------------------
void MapVertex::setFloatProperty(const string& key, double value)
{
	// Update modified time
	setModified();

	if (key == "x")
		position_.x = value;
	else if (key == "y")
		position_.y = value;
	else
		return MapObject::setFloatProperty(key, value);
}

// -----------------------------------------------------------------------------
// Returns true if the property [key] can be modified via script
// -----------------------------------------------------------------------------
bool MapVertex::scriptCanModifyProp(const string& key)
{
	return !(key == "x" || key == "y");
}

// -----------------------------------------------------------------------------
// Adds [line] to the list of lines connected to this vertex
// -----------------------------------------------------------------------------
void MapVertex::connectLine(MapLine* line)
{
	VECTOR_ADD_UNIQUE(connected_lines_, line);
}

// -----------------------------------------------------------------------------
// Removes [line] from the list of lines connected to this vertex
// -----------------------------------------------------------------------------
void MapVertex::disconnectLine(MapLine* line)
{
	for (unsigned a = 0; a < connected_lines_.size(); a++)
	{
		if (connected_lines_[a] == line)
		{
			connected_lines_.erase(connected_lines_.begin() + a);
			return;
		}
	}
}

// -----------------------------------------------------------------------------
// Returns the connected line at [index]
// -----------------------------------------------------------------------------
MapLine* MapVertex::connectedLine(unsigned index)
{
	if (index > connected_lines_.size())
		return nullptr;

	return connected_lines_[index];
}

// -----------------------------------------------------------------------------
// Write all vertex info to a Backup struct
// -----------------------------------------------------------------------------
void MapVertex::writeBackup(Backup* backup)
{
	// Position
	backup->props_internal["x"] = position_.x;
	backup->props_internal["y"] = position_.y;
}

// -----------------------------------------------------------------------------
// Read all vertex info from a Backup struct
// -----------------------------------------------------------------------------
void MapVertex::readBackup(Backup* backup)
{
	// Position
	position_.x = backup->props_internal["x"].floatValue();
	position_.y = backup->props_internal["y"].floatValue();
}
