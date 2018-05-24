
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapSpecials.cpp
// Description: Various functions for processing map specials andit scripts,
//              mostly for visual effects (transparency, colours, slopes, etc.)
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
#include "Edit/Input.h"
#include "Game/Configuration.h"
#include "MapSpecials.h"
#include "SLADEMap/SLADEMap.h"
#include "Utility/MathStuff.h"
#include "Utility/Tokenizer.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace
{
const double TAU = PI * 2; // Number of radians in the unit circle
}


// -----------------------------------------------------------------------------
//
// Functions
//
// -----------------------------------------------------------------------------
using SurfaceType = MapSector::SurfaceType;

namespace
{
// -----------------------------------------------------------------------------
// Modify sector with [tag]
// -----------------------------------------------------------------------------
void setModified(SLADEMap* map, int tag)
{
	vector<MapSector*> tagged;
	map->sectorsByTag(tag, tagged);
	for (auto& sector : tagged)
		sector->setModified();
}

// -----------------------------------------------------------------------------
// Returns the floor/ceiling height of [vertex] in [sector]
// -----------------------------------------------------------------------------
template<SurfaceType p> double vertexHeight(MapVertex* vertex, MapSector* sector)
{
	// Return vertex height if set via UDMF property
	string prop = (p == SurfaceType::Floor ? "zfloor" : "zceiling");
	if (vertex->hasProp(prop))
		return vertex->floatProperty(prop);

	// Otherwise just return sector height
	return sector->planeHeight<p>();
}
} // namespace


// -----------------------------------------------------------------------------
//
// MapSpecials Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Clear out all internal state
// -----------------------------------------------------------------------------
void MapSpecials::reset()
{
	sector_colours_.clear();
	sector_fadecolours_.clear();
}

// -----------------------------------------------------------------------------
// Process map specials, depending on the current game/port
// -----------------------------------------------------------------------------
void MapSpecials::processMapSpecials(SLADEMap* map) const
{
	// ZDoom
	if (Game::configuration().currentPort() == "zdoom")
		processZDoomMapSpecials(map);
	// Eternity, currently no need for processEternityMapSpecials
	else if (Game::configuration().currentPort() == "eternity")
		processEternitySlopes(map);
}

// -----------------------------------------------------------------------------
// Process a line's special, depending on the current game/port
// -----------------------------------------------------------------------------
void MapSpecials::processLineSpecial(MapLine* line) const
{
	if (Game::configuration().currentPort() == "zdoom")
		processZDoomLineSpecial(line);
}

// -----------------------------------------------------------------------------
// Sets [colour] to the parsed colour for [tag].
// Returns true if the tag has a colour, false otherwise
// -----------------------------------------------------------------------------
bool MapSpecials::getTagColour(int tag, ColRGBA* colour)
{
	// scripts
	for (auto& sc : sector_colours_)
	{
		if (sc.tag == tag)
		{
			colour->r = sc.colour.r;
			colour->g = sc.colour.g;
			colour->b = sc.colour.b;
			colour->a = 255;
			return true;
		}
	}

	return false;
}

// -----------------------------------------------------------------------------
// Sets [colour] to the parsed fade colour for [tag].
// Returns true if the tag has a colour, false otherwise
// -----------------------------------------------------------------------------
bool MapSpecials::getTagFadeColour(int tag, ColRGBA* colour)
{
	// scripts
	for (auto& sc : sector_fadecolours_)
	{
		if (sc.tag == tag)
		{
			colour->r = sc.colour.r;
			colour->g = sc.colour.g;
			colour->b = sc.colour.b;
			colour->a = 0;
			return true;
		}
	}

	return false;
}

// -----------------------------------------------------------------------------
// Returns true if any sector tags should be coloured
// -----------------------------------------------------------------------------
bool MapSpecials::tagColoursSet() const
{
	return !(sector_colours_.empty());
}

// -----------------------------------------------------------------------------
// Returns true if any sector tags should be coloured by fog
// -----------------------------------------------------------------------------
bool MapSpecials::tagFadeColoursSet() const
{
	return !(sector_fadecolours_.empty());
}

// -----------------------------------------------------------------------------
// Updates any sectors with tags that are affected by any processed specials /
// scripts
// -----------------------------------------------------------------------------
void MapSpecials::updateTaggedSectors(SLADEMap* map)
{
	// scripts
	for (auto& sc : sector_colours_)
		setModified(map, sc.tag);

	for (auto& sc : sector_fadecolours_)
		setModified(map, sc.tag);
}

