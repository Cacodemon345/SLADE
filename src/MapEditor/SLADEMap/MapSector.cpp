
/*******************************************************************
 * SLADE - It's a Doom Editor
 * Copyright (C) 2008-2014 Simon Judd
 *
 * Email:       sirjuddington@gmail.com
 * Web:         http://slade.mancubus.net
 * Filename:    MapSector.cpp
 * Description: MapSector class, represents a sector object in a map
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *******************************************************************/


/*******************************************************************
 * INCLUDES
 *******************************************************************/
#include "Main.h"
#include "MapSector.h"
#include "MapLine.h"
#include "MapSide.h"
#include "MapVertex.h"
#include "App.h"
#include "SLADEMap.h"
#include "Utility/MathStuff.h"
#include "Game/Configuration.h"

// Number of radians in the unit circle
const double TAU = M_PI * 2;


/*******************************************************************
 * MAPSECTOR CLASS FUNCTIONS
 *******************************************************************/

/* MapSector::MapSector
 * MapSector class constructor
 *******************************************************************/
MapSector::MapSector(SLADEMap* parent) :
	MapObject(Type::Sector, parent),
	geometry_updated_{ App::runTimer() }
{
}

/* MapSector::MapSector
 * MapSector class constructor
 *******************************************************************/
MapSector::MapSector(string f_tex, string c_tex, SLADEMap* parent) :
	MapObject(Type::Sector, parent),
	tex_floor_{ f_tex },
	tex_ceiling_{ c_tex },
	geometry_updated_{ App::runTimer() }
{
}

/* MapSector::~MapSector
 * MapSector class destructor
 *******************************************************************/
MapSector::~MapSector()
{
}

/* MapSector::copy
 * Copies another map object [s]
 *******************************************************************/
void MapSector::copy(MapObject* s)
{
	// Don't copy a non-sector
	if (s->objType() != Type::Sector)
		return;

	setModified();

	// Update texture counts (decrement previous)
	if (parent_map_)
	{
		parent_map_->updateFlatUsage(tex_floor_, -1);
		parent_map_->updateFlatUsage(tex_ceiling_, -1);
	}

	// Basic variables
	MapSector* sector = (MapSector*)s;
	this->tex_floor_ = sector->tex_floor_;
	this->tex_ceiling_ = sector->tex_ceiling_;
	this->height_floor_ = sector->height_floor_;
	this->height_ceiling_ = sector->height_ceiling_;
	this->light_ = sector->light_;
	this->special_ = sector->special_;
	this->id_ = sector->id_;
	plane_floor_.set(0, 0, 1, sector->height_floor_);
	plane_ceiling_.set(0, 0, 1, sector->height_ceiling_);

	// Update texture counts (increment new)
	if (parent_map_)
	{
		parent_map_->updateFlatUsage(tex_floor_, 1);
		parent_map_->updateFlatUsage(tex_ceiling_, 1);
	}

	// Other properties
	MapObject::copy(s);
}

/* MapSector::setGeometryUpdated
 * Update the last time the sector geometry changed
 *******************************************************************/
void MapSector::setGeometryUpdated()
{
	geometry_updated_ = App::runTimer();
}

/* MapSector::stringProperty
 * Returns the value of the string property matching [key]
 *******************************************************************/
string MapSector::stringProperty(const string& key)
{
	if (key == "texturefloor")
		return tex_floor_;
	else if (key == "textureceiling")
		return tex_ceiling_;
	else
		return MapObject::stringProperty(key);
}

/* MapSector::intProperty
 * Returns the value of the integer property matching [key]
 *******************************************************************/
int MapSector::intProperty(const string& key)
{
	if (key == "heightfloor")
		return height_floor_;
	else if (key == "heightceiling")
		return height_ceiling_;
	else if (key == "lightlevel")
		return light_;
	else if (key == "special")
		return special_;
	else if (key == "id")
		return id_;
	else
		return MapObject::intProperty(key);
}

/* MapSector::setStringProperty
 * Sets the string value of the property [key] to [value]
 *******************************************************************/