// -----------------------------------------------------------------------------
// Process ZDoom map specials, mostly to convert hexen specials to UDMF
// counterparts
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomMapSpecials(SLADEMap* map) const
{
	// Line specials
	for (auto line : map->lines())
		processZDoomLineSpecial(line);

	// All slope specials, which must be done in a particular order
	processZDoomSlopes(map);
}

// -----------------------------------------------------------------------------
// Process ZDoom line special
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomLineSpecial(MapLine* line) const
{
	// Get special
	int special = line->special();
	if (special == 0)
		return;

	// Get parent map
	SLADEMap* map = line->parentMap();

	// Get args
	int args[5];
	for (unsigned arg = 0; arg < 5; arg++)
		args[arg] = line->intProperty(fmt::sprintf("arg%d", arg));

	// --- TranslucentLine ---
	if (special == 208)
	{
		// Get tagged lines
		vector<MapLine*> tagged;
		if (args[0] > 0)
			map->linesById(args[0], tagged);
		else
			tagged.push_back(line);

		// Get args
		double alpha = (double)args[1] / 255.0;
		string type  = (args[2] == 0) ? "translucent" : "add";

		// Set transparency
		for (auto& tagline : tagged)
		{
			tagline->setFloatProperty("alpha", alpha);
			tagline->setStringProperty("renderstyle", type);

			Log::info(3, fmt::sprintf("Line %d translucent: (%d) %1.2f, %s", tagline->index(), args[1], alpha, type));
		}
	}
}

// -----------------------------------------------------------------------------
// Process 'OPEN' ACS scripts for various specials - sector colours, slopes, etc
// -----------------------------------------------------------------------------
void MapSpecials::processACSScripts(ArchiveEntry* entry)
{
	sector_colours_.clear();
	sector_fadecolours_.clear();

	if (!entry || entry->size() == 0)
		return;

	Tokenizer tz;
	tz.setSpecialCharacters(";,:|={}/()");
	tz.openMem(entry->data(), "ACS Scripts");

	while (!tz.atEnd())
	{
		if (tz.checkNC("script"))
		{
			Log::info(3, "script found");

			tz.adv(2); // Skip script #

			// Check for open script
			if (tz.checkNC("OPEN"))
			{
				Log::info(3, "script is OPEN");

				// Skip to opening brace
				while (!tz.check("{"))
					tz.adv();

				// Parse script
				while (!tz.checkOrEnd("}"))
				{
					// --- Sector_SetColor ---
					if (tz.checkNC("Sector_SetColor"))
					{
						// Get parameters
						auto parameters = tz.getTokensUntil(")");

						// Parse parameters
						int tag = -1;
						int r   = -1;
						int g   = -1;
						int b   = -1;
						for (auto& parameter : parameters)
						{
							if (parameter.isInteger())
							{
								if (tag < 0)
									tag = parameter.asInt();
								else if (r < 0)
									r = parameter.asInt();
								else if (g < 0)
									g = parameter.asInt();
								else if (b < 0)
									b = parameter.asInt();
							}
						}

						// Check everything is set
						if (b < 0)
						{
							Log::warning(2, "Invalid Sector_SetColor parameters");
						}
						else
						{
							SectorColour sc;
							sc.tag = tag;
							sc.colour.set(r, g, b, 255);
							Log::info(3, fmt::sprintf("Sector tag %d, colour %d,%d,%d", tag, r, g, b));
							sector_colours_.push_back(sc);
						}
					}
					// --- Sector_SetFade ---
					else if (tz.checkNC("Sector_SetFade"))
					{
						// Get parameters
						auto parameters = tz.getTokensUntil(")");

						// Parse parameters
						int tag = -1;
						int r   = -1;
						int g   = -1;
						int b   = -1;
						for (auto& parameter : parameters)
						{
							if (parameter.isInteger())
							{
								if (tag < 0)
									tag = parameter.asInt();
								else if (r < 0)
									r = parameter.asInt();
								else if (g < 0)
									g = parameter.asInt();
								else if (b < 0)
									b = parameter.asInt();
							}
						}

						// Check everything is set
						if (b < 0)
						{
							Log::warning(2, "Invalid Sector_SetFade parameters");
						}
						else
						{
							SectorColour sc;
							sc.tag = tag;
							sc.colour.set(r, g, b, 0);
							Log::info(3, fmt::sprintf("Sector tag %d, fade colour %d,%d,%d", tag, r, g, b));
							sector_fadecolours_.push_back(sc);
						}
					}

					tz.adv();
				}
			}
		}

		tz.adv();
	}
}

// -----------------------------------------------------------------------------
// Process ZDoom slope specials
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomSlopes(SLADEMap* map) const
{
	// ZDoom has a variety of slope mechanisms, which must be evaluated in a
	// specific order.
	//  - Plane_Align, in line order
	//  - line slope + sector tilt + vavoom, in thing order
	//  - slope copy things, in thing order
	//  - overwrite vertex heights with vertex height things
	//  - vertex triangle slopes, in sector order
	//  - Plane_Copy, in line order

	// First things first: reset every sector to flat planes
	for (unsigned a = 0; a < map->nSectors(); a++)
	{
		MapSector* target = map->sector(a);
		target->setFloorPlane(Plane::flat(target->floor().height));
		target->setCeilingPlane(Plane::flat(target->ceiling().height));
	}

	// Plane_Align (line special 181)
	for (auto line : map->lines())
	{
		if (line->special() != 181)
			continue;

		MapSector* sector1 = line->frontSector();
		MapSector* sector2 = line->backSector();
		if (!sector1 || !sector2)
		{
			Log::warning(fmt::sprintf("Ignoring Plane_Align on one-sided line %d", line->index()));
			continue;
		}
		if (sector1 == sector2)
		{
			Log::warning(fmt::sprintf(
				"Ignoring Plane_Align on line %d, which has the same sector on both sides", line->index()));
			continue;
		}

		int floor_arg = line->intProperty("arg0");
		if (floor_arg == 1)
			applyPlaneAlign<SurfaceType::Floor>(line, sector1, sector2);
		else if (floor_arg == 2)
			applyPlaneAlign<SurfaceType::Floor>(line, sector2, sector1);

		int ceiling_arg = line->intProperty("arg1");
		if (ceiling_arg == 1)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector1, sector2);
		else if (ceiling_arg == 2)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector2, sector1);
	}

	// Line slope things (9500/9501), sector tilt things (9502/9503), and
	// vavoom things (1500/1501), all in the same pass
	for (auto thing : map->things())
	{
		// Line slope things
		if (thing->type() == 9500)
			applyLineSlopeThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 9501)
			applyLineSlopeThing<SurfaceType::Ceiling>(map, thing);
		// Sector tilt things
		else if (thing->type() == 9502)
			applySectorTiltThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 9503)
			applySectorTiltThing<SurfaceType::Ceiling>(map, thing);
		// Vavoom things
		else if (thing->type() == 1500)
			applyVavoomSlopeThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 1501)
			applyVavoomSlopeThing<SurfaceType::Ceiling>(map, thing);
	}

	// Slope copy things (9510/9511)
	for (auto thing : map->things())
	{
		if (thing->type() == 9510 || thing->type() == 9511)
		{
			int target_idx = map->sectorAt(thing->position());
			if (target_idx < 0)
				continue;
			MapSector* target = map->sector(target_idx);

			// First argument is the tag of a sector whose slope should be copied
			int tag = thing->intProperty("arg0");
			if (!tag)
			{
				Log::warning(fmt::sprintf("Ignoring slope copy thing in sector %d with no argument", target_idx));
				continue;
			}

			vector<MapSector*> tagged_sectors;
			map->sectorsByTag(tag, tagged_sectors);
			if (tagged_sectors.empty())
			{
				Log::warning(fmt::sprintf(
					"Ignoring slope copy thing in sector %d; no sectors have target tag %d", target_idx, tag));
				continue;
			}

			if (thing->type() == 9510)
				target->setFloorPlane(tagged_sectors[0]->floor().plane);
			else
				target->setCeilingPlane(tagged_sectors[0]->ceiling().plane);
		}
	}

	// Vertex height things
	// These only affect the calculation of slopes and shouldn't be stored in
	// the map data proper, so instead of actually changing vertex properties,
	// we store them in a hashmap.
	VertexHeightMap vertex_floor_heights;
	VertexHeightMap vertex_ceiling_heights;
	for (auto thing : map->things())
	{
		if (thing->type() == 1504 || thing->type() == 1505)
		{
			// TODO there could be more than one vertex at this point
			MapVertex* vertex = map->vertexAt(thing->xPos(), thing->yPos());
			if (vertex)
			{
				if (thing->type() == 1504)
					vertex_floor_heights[vertex] = thing->floatProperty("height");
				else if (thing->type() == 1505)
					vertex_ceiling_heights[vertex] = thing->floatProperty("height");
			}
		}
	}

	// Vertex heights -- only applies for sectors with exactly three vertices.
	// Heights may be set by UDMF properties, or by a vertex height thing
	// placed exactly on the vertex (which takes priority over the prop).
	vector<MapVertex*> vertices;
	for (auto target : map->sectors())
	{
		vertices.clear();
		target->getVertices(vertices);
		if (vertices.size() != 3)
			continue;

		applyVertexHeightSlope<SurfaceType::Floor>(target, vertices, vertex_floor_heights);
		applyVertexHeightSlope<SurfaceType::Ceiling>(target, vertices, vertex_ceiling_heights);
	}

	// Plane_Copy
	vector<MapSector*> sectors;
	for (auto line : map->lines())
	{
		if (line->special() != 118)
			continue;

		int        tag;
		MapSector* front = line->frontSector();
		MapSector* back  = line->backSector();
		if (((tag = line->intProperty("arg0"))) && front)
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				front->setFloorPlane(sectors[0]->floor().plane);
		}
		if (((tag = line->intProperty("arg1"))) && front)
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				front->setCeilingPlane(sectors[0]->ceiling().plane);
		}
		if (((tag = line->intProperty("arg2"))) && back)
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				back->setFloorPlane(sectors[0]->floor().plane);
		}
		if (((tag = line->intProperty("arg3"))) && back)
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				back->setCeilingPlane(sectors[0]->ceiling().plane);
		}

		// The fifth "share" argument copies from one side of the line to the
		// other
		if (front && back)
		{
			int share = line->intProperty("arg4");

			if ((share & 3) == 1)
				back->setFloorPlane(front->floor().plane);
			else if ((share & 3) == 2)
				front->setFloorPlane(back->floor().plane);

			if ((share & 12) == 4)
				back->setCeilingPlane(front->ceiling().plane);
			else if ((share & 12) == 8)
				front->setCeilingPlane(back->ceiling().plane);
		}
	}
}