void MapSector::setStringProperty(const string& key, const string& value)
{
	// Update modified time
	setModified();

	if (key == "texturefloor")
	{
		if (parent_map_) parent_map_->updateFlatUsage(tex_floor_, -1);
		tex_floor_ = value;
		if (parent_map_) parent_map_->updateFlatUsage(tex_floor_, 1);
	}
	else if (key == "textureceiling")
	{
		if (parent_map_) parent_map_->updateFlatUsage(tex_ceiling_, -1);
		tex_ceiling_ = value;
		if (parent_map_) parent_map_->updateFlatUsage(tex_ceiling_, 1);
	}
	else
		return MapObject::setStringProperty(key, value);
}

/* MapSector::setFloatProperty
 * Sets the float value of the property [key] to [value]
 *******************************************************************/
void MapSector::setFloatProperty(const string& key, double value)
{
	using Game::UDMFFeature;

	// Check if flat offset/scale/rotation is changing (if UDMF)
	if (parent_map_->currentFormat() == MAP_UDMF)
	{
		if ((Game::configuration().featureSupported(UDMFFeature::FlatPanning) &&
			(key == "xpanningfloor" || key == "ypanningfloor")) ||
			(Game::configuration().featureSupported(UDMFFeature::FlatScaling) &&
			(key == "xscalefloor" || key == "yscalefloor" ||
			 key == "xscaleceiling" || key == "yscaleceiling")) || 
			(Game::configuration().featureSupported(UDMFFeature::FlatRotation) &&
			(key == "rotationfloor" || key == "rotationceiling")))
			polygon_.setTexture(nullptr);	// Clear texture to force update
	}

	MapObject::setFloatProperty(key, value);
}

/* MapSector::setIntProperty
 * Sets the integer value of the property [key] to [value]
 *******************************************************************/
void MapSector::setIntProperty(const string& key, int value)
{
	// Update modified time
	setModified();

	if (key == "heightfloor")
		setFloorHeight(value);
	else if (key == "heightceiling")
		setCeilingHeight(value);
	else if (key == "lightlevel")
		light_ = value;
	else if (key == "special")
		special_ = value;
	else if (key == "id")
		id_ = value;
	else
		MapObject::setIntProperty(key, value);
}

void MapSector::setFloorHeight(short height)
{
	setModified();
	height_floor_ = height;
	setFloorPlane(plane_t::flat(height));
}

void MapSector::setCeilingHeight(short height)
{
	setModified();
	height_ceiling_ = height;
	setCeilingPlane(plane_t::flat(height));
}

/* MapLine::getPoint
 * Returns the object point [point]: MOBJ_POINT_MID = the absolute
 * mid point of the sector, MOBJ_POINT_WITHIN/MOBJ_POINT_TEXT =
 * a calculated point that is within the actual sector
 *******************************************************************/
fpoint2_t MapSector::point(Point point)
{
	if (point == Point::Mid)
	{
		bbox_t bbox = boundingBox();
		return fpoint2_t(bbox.min.x + ((bbox.max.x-bbox.min.x)*0.5),
			bbox.min.y + ((bbox.max.y-bbox.min.y)*0.5));
	}
	else
	{
		if (text_point_.x == 0 && text_point_.y == 0 && parent_map_)
			parent_map_->findSectorTextPoint(this);
		return text_point_;
	}
}

/* MapSector::updateBBox
 * Calculates the sector's bounding box
 *******************************************************************/
void MapSector::updateBBox()
{
	// Reset bounding box
	bbox_.reset();

	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		MapLine* line = connected_sides_[a]->parentLine();
		if (!line) continue;
		bbox_.extend(line->v1()->xPos(), line->v1()->yPos());
		bbox_.extend(line->v2()->xPos(), line->v2()->yPos());
	}

	text_point_.set(0, 0);
	setGeometryUpdated();
}

/* MapSector::boundingBox
 * Returns the sector bounding box
 *******************************************************************/
bbox_t MapSector::boundingBox()
{
	// Update bbox if needed
	if (!bbox_.is_valid())
		updateBBox();

	return bbox_;
}

/* MapSector::getPolygon
 * Returns the sector polygon, updating it if necessary
 *******************************************************************/
Polygon2D* MapSector::getPolygon()
{
	if (poly_needsupdate_)
	{
		polygon_.openSector(this);
		poly_needsupdate_ = false;
	}

	return &polygon_;
}