// -----------------------------------------------------------------------------
// Process Eternity slope specials
// -----------------------------------------------------------------------------
void MapSpecials::processEternitySlopes(SLADEMap* map) const
{
	// Eternity plans on having a few slope mechanisms,
	// which must be evaluated in a specific order.
	//  - Plane_Align, in line order
	//  - vertex triangle slopes, in sector order (wip)
	//  - Plane_Copy, in line order

	// First things first: reset every sector to flat planes
	for (unsigned a = 0; a < map->nSectors(); a++)
	{
		MapSector* target = map->sector(a);
		target->setFloorPlane(Plane::flat(target->floor().height));
		target->setCeilingPlane(Plane::flat(target->ceiling().height));
	}

	// Plane_Align (line special 181)
	for (auto line : map->lines())
	{
		if (line->special() != 181)
			continue;

		MapSector* sector1 = line->frontSector();
		MapSector* sector2 = line->backSector();
		if (!sector1 || !sector2)
		{
			Log::warning(fmt::sprintf("Ignoring Plane_Align on one-sided line %d", line->index()));
			continue;
		}
		if (sector1 == sector2)
		{
			Log::warning(fmt::sprintf(
				"Ignoring Plane_Align on line %d, which has the same sector on both sides", line->index()));
			continue;
		}

		int floor_arg = line->intProperty("arg0");
		if (floor_arg == 1)
			applyPlaneAlign<SurfaceType::Floor>(line, sector1, sector2);
		else if (floor_arg == 2)
			applyPlaneAlign<SurfaceType::Floor>(line, sector2, sector1);

		int ceiling_arg = line->intProperty("arg1");
		if (ceiling_arg == 1)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector1, sector2);
		else if (ceiling_arg == 2)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector2, sector1);
	}

	// Plane_Copy
	vector<MapSector*> sectors;
	for (auto line : map->lines())
	{
		if (line->special() != 118)
			continue;

		int        tag;
		MapSector* front = line->frontSector();
		MapSector* back  = line->backSector();
		if ((tag = line->intProperty("arg0")))
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				front->setFloorPlane(sectors[0]->floor().plane);
		}
		if ((tag = line->intProperty("arg1")))
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				front->setCeilingPlane(sectors[0]->ceiling().plane);
		}
		if ((tag = line->intProperty("arg2")))
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				back->setFloorPlane(sectors[0]->floor().plane);
		}
		if ((tag = line->intProperty("arg3")))
		{
			sectors.clear();
			map->sectorsByTag(tag, sectors);
			if (!sectors.empty())
				back->setCeilingPlane(sectors[0]->ceiling().plane);
		}

		// The fifth "share" argument copies from one side of the line to the
		// other
		if (front && back)
		{
			int share = line->intProperty("arg4");

			if ((share & 3) == 1)
				back->setFloorPlane(front->floor().plane);
			else if ((share & 3) == 2)
				front->setFloorPlane(back->floor().plane);

			if ((share & 12) == 4)
				back->setCeilingPlane(front->ceiling().plane);
			else if ((share & 12) == 8)
				front->setCeilingPlane(back->ceiling().plane);
		}
	}
}


// -----------------------------------------------------------------------------
// Applies a Plane_Align special on [line], to [target] from [model]
// -----------------------------------------------------------------------------
template<SurfaceType p> void MapSpecials::applyPlaneAlign(MapLine* line, MapSector* target, MapSector* model) const
{
	vector<MapVertex*> vertices;
	target->getVertices(vertices);

	// The slope is between the line with Plane_Align, and the point in the
	// sector furthest away from it, which can only be at a vertex
	double     this_dist;
	double     furthest_dist   = 0.0;
	MapVertex* furthest_vertex = nullptr;
	for (auto& vertex : vertices)
	{
		this_dist = line->distanceTo(vertex->position());
		if (this_dist > furthest_dist)
		{
			furthest_dist   = this_dist;
			furthest_vertex = vertex;
		}
	}

	if (!furthest_vertex || furthest_dist < 0.01)
	{
		Log::warning(
			fmt::sprintf(
			"Ignoring Plane_Align on line %d; sector %d has no appropriate reference vertex",
			line->index(),
			target->index()));
		return;
	}

	// Calculate slope plane from our three points: this line's endpoints
	// (at the model sector's height) and the found vertex (at this sector's height).
	double    modelz  = model->planeHeight<p>();
	double    targetz = target->planeHeight<p>();
	fpoint3_t p1(line->x1(), line->y1(), modelz);
	fpoint3_t p2(line->x2(), line->y2(), modelz);
	fpoint3_t p3(furthest_vertex->position(), targetz);
	target->setPlane<p>(MathStuff::planeFromTriangle(p1, p2, p3));
}

// -----------------------------------------------------------------------------
// Applies a line slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType p> void MapSpecials::applyLineSlopeThing(SLADEMap* map, MapThing* thing) const
{
	int lineid = thing->intProperty("arg0");
	if (!lineid)
	{
		Log::warning(fmt::sprintf("Ignoring line slope thing %d with no lineid argument", thing->index()));
		return;
	}

	// These are computed on first use, to avoid extra work if no lines match
	MapSector* containing_sector = nullptr;
	double     thingz            = 0;

	vector<MapLine*> lines;
	map->linesById(lineid, lines);
	for (auto& line : lines)
	{
		// Line slope things only affect the sector on the side of the line
		// that faces the thing
		double     side   = MathStuff::lineSide(thing->position(), line->seg());
		MapSector* target = nullptr;
		if (side < 0)
			target = line->backSector();
		else if (side > 0)
			target = line->frontSector();
		if (!target)
			continue;

		// Need to know the containing sector's height to find the thing's true height
		if (!containing_sector)
		{
			int containing_sector_idx = map->sectorAt(thing->position());
			if (containing_sector_idx < 0)
				return;
			containing_sector = map->sector(containing_sector_idx);
			thingz = (containing_sector->plane<p>().heightAt(thing->position()) + thing->floatProperty("height"));
		}

		// Three points: endpoints of the line, and the thing itself
		Plane     target_plane = target->plane<p>();
		fpoint3_t p1(line->x1(), line->y1(), target_plane.heightAt(line->point1()));
		fpoint3_t p2(line->x2(), line->y2(), target_plane.heightAt(line->point2()));
		fpoint3_t p3(thing->xPos(), thing->yPos(), thingz);
		target->setPlane<p>(MathStuff::planeFromTriangle(p1, p2, p3));
	}
}