/* MapSector::isWithin
 * Returns true if the point is inside the sector
 *******************************************************************/
bool MapSector::isWithin(fpoint2_t point)
{
	// Check with bbox first
	if (!boundingBox().contains(point))
		return false;

	// Find nearest line in the sector
	double dist;
	double min_dist = 999999;
	MapLine* nline = nullptr;
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		// Calculate distance to line
		//if (connected_sides[a] == NULL) {
		//	LOG_MESSAGE(3, "Warning: connected side #%i is a NULL pointer!", a);
		//	continue;
		//} else if (connected_sides[a]->getParentLine() == NULL) {
		//	LOG_MESSAGE(3, "Warning: connected side #%i has a NULL pointer parent line!", connected_sides[a]->getIndex());
		//	continue;
		//}
		dist = connected_sides_[a]->parentLine()->distanceTo(point);

		// Check distance
		if (dist < min_dist)
		{
			nline = connected_sides_[a]->parentLine();
			min_dist = dist;
		}
	}

	// No nearest (shouldn't happen)
	if (!nline)
		return false;

	// Check the side of the nearest line
	double side = MathStuff::lineSide(point, nline->seg());
	if (side >= 0 && nline->frontSector() == this)
		return true;
	else if (side < 0 && nline->backSector() == this)
		return true;
	else
		return false;
}

/* MapSector::distanceTo
 * Returns the minimum distance from the point to the closest line in
 * the sector
 *******************************************************************/
double MapSector::distanceTo(fpoint2_t point, double maxdist)
{
	// Init
	if (maxdist < 0)
		maxdist = 9999999;

	// Check bounding box first
	if (!bbox_.is_valid())
		updateBBox();
	double min_dist = 9999999;
	double dist = MathStuff::distanceToLine(point, bbox_.left_side());
	if (dist < min_dist) min_dist = dist;
	dist = MathStuff::distanceToLine(point, bbox_.top_side());
	if (dist < min_dist) min_dist = dist;
	dist = MathStuff::distanceToLine(point, bbox_.right_side());
	if (dist < min_dist) min_dist = dist;
	dist = MathStuff::distanceToLine(point, bbox_.bottom_side());
	if (dist < min_dist) min_dist = dist;

	if (min_dist > maxdist && !bbox_.contains(point))
		return -1;

	// Go through connected sides
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		// Get side parent line
		MapLine* line = connected_sides_[a]->parentLine();
		if (!line) continue;

		// Check distance
		dist = line->distanceTo(point);
		if (dist < min_dist)
			min_dist = dist;
	}

	return min_dist;
}

/* MapSector::getLines
 * Adds all lines that are part of the sector to [list]
 *******************************************************************/
bool MapSector::getLines(vector<MapLine*>& list)
{
	// Go through connected sides
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		// Add the side's parent line to the list if it doesn't already exist
		if (std::find(list.begin(), list.end(), connected_sides_[a]->parentLine()) == list.end())
			list.push_back(connected_sides_[a]->parentLine());
	}

	return true;
}

/* MapSector::getVertices
 * Adds all vertices that are part of the sector to [list]
 *******************************************************************/
bool MapSector::getVertices(vector<MapVertex*>& list)
{
	// Go through connected sides
	MapLine* line;
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		line = connected_sides_[a]->parentLine();

		// Add the side's parent line's vertices to the list if they doesn't already exist
		if (line->v1() && std::find(list.begin(), list.end(), line->v1()) == list.end())
			list.push_back(line->v1());
		if (line->v2() && std::find(list.begin(), list.end(), line->v2()) == list.end())
			list.push_back(line->v2());
	}

	return true;
}

/* MapSector::getVertices
 * Adds all vertices that are part of the sector to [list]
 *******************************************************************/
bool MapSector::getVertices(vector<MapObject*>& list)
{
	// Go through connected sides
	MapLine* line;
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		line = connected_sides_[a]->parentLine();

		// Add the side's parent line's vertices to the list if they doesn't already exist
		if (line->v1() && std::find(list.begin(), list.end(), line->v1()) == list.end())
			list.push_back(line->v1());
		if (line->v2() && std::find(list.begin(), list.end(), line->v2()) == list.end())
			list.push_back(line->v2());
	}

	return true;
}