// -----------------------------------------------------------------------------
// Applies a tilt slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType p> void MapSpecials::applySectorTiltThing(SLADEMap* map, MapThing* thing) const
{
	// TODO should this apply to /all/ sectors at this point, in the case of an intersection?
	int target_idx = map->sectorAt(thing->position());
	if (target_idx < 0)
		return;
	MapSector* target = map->sector(target_idx);

	// First argument is the tilt angle, but starting with 0 as straight down;
	// subtracting 90 fixes that.
	int raw_angle = thing->intProperty("arg0");
	if (raw_angle == 0 || raw_angle == 180)
		// Exact vertical tilt is nonsense
		return;

	double angle = thing->angle() / 360.0 * TAU;
	double tilt  = (raw_angle - 90) / 360.0 * TAU;
	// Resulting plane goes through the position of the thing
	double    z = target->planeHeight<p>() + thing->floatProperty("height");
	fpoint3_t point(thing->xPos(), thing->yPos(), z);

	double cos_angle = cos(angle);
	double sin_angle = sin(angle);
	double cos_tilt  = cos(tilt);
	double sin_tilt  = sin(tilt);
	// Need to convert these angles into vectors on the plane, so we can take a
	// normal.
	// For the first: we know that the line perpendicular to the direction the
	// thing faces lies "flat", because this is the axis the tilt thing rotates
	// around.  "Rotate" the angle a quarter turn to get this vector -- switch
	// x and y, and negate one.
	fpoint3_t vec1(-sin_angle, cos_angle, 0.0);

	// For the second: the tilt angle makes a triangle between the floor plane
	// and the z axis.  sin gives us the distance along the z-axis, but cos
	// only gives us the distance away /from/ the z-axis.  Break that into x
	// and y by multiplying by cos and sin of the thing's facing angle.
	fpoint3_t vec2(cos_tilt * cos_angle, cos_tilt * sin_angle, sin_tilt);

	target->setPlane<p>(MathStuff::planeFromTriangle(point, point + vec1, point + vec2));
}

// -----------------------------------------------------------------------------
// Applies a vavoom slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType p> void MapSpecials::applyVavoomSlopeThing(SLADEMap* map, MapThing* thing) const
{
	int target_idx = map->sectorAt(thing->position());
	if (target_idx < 0)
		return;
	MapSector* target = map->sector(target_idx);

	int              tid = thing->intProperty("id");
	vector<MapLine*> lines;
	target->getLines(lines);

	// TODO unclear if this is the same order that ZDoom would go through the
	// lines, which matters if two lines have the same first arg
	for (unsigned a = 0; a < lines.size(); a++)
	{
		if (tid != lines[a]->intProperty("arg0"))
			continue;

		// Vavoom things use the plane defined by the thing and its two
		// endpoints, based on the sector's original (flat) plane and treating
		// the thing's height as absolute
		if (MathStuff::distanceToLineFast(thing->position(), lines[a]->seg()) == 0)
		{
			Log::warning(fmt::sprintf("Vavoom thing %d lies directly on its target line %d", thing->index(), a));
			return;
		}

		short     height = target->planeHeight<p>();
		fpoint3_t p1(thing->xPos(), thing->yPos(), thing->floatProperty("height"));
		fpoint3_t p2(lines[a]->x1(), lines[a]->y1(), height);
		fpoint3_t p3(lines[a]->x2(), lines[a]->y2(), height);

		target->setPlane<p>(MathStuff::planeFromTriangle(p1, p2, p3));
		return;
	}

	Log::warning(fmt::sprintf("Vavoom thing %d has no matching line with first arg %d", thing->index(), tid));
}

// -----------------------------------------------------------------------------
// Applies a slope to sector [target] based on the heights of its vertices
// (triangular sectors only)
// -----------------------------------------------------------------------------
template<SurfaceType p>
void MapSpecials::applyVertexHeightSlope(MapSector* target, vector<MapVertex*>& vertices, VertexHeightMap& heights)
	const
{
	double z1 = heights.count(vertices[0]) ? heights[vertices[0]] : vertexHeight<p>(vertices[0], target);
	double z2 = heights.count(vertices[1]) ? heights[vertices[1]] : vertexHeight<p>(vertices[1], target);
	double z3 = heights.count(vertices[2]) ? heights[vertices[2]] : vertexHeight<p>(vertices[2], target);

	fpoint3_t p1(vertices[0]->xPos(), vertices[0]->yPos(), z1);
	fpoint3_t p2(vertices[1]->xPos(), vertices[1]->yPos(), z2);
	fpoint3_t p3(vertices[2]->xPos(), vertices[2]->yPos(), z3);
	target->setPlane<p>(MathStuff::planeFromTriangle(p1, p2, p3));
}