/* MapSector::getLight
 * Returns the light level of the sector at [where] - 1 = floor,
 * 2 = ceiling
 *******************************************************************/
uint8_t MapSector::getLight(int where)
{
	// Check for UDMF + flat lighting
	if (parent_map_->currentFormat() == MAP_UDMF &&
		Game::configuration().featureSupported(Game::UDMFFeature::FlatLighting))
	{
		// Get general light level
		int l = light_;

		// Get specific light level
		if (where == 1)
		{
			// Floor
			int fl = intProperty("lightfloor");
			if (boolProperty("lightfloorabsolute"))
				l = fl;
			else
				l += fl;
		}
		else if (where == 2)
		{
			// Ceiling
			int cl = intProperty("lightceiling");
			if (boolProperty("lightceilingabsolute"))
				l = cl;
			else
				l += cl;
		}

		// Clamp light level
		if (l > 255)
			l = 255;
		if (l < 0)
			l = 0;

		return l;
	}
	else
	{
		// Clamp light level
		int l = light_;
		if (l > 255)
			l = 255;
		if (l < 0)
			l = 0;

		return l;
	}
}

/* MapSector::changeLight
 * Changes the sector light level by [amount]
 *******************************************************************/
void MapSector::changeLight(int amount, int where)
{
	// Get current light level
	int ll = getLight(where);

	// Clamp amount
	if (ll + amount > 255)
		amount -= ((ll+amount) - 255);
	else if (ll + amount < 0)
		amount = -ll;

	// Check for UDMF + flat lighting independent from the sector
	bool separate = parent_map_->currentFormat() == MAP_UDMF &&
					Game::configuration().featureSupported(Game::UDMFFeature::FlatLighting);

	// Change light level by amount
	if (where == 1 && separate)
	{
		int cur = intProperty("lightfloor");
		setIntProperty("lightfloor", cur + amount);
	}
	else if (where == 2 && separate)
	{
		int cur = intProperty("lightceiling");
		setIntProperty("lightceiling", cur + amount);
	}
	else
	{
		setModified();
		light_ = ll + amount;
	}
}

/* MapSector::getColour
 * Returns the colour of the sector at [where] - 1 = floor,
 * 2 = ceiling. If [fullbright] is true, light level is ignored
 *******************************************************************/
rgba_t MapSector::getColour(int where, bool fullbright)
{
	using Game::UDMFFeature;

	// Check for sector colour set in open script
	// TODO: Test if this is correct behaviour
	if (parent_map_->mapSpecials()->tagColoursSet())
	{
		rgba_t col;
		if (parent_map_->mapSpecials()->getTagColour(id_, &col))
		{
			if (fullbright)
				return col;

			// Get sector light level
			int ll = light_;

			// Clamp light level
			if (ll > 255)
				ll = 255;
			if (ll < 0)
				ll = 0;

			// Calculate and return the colour
			float lightmult = (float)ll / 255.0f;
			return col.ampf(lightmult, lightmult, lightmult, 1.0f);
		}
	}

	// Check for UDMF
	if (parent_map_->currentFormat() == MAP_UDMF &&
		(Game::configuration().featureSupported(UDMFFeature::SectorColor) ||
		Game::configuration().featureSupported(UDMFFeature::FlatLighting)))
	{
		// Get sector light colour
		wxColour wxcol;
		if(Game::configuration().featureSupported(UDMFFeature::SectorColor))
		{
			int intcol = MapObject::intProperty("lightcolor");
			wxcol = wxColour(intcol);
		}
		else
			wxcol = wxColour(255, 255, 255, 255);
		

		// Ignore light level if fullbright
		if (fullbright)
			return rgba_t(wxcol.Blue(), wxcol.Green(), wxcol.Red(), 255);

		// Get sector light level
		int ll = light_;

		if (Game::configuration().featureSupported(UDMFFeature::FlatLighting))
		{
			// Get specific light level
			if(where == 1)
			{
				// Floor
				int fl = MapObject::intProperty("lightfloor");
				if(boolProperty("lightfloorabsolute"))
					ll = fl;
				else
					ll += fl;
			}
			else if(where == 2)
			{
				// Ceiling
				int cl = MapObject::intProperty("lightceiling");
				if(boolProperty("lightceilingabsolute"))
					ll = cl;
				else
					ll += cl;
			}
		}

		// Clamp light level
		if (ll > 255)
			ll = 255;
		if (ll < 0)
			ll = 0;

		// Calculate and return the colour
		float lightmult = (float)ll / 255.0f;
		return rgba_t(wxcol.Blue() * lightmult, wxcol.Green() * lightmult, wxcol.Red() * lightmult, 255);
	}

	// Other format, simply return the light level
	if (fullbright)
		return rgba_t(255, 255, 255, 255);
	else
	{
		int l = light_;

		// Clamp light level
		if (l > 255)
			l = 255;
		if (l < 0)
			l = 0;

		return rgba_t(l, l, l, 255);
	}
}

/* MapSector::getColour
 * Returns the fog colour of the sector
 *******************************************************************/
rgba_t MapSector::getFogColour()
{
	rgba_t color(0, 0, 0, 0);

	// map specials/scripts
	if (parent_map_->mapSpecials()->tagFadeColoursSet())
	{
		if (parent_map_->mapSpecials()->getTagFadeColour(id_, &color))
			return color;
	}

	// udmf
	if (parent_map_->currentFormat() == MAP_UDMF &&
		Game::configuration().featureSupported(Game::UDMFFeature::SectorFog))
	{
		int intcol = MapObject::intProperty("fadecolor");

		wxColour wxcol(intcol);
		color = rgba_t(wxcol.Blue(), wxcol.Green(), wxcol.Red(), 0);
	}
	return color;
}

/* MapSector::connectSide
 * Adds [side] to the list of 'connected sides' (sides that are part
 * of this sector)
 *******************************************************************/
void MapSector::connectSide(MapSide* side)
{
	setModified();
	connected_sides_.push_back(side);
	poly_needsupdate_ = true;
	bbox_.reset();
	setGeometryUpdated();
}

/* MapSector::disconnectSide
 * Removes [side] from the list of connected sides
 *******************************************************************/
void MapSector::disconnectSide(MapSide* side)
{
	setModified();
	for (unsigned a = 0; a < connected_sides_.size(); a++)
	{
		if (connected_sides_[a] == side)
		{
			connected_sides_.erase(connected_sides_.begin() + a);
			break;
		}
	}

	poly_needsupdate_ = true;
	bbox_.reset();
	setGeometryUpdated();
}

/* MapSector::writeBackup
 * Write all sector info to a mobj_backup_t struct
 *******************************************************************/
void MapSector::writeBackup(Backup* backup)
{
	backup->props_internal["texturefloor"] = tex_floor_;
	backup->props_internal["textureceiling"] = tex_ceiling_;
	backup->props_internal["heightfloor"] = height_floor_;
	backup->props_internal["heightceiling"] = height_ceiling_;
	backup->props_internal["lightlevel"] = light_;
	backup->props_internal["special"] = special_;
	backup->props_internal["id"] = id_;
}

/* MapSector::readBackup
 * Reads all sector info from a mobj_backup_t struct
 *******************************************************************/
void MapSector::readBackup(Backup* backup)
{
	// Update texture counts (decrement previous)
	parent_map_->updateFlatUsage(tex_floor_, -1);
	parent_map_->updateFlatUsage(tex_ceiling_, -1);

	tex_floor_ = backup->props_internal["texturefloor"].getStringValue();
	tex_ceiling_ = backup->props_internal["textureceiling"].getStringValue();
	height_floor_ = backup->props_internal["heightfloor"].getIntValue();
	height_ceiling_ = backup->props_internal["heightceiling"].getIntValue();
	plane_floor_.set(0, 0, 1, height_floor_);
	plane_ceiling_.set(0, 0, 1, height_ceiling_);
	light_ = backup->props_internal["lightlevel"].getIntValue();
	special_ = backup->props_internal["special"].getIntValue();
	id_ = backup->props_internal["id"].getIntValue();

	// Update texture counts (increment new)
	parent_map_->updateFlatUsage(tex_floor_, 1);
	parent_map_->updateFlatUsage(tex_ceiling_, 1);

	// Update geometry info
	poly_needsupdate_ = true;
	bbox_.reset();
	setGeometryUpdated();
}
