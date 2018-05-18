
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2017 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapRenderer2D.cpp
// Description: MapRenderer2D class - handles all rendering related stuff for
//              the map in 2d
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
#include "MapRenderer2D.h"
#include "App.h"
#include "Game/Configuration.h"
#include "General/ColourConfiguration.h"
#include "MapEditor/Edit/ObjectEdit.h"
#include "MapEditor/MapEditContext.h"
#include "MapEditor/MapEditor.h"
#include "MapEditor/MapTextureManager.h"
#include "MapEditor/SLADEMap/SLADEMap.h"
#include "OpenGL/Drawing.h"
#include "OpenGL/GLTexture.h"
#include "OpenGL/OpenGL.h"
#include "Utility/Polygon2D.h"


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
CVAR(Bool, vertex_round, true, CVAR_SAVE)
CVAR(Int, vertex_size, 7, CVAR_SAVE)
CVAR(Float, line_width, 1.5f, CVAR_SAVE)
CVAR(Bool, line_smooth, true, CVAR_SAVE)
CVAR(Int, thing_drawtype, 1, CVAR_SAVE)
CVAR(Bool, thing_force_dir, false, CVAR_SAVE)
CVAR(Bool, thing_overlay_square, false, CVAR_SAVE)
CVAR(Float, flat_brightness, 0.8f, CVAR_SAVE)
CVAR(Bool, flat_ignore_light, false, CVAR_SAVE)
CVAR(Float, thing_shadow, 0.5f, CVAR_SAVE)
CVAR(Bool, sector_hilight_fill, true, CVAR_SAVE)
CVAR(Bool, sector_selected_fill, true, CVAR_SAVE)
CVAR(Bool, map_animate_hilight, true, CVAR_SAVE)
CVAR(Bool, map_animate_selection, false, CVAR_SAVE)
CVAR(Bool, map_animate_tagged, true, CVAR_SAVE)
CVAR(Float, arrow_alpha, 1.0f, CVAR_SAVE)
CVAR(Bool, arrow_colour, false, CVAR_SAVE)
CVAR(Bool, flats_use_vbo, true, CVAR_SAVE)
CVAR(Int, halo_width, 5, CVAR_SAVE)
CVAR(Float, arrowhead_angle, 0.7854f, CVAR_SAVE)
CVAR(Float, arrowhead_length, 25.f, CVAR_SAVE)
CVAR(Bool, action_lines, true, CVAR_SAVE)
CVAR(String, arrow_pathed_color, "#22FFFF", CVAR_SAVE)
CVAR(String, arrow_dragon_color, "#FF2222", CVAR_SAVE)
CVAR(Bool, test_ssplit, false, CVAR_SAVE)

namespace
{
// Texture coordinates for rendering square things (since we can't just rotate these)
float sq_thing_tc[] = { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
} // namespace


// -----------------------------------------------------------------------------
//
// External Variables
//
// -----------------------------------------------------------------------------
EXTERN_CVAR(Bool, use_zeth_icons)


// -----------------------------------------------------------------------------
//
// MapRenderer2D Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// MapRenderer2D class destructor
// -----------------------------------------------------------------------------
MapRenderer2D::~MapRenderer2D()
{
	if (vbo_vertices_ > 0)
		glDeleteBuffers(1, &vbo_vertices_);
	if (vbo_lines_ > 0)
		glDeleteBuffers(1, &vbo_lines_);
	if (vbo_flats_ > 0)
		glDeleteBuffers(1, &vbo_flats_);
	if (list_vertices_ > 0)
		glDeleteLists(list_vertices_, 1);
	if (list_lines_ > 0)
		glDeleteLists(list_lines_, 1);
}

// -----------------------------------------------------------------------------
// Sets up the renderer for vertices (point sprites, etc.).
// If [overlay] is true, use the point sprite for hilight/selection/etc
// -----------------------------------------------------------------------------
bool MapRenderer2D::setupVertexRendering(float size_scale, bool overlay) const
{
	// Setup rendering properties
	float vs = vertex_size * size_scale;
	if (view_scale_ < 1.0)
		vs *= view_scale_;
	if (vs < 2.0)
		vs = 2.0;
	glPointSize(vs);

	// Setup point sprites if supported
	bool point = false;
	if (OpenGL::pointSpriteSupport())
	{
		// Get appropriate vertex texture
		GLTexture* tex;
		if (overlay)
		{
			if (vertex_round)
				tex = MapEditor::textureManager().editorImage("vertex/hilight_r");
			else
				tex = MapEditor::textureManager().editorImage("vertex/hilight_s");
		}
		else
		{
			if (vertex_round)
				tex = MapEditor::textureManager().editorImage("vertex/round");
			else
				tex = MapEditor::textureManager().editorImage("vertex/square");
		}

		// If it was found, enable point sprites
		if (tex)
		{
			glEnable(GL_TEXTURE_2D);
			tex->bind();
			glEnable(GL_POINT_SPRITE);
			glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
			point = true;
		}
	}

	// No point sprites, use regular points
	if (!point)
	{
		if (vertex_round)
			glEnable(GL_POINT_SMOOTH);
		else
			glDisable(GL_POINT_SMOOTH);
	}

	return point;
}

// -----------------------------------------------------------------------------
// Renders map vertices
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertices(float alpha)
{
	// Check there are any vertices to render
	if (map_->nVertices() == 0)
		return;

	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	// Setup rendering properties
	bool point = setupVertexRendering(1.0f);

	// Set to vertex colour
	ColRGBA col = ColourConfiguration::colour("map_vertex");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * alpha);

	// Render the vertices depending on what features are supported
	if (OpenGL::vboSupport())
		renderVerticesVBO();
	else
		renderVerticesImmediate();

	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Renders vertices in immediate mode (slower, but required for old video cards)
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVerticesImmediate()
{
	if (list_vertices_ > 0 && map_->nVertices() == n_vertices_ && map_->geometryUpdated() <= vertices_updated_)
		glCallList(list_vertices_);
	else
	{
		list_vertices_ = glGenLists(1);
		glNewList(list_vertices_, GL_COMPILE_AND_EXECUTE);

		// Draw all vertices
		glBegin(GL_POINTS);
		for (unsigned a = 0; a < map_->nVertices(); a++)
			glVertex2d(map_->vertex(a)->xPos(), map_->vertex(a)->yPos());
		glEnd();

		glEndList();

		vertices_updated_ = App::runTimer();
	}
}

// -----------------------------------------------------------------------------
// Renders vertices using an OpenGL Vertex Buffer Object
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVerticesVBO()
{
	// Do nothing if there are no vertices in the map
	if (map_->nVertices() == 0)
		return;

	// Update vertices VBO if required
	if (vbo_vertices_ == 0 || map_->nVertices() != n_vertices_ || map_->geometryUpdated() > vertices_updated_)
		updateVerticesVBO();

	// Set VBO arrays to use
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	// Setup VBO pointers
	glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices_);
	glVertexPointer(2, GL_FLOAT, 0, nullptr);

	// Render the VBO
	glDrawArrays(GL_POINTS, 0, map_->nVertices());

	// Cleanup state
	glDisableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// -----------------------------------------------------------------------------
// Renders the vertex hilight overlay for vertex [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertexHilight(int index, float fade) const
{
	// Check hilight
	if (!map_->vertex(index))
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_hilight");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup rendering properties
	bool point = setupVertexRendering(1.8f + (0.6f * fade), true);

	// Draw vertex
	glBegin(GL_POINTS);
	glVertex2d(map_->vertex(index)->xPos(), map_->vertex(index)->yPos());
	glEnd();

	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Renders the vertex selection overlay for vertex indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderVertexSelection(const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Set selection colour
	ColRGBA col = ColourConfiguration::colour("map_selection");
	col.a      = 255;
	OpenGL::setColour(col);

	// Setup rendering properties
	bool point = setupVertexRendering(1.8f, true);

	// Draw selected vertices
	glBegin(GL_POINTS);
	for (auto item : selection)
		glVertex2d(map_->vertex(item.index)->xPos(), map_->vertex(item.index)->yPos());
	glEnd();

	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Returns the colour for [line]
// -----------------------------------------------------------------------------
ColRGBA MapRenderer2D::lineColour(MapLine* line, bool ignore_filter) const
{
	ColRGBA col;

	if (line)
	{
		// Check for special line
		if (line->intProperty("special") > 0)
			col.set(ColourConfiguration::colour("map_line_special"));
		else if (line->s1())
			col.set(ColourConfiguration::colour("map_line_normal"));
		else
			col.set(ColourConfiguration::colour("map_line_invalid"));

		// Check for two-sided line
		if (line->s2())
			col.a *= 0.5f;

		// Check if filtered
		if (line->isFiltered() && !ignore_filter)
			col.a *= 0.25f;
	}

	return col;
}

// -----------------------------------------------------------------------------
// Renders map lines, with direction tabs if [show_direction] is true
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLines(bool show_direction, float alpha)
{
	// Check there are any lines to render
	if (map_->nLines() == 0)
		return;

	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	// Setup rendering properties
	glLineWidth(line_width);
	if (line_smooth)
		glEnable(GL_LINE_SMOOTH);
	else
		glDisable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Render the lines depending on what features are supported
	if (OpenGL::vboSupport())
		renderLinesVBO(show_direction, alpha);
	else
		renderLinesImmediate(show_direction, alpha);
}

// -----------------------------------------------------------------------------
// Renders map lines in immediate mode
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLinesImmediate(bool show_direction, float alpha)
{
	// Use display list if it's built
	if (list_lines_ > 0 && show_direction == lines_dirs_ && map_->nLines() == n_lines_
		&& map_->geometryUpdated() <= lines_updated_ && !map_->modifiedSince(lines_updated_, MapObject::Type::Line))
	{
		glCallList(list_lines_);
		return;
	}

	// Rebuild display list if we're switching from simple lines to directional lines
	if (list_lines_ > 0)
		glDeleteLists(list_lines_, 1);

	// Create display list
	list_lines_ = glGenLists(1);
	glNewList(list_lines_, GL_COMPILE_AND_EXECUTE);

	// Draw all lines
	ColRGBA   col;
	MapLine* line;
	double   x1, y1, x2, y2;
	glBegin(GL_LINES);
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		// Get line info
		line = map_->line(a);
		x1   = line->v1()->xPos();
		y1   = line->v1()->yPos();
		x2   = line->v2()->xPos();
		y2   = line->v2()->yPos();

		// Get line colour
		col = lineColour(line);

		// Set line colour
		glColor4f(col.fr(), col.fg(), col.fb(), alpha * col.fa());

		// Draw the line
		glVertex2d(x1, y1);
		glVertex2d(x2, y2);

		// Direction tab
		if (show_direction)
		{
			fpoint2_t mid = line->point(MapObject::Point::Mid);
			fpoint2_t tab = line->dirTabPoint();
			glVertex2d(mid.x, mid.y);
			glVertex2d(tab.x, tab.y);
		}
	}
	glEnd();

	glEndList();
	lines_dirs_    = show_direction;
	lines_updated_ = App::runTimer();
}

// -----------------------------------------------------------------------------
// Renders map lines using an OpenGL Vertex Buffer Object
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLinesVBO(bool show_direction, float alpha)
{
	// Do nothing if there are no lines in the map
	if (map_->nLines() == 0)
		return;

	// Update lines VBO if required
	if (vbo_lines_ == 0 || show_direction != lines_dirs_ || map_->nLines() != n_lines_
		|| map_->geometryUpdated() > lines_updated_ || map_->modifiedSince(lines_updated_, MapObject::Type::Line))
		updateLinesVBO(show_direction, alpha);

	// Disable any blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Set VBO arrays to use
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	// Setup VBO pointers
	glBindBuffer(GL_ARRAY_BUFFER, vbo_lines_);
	glVertexPointer(2, GL_FLOAT, 24, nullptr);

	glColorPointer(4, GL_FLOAT, 24, ((char*)nullptr + 8));

	// Render the VBO
	if (show_direction)
		glDrawArrays(GL_LINES, 0, map_->nLines() * 4);
	else
		glDrawArrays(GL_LINES, 0, map_->nLines() * 2);

	// Clean state
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	lines_dirs_ = show_direction;
}

// -----------------------------------------------------------------------------
// Renders the line hilight overlay for line [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLineHilight(int index, float fade) const
{
	// Check hilight
	if (!map_->line(index))
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_hilight");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup rendering properties
	glLineWidth(line_width * ColourConfiguration::lineHilightWidth());

	// Render line
	MapLine* line = map_->line(index);
	double   x1   = line->v1()->xPos();
	double   y1   = line->v1()->yPos();
	double   x2   = line->v2()->xPos();
	double   y2   = line->v2()->yPos();
	glBegin(GL_LINES);
	glVertex2d(x1, y1);
	glVertex2d(x2, y2);
	glEnd();

	// Direction tab
	fpoint2_t mid = line->point(MapObject::Point::Mid);
	fpoint2_t tab = line->dirTabPoint();
	glBegin(GL_LINES);
	glVertex2d(mid.x, mid.y);
	glVertex2d(tab.x, tab.y);
	glEnd();
}

// -----------------------------------------------------------------------------
// Renders the line selection overlay for line indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderLineSelection(const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Set selection colour
	ColRGBA col = ColourConfiguration::colour("map_selection");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup rendering properties
	glLineWidth(line_width * ColourConfiguration::lineSelectionWidth());

	// Render selected lines
	MapLine* line;
	double   x1, y1, x2, y2;
	glBegin(GL_LINES);
	for (auto item : selection)
	{
		// Get line properties
		line = map_->line(item.index);
		x1   = line->v1()->xPos();
		y1   = line->v1()->yPos();
		x2   = line->v2()->xPos();
		y2   = line->v2()->yPos();

		// Draw line
		glVertex2d(x1, y1);
		glVertex2d(x2, y2);

		// Direction tab
		fpoint2_t mid = line->point(MapObject::Point::Mid);
		fpoint2_t tab = line->dirTabPoint();
		glVertex2d(mid.x, mid.y);
		glVertex2d(tab.x, tab.y);
	}
	glEnd();
}

// -----------------------------------------------------------------------------
// Renders the tagged line overlay for lines in [lines]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedLines(vector<MapLine*>& lines, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_tagged");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup rendering properties
	glLineWidth(line_width * ColourConfiguration::lineHilightWidth());

	// Go through tagged lines
	double     x1, y1, x2, y2;
	MapObject* object = MapEditor::editContext().selection().hilightedObject();
	for (auto line : lines)
	{
		// Render line
		x1 = line->v1()->xPos();
		y1 = line->v1()->yPos();
		x2 = line->v2()->xPos();
		y2 = line->v2()->yPos();
		glBegin(GL_LINES);
		glVertex2d(x1, y1);
		glVertex2d(x2, y2);
		glEnd();

		// Direction tab
		fpoint2_t mid = line->point(MapObject::Point::Mid);
		fpoint2_t tab = line->dirTabPoint();
		glBegin(GL_LINES);
		glVertex2d(mid.x, mid.y);
		glVertex2d(tab.x, tab.y);
		glEnd();

		// Action lines
		if (object && action_lines)
		{
			glLineWidth(line_width * 1.5f);
			Drawing::drawArrow(
				line->point(MapObject::Point::Within),
				object->point(MapObject::Point::Within),
				col,
				false,
				arrowhead_angle,
				arrowhead_length);
			glLineWidth(line_width * 3);
		}
	}
}

// -----------------------------------------------------------------------------
// Renders the tagging line overlay for lines in [lines]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggingLines(vector<MapLine*>& lines, float fade) const
{
	// Reset fade if tagging animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_tagging");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup rendering properties
	glLineWidth(line_width * ColourConfiguration::lineHilightWidth());

	// Go through tagging lines
	double     x1, y1, x2, y2;
	MapObject* object = MapEditor::editContext().selection().hilightedObject();
	for (auto line : lines)
	{
		// Render line
		x1 = line->v1()->xPos();
		y1 = line->v1()->yPos();
		x2 = line->v2()->xPos();
		y2 = line->v2()->yPos();
		glBegin(GL_LINES);
		glVertex2d(x1, y1);
		glVertex2d(x2, y2);
		glEnd();

		// Direction tab
		fpoint2_t mid = line->point(MapObject::Point::Mid);
		fpoint2_t tab = line->dirTabPoint();
		glBegin(GL_LINES);
		glVertex2d(mid.x, mid.y);
		glVertex2d(tab.x, tab.y);
		glEnd();

		// Action lines
		if (object && action_lines)
		{
			glLineWidth(line_width * 1.5f);
			Drawing::drawArrow(
				object->point(MapObject::Point::Within),
				line->point(MapObject::Point::Within),
				col,
				false,
				arrowhead_angle,
				arrowhead_length);
			glLineWidth(line_width * 5);
		}
	}
}

// -----------------------------------------------------------------------------
// Sets up the renderer for thing overlays (point sprites, etc.)
// -----------------------------------------------------------------------------
bool MapRenderer2D::setupThingOverlay() const
{
	// Get hilight texture
	GLTexture* tex = MapEditor::textureManager().editorImage("thing/hilight");
	if (thing_drawtype == Square || thing_drawtype == SquareSprite || thing_drawtype == FramedSprite)
		tex = MapEditor::textureManager().editorImage("thing/square/hilight");

	// Nothing to do if thing_overlay_square is true and thing_drawtype is 1 or 2 (circles or sprites)
	// or if the hilight circle texture isn't found for some reason
	if (!tex || (thing_overlay_square && (thing_drawtype == Round || thing_drawtype == Sprite)))
	{
		glDisable(GL_TEXTURE_2D);
		return false;
	}

	// Otherwise, we want the textured selection overlay
	glEnable(GL_TEXTURE_2D);
	tex->bind();

	// Setup point sprites if supported
	bool point = false;
	if (OpenGL::pointSpriteSupport())
	{
		glEnable(GL_POINT_SPRITE);
		glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
		point = true;
	}

	return point;
}

// -----------------------------------------------------------------------------
// Renders a thing overlay at [x,y] of size [radius]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingOverlay(double x, double y, double radius, bool point) const
{
	// Simplest case, thing_overlay_square is true and thing_drawtype is 1 or 2 (circles or sprites)
	if (thing_overlay_square && (thing_drawtype == Round || thing_drawtype == Sprite))
	{
		// Draw square
		glBegin(GL_QUADS);
		glVertex2d(x - radius, y - radius);
		glVertex2d(x - radius, y + radius);
		glVertex2d(x + radius, y + radius);
		glVertex2d(x + radius, y - radius);
		glEnd();

		return;
	}

	// Otherwise, we want the circle selection overlay
	double ps = radius * 2 * view_scale_;

	// Draw it
	if (point && ps <= OpenGL::maxPointSize())
	{
		// Point sprite
		glPointSize(ps);
		glBegin(GL_POINTS);
		glVertex2d(x, y);
		glEnd();
	}
	else
	{
		// Textured quad
		if (point)
			glDisable(GL_POINT_SPRITE);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2d(x - radius, y - radius);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2d(x - radius, y + radius);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2d(x + radius, y + radius);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2d(x + radius, y - radius);
		glEnd();
		if (point)
			glEnable(GL_POINT_SPRITE);
	}
}

// -----------------------------------------------------------------------------
// Renders a round thing icon at [x,y]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderRoundThing(
	double                 x,
	double                 y,
	double                 angle,
	const Game::ThingType& tt,
	float                  alpha,
	double                 radius_mult)
{
	// --- Determine texture to use ---
	GLTexture* tex    = nullptr;
	bool       rotate = false;

	// Set colour
	glColor4f(tt.colour().fr(), tt.colour().fg(), tt.colour().fb(), alpha);

	// Check for custom thing icon
	if (!tt.icon().empty() && !thing_force_dir && !things_angles_)
	{
		if (use_zeth_icons && tt.zethIcon() >= 0)
			tex = MapEditor::textureManager().editorImage(S_FMT("zethicons/zeth%02d", tt.zethIcon()));
		if (!tex)
			tex = MapEditor::textureManager().editorImage(S_FMT("thing/%s", tt.icon()));
	}

	if (!tex)
	{
		// Otherwise, normal thing image

		// Check if we want an angle indicator
		if (tt.angled() || thing_force_dir || things_angles_)
		{
			if (angle != 0)
				rotate = true; // Also rotate to angle
			tex = MapEditor::textureManager().editorImage("thing/normal_d");
		}
		else
			tex = MapEditor::textureManager().editorImage("thing/normal_n");
	}

	// If for whatever reason the thing texture doesn't exist, just draw a basic, square thing
	if (!tex)
	{
		renderSimpleSquareThing(x, y, angle, tt, alpha);
		return;
	}

	// Bind texture
	if (tex_last_ != tex)
	{
		tex->bind();
		tex_last_ = tex;
	}

	// Rotate if needed
	if (rotate)
	{
		glPushMatrix();
		glTranslated(x, y, 0);
		glRotated(angle, 0, 0, 1);
		x = y = 0;
	}

	// Draw thing
	double radius = tt.radius() * radius_mult;
	if (tt.shrinkOnZoom())
		radius = scaledRadius(radius);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2d(x - radius, y - radius);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2d(x - radius, y + radius);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2d(x + radius, y + radius);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2d(x + radius, y - radius);
	glEnd();

	// Restore previous matrix if rotated
	if (rotate)
		glPopMatrix();
}

// -----------------------------------------------------------------------------
// Renders a sprite thing icon at [x,y].
// If [fitradius] is true, the sprite is drawn to fit within the thing's radius
// -----------------------------------------------------------------------------
bool MapRenderer2D::renderSpriteThing(
	double                 x,
	double                 y,
	double                 angle,
	const Game::ThingType& tt,
	unsigned               index,
	float                  alpha,
	bool                   fitradius)
{
	// Refresh sprites list if needed
	if (thing_sprites_.size() != map_->nThings())
	{
		thing_sprites_.clear();
		for (unsigned a = 0; a < map_->nThings(); a++)
			thing_sprites_.push_back(nullptr);
	}

	// --- Determine texture to use ---
	bool       show_angle = false;
	GLTexture* tex        = index < thing_sprites_.size() ? thing_sprites_[index] : nullptr;

	// Attempt to get sprite texture
	if (!tex)
	{
		tex = MapEditor::textureManager().sprite(tt.sprite(), tt.translation(), tt.palette());

		if (index < thing_sprites_.size())
		{
			thing_sprites_[index]  = tex;
			thing_sprites_updated_ = App::runTimer();
		}
	}

	// If sprite not found, just draw as a normal, round thing
	if (!tex)
	{
		if (thing_drawtype == FramedSprite)
			renderRoundThing(x, y, angle, tt, alpha, 0.7);
		else
			renderRoundThing(x, y, angle, tt, alpha);
		return false;
	}

	// Check if we have to draw the angle arrow later
	if (tt.angled() || thing_force_dir || things_angles_)
		show_angle = true;

	//// If for whatever reason the thing texture doesn't exist, just draw a basic, square thing
	// if (!tex)
	//{
	//	renderSquareThing(x, y, angle, tt, alpha);
	//	return false;
	//}

	// Bind texture
	if (tex_last_ != tex)
	{
		tex->bind();
		tex_last_ = tex;
	}

	// Draw thing
	double hw = tex->getWidth() * 0.5;
	double hh = tex->getHeight() * 0.5;

	// Fit to radius if needed
	if (fitradius)
	{
		double scale = ((double)tt.radius() * 0.8) / max(hw, hh);
		hw *= scale;
		hh *= scale;
	}

	// Shadow if needed
	if (thing_shadow > 0.01f && alpha >= 0.9 && !fitradius)
	{
		double sz = (min(hw, hh)) * 0.1;
		if (sz < 1)
			sz = 1;
		glColor4f(0.0f, 0.0f, 0.0f, alpha * (thing_shadow * 0.7));
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2d(x - hw - sz, y - hh - sz);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2d(x - hw - sz, y + hh + sz);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2d(x + hw + sz, y + hh + sz);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2d(x + hw + sz, y - hh - sz);
		glEnd();
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2d(x - hw - sz, y - hh - sz - sz);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2d(x - hw - sz, y + hh + sz);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2d(x + hw + sz + sz, y + hh + sz);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2d(x + hw + sz + sz, y - hh - sz - sz);
		glEnd();
	}
	// Draw thing
	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2d(x - hw, y - hh);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2d(x - hw, y + hh);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2d(x + hw, y + hh);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2d(x + hw, y - hh);
	glEnd();


	return show_angle;
}

// -----------------------------------------------------------------------------
// Renders a square thing icon at [x,y]
// -----------------------------------------------------------------------------
bool MapRenderer2D::renderSquareThing(
	double                 x,
	double                 y,
	double                 angle,
	const Game::ThingType& tt,
	float                  alpha,
	bool                   showicon,
	bool                   framed)
{
	// --- Determine texture to use ---
	GLTexture* tex = nullptr;

	// Set colour
	glColor4f(tt.colour().fr(), tt.colour().fg(), tt.colour().fb(), alpha);

	// Show icon anyway if no sprite set
	if (tt.sprite().empty())
		showicon = true;

	// Check for custom thing icon
	if (!tt.icon().empty() && showicon && !thing_force_dir && !things_angles_ && !framed)
		tex = MapEditor::textureManager().editorImage(S_FMT("thing/square/%s", tt.icon()));

	// Otherwise, no icon
	int tc_start = 0;
	if (!tex)
	{
		if (framed)
		{
			tex = MapEditor::textureManager().editorImage("thing/square/frame");
		}
		else
		{
			tex = MapEditor::textureManager().editorImage("thing/square/normal_n");

			if ((tt.angled() && showicon) || thing_force_dir || things_angles_)
			{
				tex = MapEditor::textureManager().editorImage("thing/square/normal_d1");

				// Setup variables depending on angle
				switch ((int)angle)
				{
				case 0: // East: normal, texcoord 0
					break;
				case 45: // Northeast: diagonal, texcoord 0
					tex = MapEditor::textureManager().editorImage("thing/square/normal_d2");
					break;
				case 90: // North: normal, texcoord 2
					tc_start = 2;
					break;
				case 135: // Northwest: diagonal, texcoord 2
					tex      = MapEditor::textureManager().editorImage("thing/square/normal_d2");
					tc_start = 2;
					break;
				case 180: // West: normal, texcoord 4
					tc_start = 4;
					break;
				case 225: // Southwest: diagonal, texcoord 4
					tex      = MapEditor::textureManager().editorImage("thing/square/normal_d2");
					tc_start = 4;
					break;
				case 270: // South: normal, texcoord 6
					tc_start = 6;
					break;
				case 315: // Southeast: diagonal, texcoord 6
					tex      = MapEditor::textureManager().editorImage("thing/square/normal_d2");
					tc_start = 6;
					break;
				default: // Unsupported angle, don't draw arrow
					tex = MapEditor::textureManager().editorImage("thing/square/normal_n");
					break;
				};
			}
		}
	}

	// If for whatever reason the thing texture doesn't exist, just draw a basic, square thing
	if (!tex)
	{
		renderSimpleSquareThing(x, y, angle, tt, alpha);
		return false;
	}

	// Bind texture
	if (tex && tex_last_ != tex)
	{
		tex->bind();
		tex_last_ = tex;
	}

	// Draw thing
	double radius = tt.radius();
	if (tt.shrinkOnZoom())
		radius = scaledRadius(radius);
	glBegin(GL_QUADS);
	int tc = tc_start;
	glTexCoord2f(sq_thing_tc[tc], sq_thing_tc[tc + 1]);
	tc += 2;
	if (tc == 8)
		tc = 0;
	glVertex2d(x - radius, y - radius);
	glTexCoord2f(sq_thing_tc[tc], sq_thing_tc[tc + 1]);
	tc += 2;
	if (tc == 8)
		tc = 0;
	glVertex2d(x - radius, y + radius);
	glTexCoord2f(sq_thing_tc[tc], sq_thing_tc[tc + 1]);
	tc += 2;
	if (tc == 8)
		tc = 0;
	glVertex2d(x + radius, y + radius);
	glTexCoord2f(sq_thing_tc[tc], sq_thing_tc[tc + 1]);
	glVertex2d(x + radius, y - radius);
	glEnd();

	return ((tt.angled() || thing_force_dir || things_angles_) && !showicon);
}

// -----------------------------------------------------------------------------
// Renders a simple square thing icon at [x,y]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderSimpleSquareThing(double x, double y, double angle, const Game::ThingType& tt, float alpha)
	const
{
	// Get thing info
	double radius = tt.radius();
	if (tt.shrinkOnZoom())
		radius = scaledRadius(radius);
	double radius2 = radius * 0.1;

	// Move to thing position
	glPushMatrix();
	glTranslated(x, y, 0);

	// Draw background
	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	glBegin(GL_QUADS);
	glVertex2d(-radius, -radius);
	glVertex2d(-radius, radius);
	glVertex2d(radius, radius);
	glVertex2d(radius, -radius);
	glEnd();

	// Draw base
	// tt.getColour().set_gl(false);
	glColor4f(tt.colour().fr(), tt.colour().fg(), tt.colour().fb(), alpha);
	glBegin(GL_QUADS);
	glVertex2d(-radius + radius2, -radius + radius2);
	glVertex2d(-radius + radius2, radius - radius2);
	glVertex2d(radius - radius2, radius - radius2);
	glVertex2d(radius - radius2, -radius + radius2);
	glEnd();

	// Draw angle indicator (if needed)
	if (tt.angled() || thing_force_dir)
	{
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		glRotated(angle, 0, 0, 1);
		glBegin(GL_LINES);
		glVertex2d(0, 0);
		glVertex2d(radius, 0);
		glEnd();
	}

	// Restore previous matrix
	glPopMatrix();
}

// -----------------------------------------------------------------------------
// Renders map things
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThings(float alpha, bool force_dir)
{
	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	things_angles_ = force_dir;
	renderThingsImmediate(alpha);
}

// -----------------------------------------------------------------------------
// Renders map things in immediate mode
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingsImmediate(float alpha)
{
	// Display lists aren't really good for this, better to check for
	// visibility and just render things in immediate mode

	// Enable textures
	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, alpha);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tex_last_ = nullptr;

	// Go through things
	MapThing*   thing;
	double      x, y, angle;
	vector<int> things_arrows;
	long        last_update = thing_sprites_updated_;

	// Draw thing shadows if needed
	if (thing_shadow > 0.01f && thing_drawtype != Sprite)
	{
		glEnable(GL_TEXTURE_2D);
		GLTexture* tex_shadow = MapEditor::textureManager().editorImage("thing/shadow");
		if (thing_drawtype == Square || thing_drawtype == SquareSprite || thing_drawtype == FramedSprite)
			tex_shadow = MapEditor::textureManager().editorImage("thing/square/shadow");
		if (tex_shadow)
		{
			tex_shadow->bind();
			glColor4f(0.0f, 0.0f, 0.0f, alpha * thing_shadow);

			// Setup point sprites if supported
			bool point = false;
			if (OpenGL::pointSpriteSupport())
			{
				glEnable(GL_POINT_SPRITE);
				glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
				point = true;
			}

			for (unsigned a = 0; a < map_->nThings(); a++)
			{
				if (vis_t_[a] != Vis::In)
					continue;

				// No shadow if filtered
				thing = map_->thing(a);
				if (thing->isFiltered())
					continue;

				// Get thing info
				auto&  tt     = Game::configuration().thingType(thing->type());
				double radius = (tt.radius() + 1);
				if (tt.shrinkOnZoom())
					radius = scaledRadius(radius);
				radius *= 1.3;
				x = thing->xPos();
				y = thing->yPos();

				// Draw shadow
				if (point && radius * 2 * view_scale_ <= OpenGL::maxPointSize())
				{
					// Point sprite
					glPointSize(radius * 2 * view_scale_);
					glBegin(GL_POINTS);
					glVertex2d(x, y);
					glEnd();
				}
				else
				{
					// Textured quad
					if (point)
						glDisable(GL_POINT_SPRITE);
					glBegin(GL_QUADS);
					glTexCoord2f(0.0f, 1.0f);
					glVertex2d(x - radius, y - radius);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2d(x - radius, y + radius);
					glTexCoord2f(1.0f, 0.0f);
					glVertex2d(x + radius, y + radius);
					glTexCoord2f(1.0f, 1.0f);
					glVertex2d(x + radius, y - radius);
					glEnd();
					if (point)
						glEnable(GL_POINT_SPRITE);
				}
			}

			if (point)
				glDisable(GL_POINT_SPRITE);
		}
	}

	// Draw things
	double talpha;
	for (unsigned a = 0; a < map_->nThings(); a++)
	{
		if (vis_t_[a] != Vis::In)
			continue;

		// Get thing info
		thing = map_->thing(a);
		x     = thing->xPos();
		y     = thing->yPos();
		angle = thing->angle();

		// Set alpha
		if (thing->isFiltered())
			talpha = alpha * 0.25;
		else
			talpha = alpha;

		// Get thing type properties from game configuration
		auto& tt = Game::configuration().thingType(thing->type());

		// Reset thing sprite if modified
		if (thing->modifiedTime() > last_update && thing_sprites_.size() > a)
			thing_sprites_[a] = nullptr;

		// Draw thing depending on 'things_drawtype' cvar
		if (thing_drawtype == Sprite) // Drawtype 2: Sprites
		{
			// Check if we need to draw the direction arrow for this thing
			if (renderSpriteThing(x, y, angle, tt, a, talpha))
				things_arrows.push_back(a);
		}
		else if (thing_drawtype == Round) // Drawtype 1: Round
			renderRoundThing(x, y, angle, tt, talpha);
		else // Drawtype 0 (or other): Square
		{
			if (renderSquareThing(
					x, y, angle, tt, talpha, (thing_drawtype < SquareSprite), (thing_drawtype == FramedSprite)))
				things_arrows.push_back(a);
		}
	}

	// Draw thing sprites within squares if that drawtype is set
	if (thing_drawtype > Sprite)
	{
		glEnable(GL_TEXTURE_2D);

		for (unsigned a = 0; a < map_->nThings(); a++)
		{
			if (vis_t_[a] != Vis::In)
				continue;

			// Get thing info
			thing    = map_->thing(a);
			auto& tt = Game::configuration().thingType(thing->type());
			x        = thing->xPos();
			y        = thing->yPos();

			if (thing_drawtype == SquareSprite && tt.sprite().empty())
				continue;

			// Set alpha
			if (thing->isFiltered())
				talpha = alpha * 0.25;
			else
				talpha = alpha;

			renderSpriteThing(x, y, thing->angle(), tt, a, talpha, true);
		}
	}

	// Draw any thing direction arrows needed
	if (!things_arrows.empty())
	{
		ColRGBA acol = ColRGBA::WHITE;
		acol.a      = 255 * alpha * arrow_alpha;
		OpenGL::setColour(acol);
		// glColor4f(1.0f, 1.0f, 1.0f, alpha * arrow_alpha);
		GLTexture* tex_arrow = MapEditor::textureManager().editorImage("arrow");
		if (tex_arrow)
		{
			glEnable(GL_TEXTURE_2D);
			tex_arrow->bind();

			for (int things_arrow : things_arrows)
			{
				thing = map_->thing(things_arrow);
				if (arrow_colour)
				{
					auto& tt = Game::configuration().thingType(thing->type());
					if (tt.defined())
					{
						acol.set(tt.colour());
						acol.a = 255 * alpha * arrow_alpha;
						OpenGL::setColour(acol, false);
						// glColor4f(tt->getColour().fr(), tt->getColour().fg(), tt->getColour().fb(), alpha *
						// arrow_alpha);
					}
				}
				x = thing->xPos();
				y = thing->yPos();

				glPushMatrix();
				glTranslated(x, y, 0);
				glRotated(thing->angle(), 0, 0, 1);

				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 1.0f);
				glVertex2d(-32, -32);
				glTexCoord2f(0.0f, 0.0f);
				glVertex2d(-32, 32);
				glTexCoord2f(1.0f, 0.0f);
				glVertex2d(32, 32);
				glTexCoord2f(1.0f, 1.0f);
				glVertex2d(32, -32);
				glEnd();

				glPopMatrix();
			}
		}
	}

	// Disable textures
	glDisable(GL_TEXTURE_2D);
}

// -----------------------------------------------------------------------------
// Renders the thing hilight overlay for thing [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingHilight(int index, float fade) const
{
	// Check hilight
	if (!map_->thing(index))
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_hilight");
	col.a *= fade;
	OpenGL::setColour(col);

	// Get thing info
	MapThing* thing = map_->thing(index);
	auto&     tt    = Game::configuration().thingType(thing->type());
	double    x     = thing->xPos();
	double    y     = thing->yPos();

	// Get thing radius
	double radius = tt.radius();

	// Check if we want square overlays
	if (thing_overlay_square)
	{
		glDisable(GL_TEXTURE_2D);
		glLineWidth(3.0f);
		glBegin(GL_LINE_LOOP);
		glVertex2d(x - radius, y - radius);
		glVertex2d(x - radius, y + radius);
		glVertex2d(x + radius, y + radius);
		glVertex2d(x + radius, y - radius);
		glEnd();
		col.a *= 0.5;
		OpenGL::setColour(col, false);
		glBegin(GL_QUADS);
		glVertex2d(x - radius, y - radius);
		glVertex2d(x - radius, y + radius);
		glVertex2d(x + radius, y + radius);
		glVertex2d(x + radius, y - radius);
		glEnd();

		return;
	}

	// Shrink if needed
	if (tt.shrinkOnZoom())
		radius = scaledRadius(radius);

	// Adjust radius
	if (thing_drawtype == Square || thing_drawtype > Sprite)
		radius += 6;
	else
		radius *= 1.1 + (0.2 * fade);
	radius += halo_width * view_scale_inv_;

	// Setup hilight thing texture
	GLTexture* tex;
	if (thing_drawtype == Square || thing_drawtype == SquareSprite || thing_drawtype == FramedSprite)
		tex = MapEditor::textureManager().editorImage("thing/square/hilight");
	else
		tex = MapEditor::textureManager().editorImage("thing/hilight");
	if (tex)
	{
		glEnable(GL_TEXTURE_2D);
		tex->bind();
	}

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2d(x - radius, y - radius);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2d(x - radius, y + radius);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2d(x + radius, y + radius);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2d(x + radius, y - radius);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

// -----------------------------------------------------------------------------
// Renders the thing selection overlay for thing indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderThingSelection(const ItemSelection& selection, float fade) const
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Set selection colour
	ColRGBA col = ColourConfiguration::colour("map_selection");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup overlay rendering
	bool point = setupThingOverlay();

	// Draw all selection overlays
	for (auto item : selection)
	{
		MapThing* thing  = map_->thing(item.index);
		auto&     tt     = Game::configuration().thingType(thing->type());
		double    radius = tt.radius();
		if (tt.shrinkOnZoom())
			radius = scaledRadius(radius);

		// Adjust radius if the overlay isn't square
		if (!thing_overlay_square)
			radius += 8;
		radius += halo_width * view_scale_inv_;

		// Draw it
		renderThingOverlay(thing->xPos(), thing->yPos(), radius * (0.8 + (0.2 * fade)), point);
	}

	// Clean up gl state
	if (point)
		glDisable(GL_POINT_SPRITE);
	glDisable(GL_TEXTURE_2D);
}

// -----------------------------------------------------------------------------
// Renders the tagged thing overlay for things in [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedThings(vector<MapThing*>& things, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_tagged");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup overlay rendering
	bool point = setupThingOverlay();

	// Draw all tagged overlays
	for (auto thing : things)
	{
		auto&  tt     = Game::configuration().thingType(thing->type());
		double radius = tt.radius();
		if (tt.shrinkOnZoom())
			radius = scaledRadius(radius);

		// Adjust radius if the overlay isn't square
		if (!thing_overlay_square)
			radius += 8;
		radius += halo_width * view_scale_inv_;

		// Draw it
		renderThingOverlay(thing->xPos(), thing->yPos(), radius, point);
	}

	// Clean up gl state
	if (point)
		glDisable(GL_POINT_SPRITE);
	glDisable(GL_TEXTURE_2D);

	// Draw action lines
	// Because gl state is in texture mode above, we cannot merge the loops
	MapObject* object = MapEditor::editContext().selection().hilightedObject();
	if (object && action_lines)
	{
		fpoint2_t dst = object->point(MapObject::Point::Within);
		glLineWidth(line_width * 1.5f);
		for (auto thing : things)
			Drawing::drawArrow(thing->point(MapObject::Point::Within), dst, col, false, arrowhead_angle, arrowhead_length);
	}
}

// -----------------------------------------------------------------------------
// Renders the tagging thing overlay for things in [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggingThings(vector<MapThing*>& things, float fade) const
{
	// Reset fade if tagging animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_tagging");
	col.a *= fade;
	OpenGL::setColour(col);

	// Setup overlay rendering
	bool point = setupThingOverlay();

	// Draw all tagging overlays
	for (auto thing : things)
	{
		auto&  tt     = Game::configuration().thingType(thing->type());
		double radius = tt.radius();
		if (tt.shrinkOnZoom())
			radius = scaledRadius(radius);

		// Adjust radius if the overlay isn't square
		if (!thing_overlay_square)
			radius += 12;
		radius += halo_width * view_scale_inv_;

		// Draw it
		renderThingOverlay(thing->xPos(), thing->yPos(), radius, point);
	}

	// Clean up gl state
	if (point)
		glDisable(GL_POINT_SPRITE);
	glDisable(GL_TEXTURE_2D);

	// Draw action lines
	// Because gl state is in texture mode above, we cannot merge the loops
	MapObject* object = MapEditor::editContext().selection().hilightedObject();
	if (object && action_lines)
	{
		fpoint2_t src = object->point(MapObject::Point::Within);
		glLineWidth(line_width * 1.5f);
		for (auto thing : things)
			Drawing::drawArrow(src, thing->point(MapObject::Point::Within), col, false, arrowhead_angle, arrowhead_length);
	}
}

// -----------------------------------------------------------------------------
// Renders thing pathing lines/arrows for [things]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderPathedThings(vector<MapThing*>& things)
{
	// Skip if action lines are not desired, or if there's nothing to do
	if (!action_lines || things.empty())
		return;

	// Check if paths need updating
	bool update = false;
	if (thing_paths_.empty())
		update = true;
	else if (map_->thingsUpdated() > thing_paths_updated_)
	{
		for (auto& thing : things)
		{
			if (thing->modifiedTime() > thing_paths_updated_)
			{
				update = true;
				break;
			}
		}
		if (!update)
			thing_paths_updated_ = App::runTimer();
	}

	// Get colours
	wxColour col(arrow_pathed_color.value);
	ColRGBA   pathedcol(COLWX(col), col.Alpha());
	col.Set(arrow_dragon_color.value);
	ColRGBA dragoncol(COLWX(col), col.Alpha());

	if (update)
	{
		thing_paths_.clear();

		// Find things that need to be pathed
		for (unsigned a = 0; a < things.size(); ++a)
		{
			MapThing* thing = things[a];
			ThingPath path{};
			path.from_index = 0;
			path.to_index   = 0;

			auto& tt = Game::configuration().thingType(thing->type());

			// Dragon Path
			if (tt.flags() & Game::ThingType::FLAG_DRAGON)
			{
				MapThing* first = map_->firstThingWithId(thing->intProperty("id"));
				if (first)
				{
					path.from_index = thing->index();
					path.to_index   = first->index();
					path.type       = PathType::Dragon;
					thing_paths_.push_back(path);

					vector<MapThing*> dragon_things;
					dragon_things.clear();
					map_->dragonTargets(first, dragon_things);
					for (unsigned d = 0; d < dragon_things.size(); ++d)
					{
						int   id1 = dragon_things[d]->intProperty("id");
						int   a11 = dragon_things[d]->intProperty("arg0");
						int   a12 = dragon_things[d]->intProperty("arg1");
						int   a13 = dragon_things[d]->intProperty("arg2");
						int   a14 = dragon_things[d]->intProperty("arg3");
						int   a15 = dragon_things[d]->intProperty("arg4");
						auto& tt1 = Game::configuration().thingType(dragon_things[d]->type());
						for (unsigned e = d + 1; e < dragon_things.size(); ++e)
						{
							int   id2  = dragon_things[e]->intProperty("id");
							int   a21  = dragon_things[e]->intProperty("arg0");
							int   a22  = dragon_things[e]->intProperty("arg1");
							int   a23  = dragon_things[e]->intProperty("arg2");
							int   a24  = dragon_things[e]->intProperty("arg3");
							int   a25  = dragon_things[e]->intProperty("arg4");
							auto& tt2  = Game::configuration().thingType(dragon_things[e]->type());
							bool l1to2 = ((a11 == id2) || (a12 == id2) || (a13 == id2) || (a14 == id2) || (a15 == id2));
							bool l2to1 = ((a21 == id1) || (a22 == id1) || (a23 == id1) || (a24 == id1) || (a25 == id1));
							if (!((tt1.flags() | tt2.flags()) & Game::ThingType::FLAG_DRAGON))
							{
								ThingPath dpath{};
								if (l1to2)
								{
									dpath.from_index = dragon_things[e]->index();
									dpath.to_index   = dragon_things[d]->index();
									dpath.type       = l2to1 ? PathType::DragonBoth : PathType::Dragon;
								}
								else if (l2to1)
								{
									dpath.from_index = dragon_things[d]->index();
									dpath.to_index   = dragon_things[e]->index();
									dpath.type       = PathType::Dragon;
								}
								thing_paths_.push_back(dpath);
							}
						}
					}
				}
				continue;
			}

			// Normal Path
			int tid = -1, tid2 = -1;
			int nexttype = tt.nextType();
			int nextargs = tt.nextArgs();
			if (nextargs)
			{
				int    pos = nextargs % 10;
				string na  = "arg_";
				na[3]      = ('0' + pos - 1);
				tid        = thing->intProperty(na);
			}
			if (nextargs >= 10)
			{
				int    pos = nextargs / 10;
				string na  = "arg_";
				na[3]      = ('0' + pos - 1);
				tid += (256 * thing->intProperty(na));
			}
			for (unsigned b = a + 1; b < things.size(); ++b)
			{
				MapThing* thing2 = things[b];
				if (thing2->type() == nexttype)
				{
					auto& tt2 = Game::configuration().thingType(thing2->type());
					nextargs  = tt2.nextArgs();
					if (nextargs)
					{
						int    pos = nextargs % 10;
						string na  = "arg_";
						na[3]      = ('0' + pos - 1);
						tid2       = thing2->intProperty(na);
					}
					if (nextargs >= 10)
					{
						int    pos = nextargs / 10;
						string na  = "arg_";
						na[3]      = ('0' + pos - 1);
						tid2 += (256 * thing2->intProperty(na));
					}
					if (thing2->intProperty("id") == tid)
					{
						path.from_index = thing->index();
						path.to_index   = thing2->index();
						path.type       = (tid2 == thing->intProperty("id")) ? PathType::NormalBoth : PathType::Normal;
					}
					else if (thing->intProperty("id") == tid2)
					{
						path.from_index = thing2->index();
						path.to_index   = thing->index();
						path.type       = PathType::Normal;
					}
					thing_paths_.push_back(path);
				}
			}
		}
		thing_paths_updated_ = App::runTimer();
	}

	// Setup GL stuff
	glLineWidth(line_width * 1.5f);

	for (auto& thing_path : thing_paths_)
	{
		if (thing_path.from_index == thing_path.to_index)
			continue;

		MapThing* from = map_->thing(thing_path.from_index);

		if (from && ((from->intProperty("arg3") | (from->intProperty("arg4") << 8)) > 0))
		{
			MapThing* to = map_->thing(thing_path.to_index);
			if (!to)
				continue;

			Drawing::drawArrow(
				to->point(MapObject::Point::Mid),
				from->point(MapObject::Point::Mid),
				(thing_path.type == PathType::DragonBoth || thing_path.type == PathType::Dragon) ? dragoncol :
																								   pathedcol,
				(thing_path.type == PathType::NormalBoth || thing_path.type == PathType::DragonBoth),
				arrowhead_angle,
				arrowhead_length);
		}
	}
}

// -----------------------------------------------------------------------------
// Renders map flats (sectors)
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlats(int type, bool texture, float alpha)
{
	// Don't bother if (practically) invisible
	if (alpha <= 0.01f)
		return;

	if (OpenGL::vboSupport() && flats_use_vbo)
		renderFlatsVBO(type, texture, alpha);
	else
		renderFlatsImmediate(type, texture, alpha);

	flats_updated_ = App::runTimer();
}

// -----------------------------------------------------------------------------
// Sorting function to sort polygons by their texture
// -----------------------------------------------------------------------------
bool sortPolyByTex(Polygon2D* left, Polygon2D* right)
{
	return left->getTexture()->glId() < right->getTexture()->glId();
}

// -----------------------------------------------------------------------------
// Renders map flats in immediate mode
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatsImmediate(int type, bool texture, float alpha)
{
	using Game::Feature;
	using Game::UDMFFeature;

	if (texture)
	{
		glEnable(GL_TEXTURE_2D);

		// Apply flat alpha from theme
		alpha *= ColourConfiguration::flatAlpha();
	}

	if (flat_ignore_light)
		glColor4f(flat_brightness, flat_brightness, flat_brightness, alpha);

	// Re-init flats texture list if invalid
	if ((texture && tex_flats_.size() < map_->nSectors()) || last_flat_type_ != type)
	{
		tex_flats_.clear();
		for (unsigned a = 0; a < map_->nSectors(); a++)
			tex_flats_.push_back(nullptr);

		last_flat_type_ = type;
	}

	// Go through sectors
	GLTexture* tex_last = nullptr;
	GLTexture* tex      = nullptr;
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		MapSector* sector = map_->sector(a);

		// Skip if sector is out of view
		if (vis_s_[a] != Vis::In)
			continue;

		if (texture)
		{
			if (!tex_flats_[a] || sector->modifiedTime() > flats_updated_ - 100)
			{
				// Get the sector texture
				if (type <= 1)
				{
					tex = MapEditor::textureManager().flat(
						sector->floor().texture, Game::configuration().featureSupported(Feature::MixTexFlats));
				}
				else
				{
					tex = MapEditor::textureManager().flat(
						sector->ceiling().texture, Game::configuration().featureSupported(Feature::MixTexFlats));
				}

				tex_flats_[a] = tex;
			}
			else
				tex = tex_flats_[a];

			// Bind the texture if needed
			if (tex)
			{
				if (!tex_last)
					glEnable(GL_TEXTURE_2D);
				if (tex != tex_last)
					tex->bind();
			}
			else if (tex_last)
				glDisable(GL_TEXTURE_2D);

			tex_last = tex;
		}

		// Setup polygon texture info if needed
		Polygon2D* poly = sector->polygon();
		if (texture && poly->getTexture() != tex)
		{
			poly->setTexture(tex);

			// Get scaling/offset info
			double ox  = 0;
			double oy  = 0;
			double sx  = tex->getScaleX();
			double sy  = tex->getScaleY();
			double rot = 0;
			// Check for various UDMF extensions
			if (MapEditor::editContext().mapDesc().format == MAP_UDMF)
			{
				// Floor
				if (type <= 1)
				{
					if (Game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningfloor");
						oy = sector->floatProperty("ypanningfloor");
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscalefloor"));
						sy *= (1.0 / sector->floatProperty("yscalefloor"));
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationfloor");
				}
				// Ceiling
				else
				{
					if (Game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningceiling");
						oy = sector->floatProperty("ypanningceiling");
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscaleceiling"));
						sy *= (1.0 / sector->floatProperty("yscaleceiling"));
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationceiling");
				}
			}

			poly->updateTextureCoords(sx, sy, ox, oy, rot);
		}

		// Render the polygon
		if (!flat_ignore_light)
		{
			ColRGBA col = sector->colourAt(type);
			col.ampf(flat_brightness, flat_brightness, flat_brightness, 1.0f);
			glColor4f(col.fr(), col.fg(), col.fb(), alpha);
		}
		poly->render();
	}

	if (texture)
		glDisable(GL_TEXTURE_2D);
}

// -----------------------------------------------------------------------------
// Renders map flats using an OpenGL Vertex Buffer Object
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatsVBO(int type, bool texture, float alpha)
{
	using Game::Feature;
	using Game::UDMFFeature;

	bool vbo_updated = false;

	if (flat_ignore_light)
		glColor4f(flat_brightness, flat_brightness, flat_brightness, alpha);

	if (!glGenBuffers)
		return;

	// Apply flat alpha from theme
	if (texture)
		alpha *= ColourConfiguration::flatAlpha();

	// Re-init flats texture list if invalid
	if ((texture && tex_flats_.size() != map_->nSectors()) || last_flat_type_ != type)
	{
		tex_flats_.clear();
		for (unsigned a = 0; a < map_->nSectors(); a++)
			tex_flats_.push_back(nullptr);

		last_flat_type_ = type;
	}

	// First, check if any polygon vertex data has changed (in this case we need to refresh the entire vbo)
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		Polygon2D* poly = map_->sector(a)->polygon();
		if (poly && poly->vboUpdate() > 1)
		{
			// LOG_MESSAGE(1, "Updating sector %d polygon vbo data", a);
			updateFlatsVBO();
			vbo_updated = true;
		}
	}

	// Create VBO if necessary
	if (!vbo_updated && vbo_flats_ == 0)
		updateFlatsVBO();

	// if (vbo_updated)
	//	LOG_MESSAGE(1, "Updated vbo");

	// Setup opengl state
	if (texture)
		glEnable(GL_TEXTURE_2D);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_flats_);

	// Setup VBO pointers
	Polygon2D::setupVBOPointers();

	// Go through sectors
	GLTexture* tex_last = nullptr;
	GLTexture* tex      = nullptr;
	bool       first;
	unsigned   update = 0;
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		MapSector* sector = map_->sector(a);

		// Skip if sector is out of view
		if (vis_s_[a] != Vis::In)
			continue;

		first = false;
		if (texture)
		{
			if (!tex_flats_[a] || sector->modifiedTime() > flats_updated_ - 100)
			{
				// Get the sector texture
				if (type <= 1)
					tex = MapEditor::textureManager().flat(
						sector->floor().texture, Game::configuration().featureSupported(Feature::MixTexFlats));
				else
					tex = MapEditor::textureManager().flat(
						sector->ceiling().texture, Game::configuration().featureSupported(Feature::MixTexFlats));

				tex_flats_[a] = tex;
			}
			else
				tex = tex_flats_[a];
		}

		// Setup polygon texture info if needed
		Polygon2D* poly = sector->polygon();
		if (texture && poly->getTexture() != tex)
		{
			poly->setTexture(tex); // Set polygon texture

			// Get scaling/offset info
			double ox  = 0;
			double oy  = 0;
			double sx  = tex->getScaleX();
			double sy  = tex->getScaleY();
			double rot = 0;
			// Check for various UDMF extensions
			if (MapEditor::editContext().mapDesc().format == MAP_UDMF)
			{
				// Floor
				if (type <= 1)
				{
					if (Game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningfloor");
						oy = sector->floatProperty("ypanningfloor");
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscalefloor"));
						sy *= (1.0 / sector->floatProperty("yscalefloor"));
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationfloor");
				}
				// Ceiling
				else
				{
					if (Game::configuration().featureSupported(UDMFFeature::FlatPanning))
					{
						ox = sector->floatProperty("xpanningceiling");
						oy = sector->floatProperty("ypanningceiling");
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatScaling))
					{
						sx *= (1.0 / sector->floatProperty("xscaleceiling"));
						sy *= (1.0 / sector->floatProperty("yscaleceiling"));
					}
					if (Game::configuration().featureSupported(UDMFFeature::FlatRotation))
						rot = sector->floatProperty("rotationceiling");
				}
			}
			// Scaling applies to offsets as well.
			// Note for posterity: worldpanning only applies to textures, not flats
			ox /= sx;
			oy /= sy;

			poly->updateTextureCoords(sx, sy, ox, oy, rot);
		}

		// Update polygon VBO data if needed
		if (poly->vboUpdate() > 0)
		{
			poly->updateVBOData();
			update++;
			if (update > 200)
				break;
		}

		// Bind the texture if needed
		if (tex)
		{
			if (!tex_last || first)
				glEnable(GL_TEXTURE_2D);
			if (tex != tex_last)
				tex->bind();
		}
		else if (!tex_last || first)
			glDisable(GL_TEXTURE_2D);
		tex_last = tex;

		// Render the polygon
		if (!flat_ignore_light)
		{
			ColRGBA col = sector->colourAt(type);
			col.ampf(flat_brightness, flat_brightness, flat_brightness, 1.0f);
			glColor4f(col.fr(), col.fg(), col.fb(), alpha);
		}
		poly->renderVBO(false);
	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	// Clean up opengl state
	if (texture)
		glDisable(GL_TEXTURE_2D);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// -----------------------------------------------------------------------------
// Renders the flat hilight overlay for sector [index]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatHilight(int index, float fade) const
{
	// Check hilight
	if (!map_->sector(index))
		return;

	// Reset fade if hilight animation is disabled
	if (!map_animate_hilight)
		fade = 1.0f;

	// Set hilight colour
	ColRGBA col = ColourConfiguration::colour("map_hilight");
	col.a *= fade;
	OpenGL::setColour(col);

	// Fill if cvar is set
	if (sector_hilight_fill)
	{
		glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * 0.5f);
		map_->sector(index)->polygon()->render();
		glLineWidth(line_width * ColourConfiguration::lineHilightWidth());
	}
	else
		glLineWidth(line_width * ColourConfiguration::lineHilightWidth());

	// Get all lines belonging to the hilighted sector
	vector<MapLine*> lines;
	map_->sector(index)->getLines(lines);

	// Draw hilight
	for (auto& line : lines)
	{
		if (!line)
			continue;

		// Draw line
		glBegin(GL_LINES);
		glVertex2d(line->v1()->xPos(), line->v1()->yPos());
		glVertex2d(line->v2()->xPos(), line->v2()->yPos());
		glEnd();
	}

	// Draw sector split lines
	if (test_ssplit)
	{
		glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * 0.5f);
		glLineWidth(1.0f);
		map_->sector(index)->polygon()->renderWireframe();
	}

	//// TEST draw text point
	// glPointSize(8.0f);
	// glBegin(GL_POINTS);
	// glVertex2d(map->sector(index)->point(MapObject::Point::Within).x,
	// map->sector(index)->point(MapObject::Point::Within).y); glEnd();
}

// -----------------------------------------------------------------------------
// Renders the flat selection overlay for sector indices in [selection]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderFlatSelection(const ItemSelection& selection, float fade)
{
	// Check anything is selected
	if (selection.empty())
		return;

	// Reset fade if selection animation is disabled
	if (!map_animate_selection)
		fade = 1.0f;

	// Set selection colour
	ColRGBA col = ColourConfiguration::colour("map_selection");
	col.a *= fade;
	OpenGL::setColour(col);

	// Draw selection
	glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * 0.75f);
	vector<MapSide*> sides_selected;
	for (auto item : selection)
	{
		// Don't draw if outside screen (but still draw if it's small)
		if (vis_s_[item.index] == Vis::Out)
			continue;

		// Get the sector's polygon
		Polygon2D*        poly  = map_->sector(item.index)->polygon();
		vector<MapSide*>& sides = map_->sector(item.index)->connectedSides();

		if (poly->hasPolygon())
		{
			if (sector_selected_fill)
				map_->sector(item.index)->polygon()->render();

			for (auto& side : sides)
				sides_selected.push_back(side);
		}
		else
		{
			// Something went wrong with the polygon, just draw sector outline instead
			glColor4f(col.fr(), col.fg(), col.fb(), col.fa());
			glBegin(GL_LINES);
			for (auto& side : sides)
			{
				MapLine* line = side->parentLine();
				glVertex2d(line->v1()->xPos(), line->v1()->yPos());
				glVertex2d(line->v2()->xPos(), line->v2()->yPos());
			}
			glEnd();

			glColor4f(col.fr(), col.fg(), col.fb(), col.fa() * 0.6f);
		}
	}

	// Draw selection outline
	glColor4f(col.fr(), col.fg(), col.fb(), col.fa());
	glLineWidth(line_width * 2);
	bool* lines_drawn = new bool[map_->nLines()];
	memset(lines_drawn, 0, map_->nLines());
	glBegin(GL_LINES);
	for (auto& side : sides_selected)
	{
		MapLine* line = side->parentLine();
		if (lines_drawn[line->index()])
			continue;

		glVertex2d(line->v1()->xPos(), line->v1()->yPos());
		glVertex2d(line->v2()->xPos(), line->v2()->yPos());
		lines_drawn[line->index()] = true;
	}
	glEnd();
	delete[] lines_drawn;
}

// -----------------------------------------------------------------------------
// Renders the tagged flat overlay for sectors in [sectors]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderTaggedFlats(vector<MapSector*>& sectors, float fade) const
{
	// Reset fade if tagged animation is disabled
	if (!map_animate_tagged)
		fade = 1.0f;

	// Set colour
	ColRGBA col = ColourConfiguration::colour("map_tagged");
	col.a *= fade;
	OpenGL::setColour(col);

	// Render each sector polygon
	glDisable(GL_TEXTURE_2D);
	MapObject* object = MapEditor::editContext().selection().hilightedObject();
	for (auto& sector : sectors)
	{
		sector->polygon()->render();

		// Get all lines belonging to the tagged sector
		vector<MapLine*> lines;
		sector->getLines(lines);

		// Draw hilight
		for (auto& line : lines)
		{
			if (!line)
				continue;

			// Draw line
			glBegin(GL_LINES);
			glVertex2d(line->v1()->xPos(), line->v1()->yPos());
			glVertex2d(line->v2()->xPos(), line->v2()->yPos());
			glEnd();
		}

		// Action lines
		if (object && action_lines)
		{
			// Skip if the tagged sector is adjacent
			if (object->objType() == MapObject::Type::Line)
			{
				MapLine* line = (MapLine*)object;
				if (line->frontSector() == sector || line->backSector() == sector)
					continue;
			}

			glLineWidth(line_width * 1.5f);
			Drawing::drawArrow(
				sector->point(MapObject::Point::Within),
				object->point(MapObject::Point::Within),
				col,
				false,
				arrowhead_angle,
				arrowhead_length);
		}
	}
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for vertex indices in [vertices], to show movement
// by [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingVertices(const vector<MapEditor::Item>& vertices, const fpoint2_t& move_vec) const
{
	uint8_t* lines_drawn = new uint8_t[map_->nLines()];
	memset(lines_drawn, 0, map_->nLines());

	// Determine what lines need drawing (and which of their vertices are being moved)
	for (auto item : vertices)
	{
		MapVertex* v = map_->vertex(item.index);
		for (unsigned l = 0; l < v->nConnectedLines(); l++)
		{
			MapLine* line = v->connectedLine(l);

			if (line->v1() == v)
				lines_drawn[line->index()] |= 1;
			if (line->v2() == v)
				lines_drawn[line->index()] |= 2;
		}
	}

	// Draw any lines attached to the moving vertices
	glLineWidth(line_width);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_LINES);
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		MapLine* line  = map_->line(a);
		uint8_t  drawn = lines_drawn[line->index()];

		// Skip if not attached to any moving vertices
		if (drawn == 0)
			continue;

		// Set line colour
		OpenGL::setColour(lineColour(line, true), false);

		// First vertex
		if (drawn & 1)
			glVertex2d(line->x1() + move_vec.x, line->y1() + move_vec.y);
		else
			glVertex2d(line->x1(), line->y1());

		// Second vertex
		if (drawn & 2)
			glVertex2d(line->x2() + move_vec.x, line->y2() + move_vec.y);
		else
			glVertex2d(line->x2(), line->y2());
	}
	glEnd();

	// Set 'moving' colour
	OpenGL::setColour(ColourConfiguration::colour("map_moving"));

	// Draw moving vertex overlays
	bool point = setupVertexRendering(1.5f);
	glBegin(GL_POINTS);
	for (auto item : vertices)
	{
		glVertex2d(map_->vertex(item.index)->xPos() + move_vec.x, map_->vertex(item.index)->yPos() + move_vec.y);
	}
	glEnd();

	// Clean up
	delete[] lines_drawn;
	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for line indices in [lines], to show movement by
// [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingLines(const vector<MapEditor::Item>& lines, const fpoint2_t& move_vec) const
{
	uint8_t* lines_drawn = new uint8_t[map_->nLines()];
	memset(lines_drawn, 0, map_->nLines());

	// Determine what lines need drawing (and which of their vertices are being moved)
	for (auto item : lines)
	{
		// Check first vertex
		MapVertex* v = map_->line(item.index)->v1();
		for (unsigned l = 0; l < v->nConnectedLines(); l++)
		{
			MapLine* line = v->connectedLine(l);

			if (line->v1() == v)
				lines_drawn[line->index()] |= 1;
			if (line->v2() == v)
				lines_drawn[line->index()] |= 2;
		}

		// Check second vertex
		v = map_->line(item.index)->v2();
		for (unsigned l = 0; l < v->nConnectedLines(); l++)
		{
			MapLine* line = v->connectedLine(l);

			if (line->v1() == v)
				lines_drawn[line->index()] |= 1;
			if (line->v2() == v)
				lines_drawn[line->index()] |= 2;
		}
	}

	// Draw any lines attached to the moving vertices
	glLineWidth(line_width);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_LINES);
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		MapLine* line  = map_->line(a);
		uint8_t  drawn = lines_drawn[line->index()];

		// Skip if not attached to any moving vertices
		if (drawn == 0)
			continue;

		// Set line colour
		OpenGL::setColour(lineColour(line, true), false);

		// First vertex
		if (drawn & 1)
			glVertex2d(line->x1() + move_vec.x, line->y1() + move_vec.y);
		else
			glVertex2d(line->x1(), line->y1());

		// Second vertex
		if (drawn & 2)
			glVertex2d(line->x2() + move_vec.x, line->y2() + move_vec.y);
		else
			glVertex2d(line->x2(), line->y2());
	}
	glEnd();

	// Set 'moving' colour
	OpenGL::setColour(ColourConfiguration::colour("map_moving"));

	// Draw moving line overlays
	glLineWidth(line_width * 3);
	glBegin(GL_LINES);
	for (auto item : lines)
	{
		MapLine* line = map_->line(item.index);
		glVertex2d(line->x1() + move_vec.x, line->y1() + move_vec.y);
		glVertex2d(line->x2() + move_vec.x, line->y2() + move_vec.y);
	}
	glEnd();

	// Clean up
	delete[] lines_drawn;
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for sector indices in [sectors], to show movement
// by [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingSectors(const vector<MapEditor::Item>& sectors, const fpoint2_t& move_vec) const
{
	// Determine what lines are being moved
	uint8_t* lines_moved = new uint8_t[map_->nLines()];
	memset(lines_moved, 0, map_->nLines());
	for (auto item : sectors)
	{
		// Go through connected sides
		vector<MapSide*>& sides = map_->sector(item.index)->connectedSides();
		for (auto& side : sides)
			lines_moved[side->parentLine()->index()] = 1; // Mark parent line as moved
	}

	// Build list of moving lines
	vector<MapEditor::Item> lines;
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		if (lines_moved[a] > 0)
			lines.emplace_back((int)a, MapEditor::ItemType::Line);
	}

	// Draw moving lines
	renderMovingLines(lines, move_vec);

	// Clean up
	delete[] lines_moved;
}

// -----------------------------------------------------------------------------
// Renders the moving overlay for thing indices in [things], to show movement by
// [move_vec]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderMovingThings(const vector<MapEditor::Item>& things, const fpoint2_t& move_vec)
{
	// Enable textures
	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tex_last_ = nullptr;

	// Draw things
	MapThing* thing;
	double    x, y, angle;
	for (unsigned a = 0; a < things.size(); a++)
	{
		// Get thing info
		thing = map_->thing(things[a].index);
		x     = thing->xPos() + move_vec.x;
		y     = thing->yPos() + move_vec.y;
		angle = thing->angle();

		// Get thing type properties from game configuration
		auto& tt = Game::configuration().thingType(thing->type());

		// Draw thing depending on 'things_drawtype' cvar
		if (thing_drawtype == Sprite) // Drawtype 2: Sprites
			renderSpriteThing(x, y, angle, tt, a, 1.0f);
		else if (thing_drawtype == Round) // Drawtype 1: Round
			renderRoundThing(x, y, angle, tt, 1.0f);
		else // Drawtype 0 (or other): Square
			renderSquareThing(x, y, angle, tt, 1.0f, thing_drawtype < SquareSprite, thing_drawtype == FramedSprite);
	}

	// Draw thing sprites within squares if that drawtype is set
	if (thing_drawtype > Sprite)
	{
		glEnable(GL_TEXTURE_2D);

		for (auto item : things)
		{
			// Get thing info
			thing    = map_->thing(item.index);
			auto& tt = Game::configuration().thingType(thing->type());
			x        = thing->xPos() + move_vec.x;
			y        = thing->yPos() + move_vec.y;
			angle    = thing->angle();

			renderSpriteThing(x, y, angle, tt, item.index, 1.0f, true);
		}
	}

	// Set 'moving' colour
	OpenGL::setColour(ColourConfiguration::colour("map_moving"));

	// Draw moving thing overlays
	bool point = setupThingOverlay();
	for (auto item : things)
	{
		thing         = map_->thing(item.index);
		auto&  tt     = Game::configuration().thingType(thing->type());
		double radius = tt.radius();
		if (tt.shrinkOnZoom())
			radius = scaledRadius(radius);

		// Adjust radius if the overlay isn't square
		if (!thing_overlay_square)
			radius += 8;

		renderThingOverlay(thing->xPos() + move_vec.x, thing->yPos() + move_vec.y, radius, point);
	}

	// Clean up gl state
	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Renders pasting overlay for [things] at [pos]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderPasteThings(vector<MapThing*>& things, const fpoint2_t& pos)
{
	// Enable textures
	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tex_last_ = nullptr;

	// Draw things
	double x, y, angle;
	for (auto& thing : things)
	{
		// Get thing info
		x     = thing->xPos() + pos.x;
		y     = thing->yPos() + pos.y;
		angle = thing->angle();

		// Get thing type properties from game configuration
		auto& tt = Game::configuration().thingType(thing->type());

		// Draw thing depending on 'things_drawtype' cvar
		if (thing_drawtype == Sprite) // Drawtype 2: Sprites
			renderSpriteThing(x, y, angle, tt, wxUINT32_MAX, 1.0f);
		else if (thing_drawtype == Round) // Drawtype 1: Round
			renderRoundThing(x, y, angle, tt, 1.0f);
		else // Drawtype 0 (or other): Square
			renderSquareThing(x, y, angle, tt, 1.0f, thing_drawtype < SquareSprite, thing_drawtype == FramedSprite);
	}

	// Draw thing sprites within squares if that drawtype is set
	if (thing_drawtype > Sprite)
	{
		glEnable(GL_TEXTURE_2D);

		for (auto& thing : things)
		{
			// Get thing info
			auto& tt = Game::configuration().thingType(thing->type());
			x        = thing->xPos() + pos.x;
			y        = thing->yPos() + pos.y;
			angle    = thing->angle();

			renderSpriteThing(x, y, angle, tt, wxUINT32_MAX, 1.0f, true);
		}
	}

	// Set 'drawing' colour
	OpenGL::setColour(ColourConfiguration::colour("map_linedraw"));

	// Draw moving thing overlays
	bool point = setupThingOverlay();
	for (auto& thing : things)
	{
		auto&  tt     = Game::configuration().thingType(thing->type());
		double radius = tt.radius();
		if (tt.shrinkOnZoom())
			radius = scaledRadius(radius);

		// Adjust radius if the overlay isn't square
		if (!thing_overlay_square)
			radius += 8;

		renderThingOverlay(thing->xPos() + pos.x, thing->yPos() + pos.y, radius, point);
	}

	// Clean up gl state
	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}
}

// -----------------------------------------------------------------------------
// Renders object edit group overlay for [group]
// -----------------------------------------------------------------------------
void MapRenderer2D::renderObjectEditGroup(ObjectEditGroup* group)
{
	// Simple test
	vector<fpoint2_t> vertex_points;
	group->getVerticesToDraw(vertex_points);
	vector<ObjectEditGroup::Line> lines;
	group->getLinesToDraw(lines);

	// Set 'drawing' colour
	OpenGL::setColour(ColourConfiguration::colour("map_linedraw"));

	// --- Lines ---

	// Lines
	glBegin(GL_LINES);
	glLineWidth(line_width);
	for (auto& line : lines)
	{
		OpenGL::setColour(lineColour(line.map_line, true), false);
		glVertex2d(line.v1->position.x, line.v1->position.y);
		glVertex2d(line.v2->position.x, line.v2->position.y);
	}
	glEnd();

	// Edit overlay
	OpenGL::setColour(ColourConfiguration::colour("map_object_edit"));
	glLineWidth(line_width * 3);
	glBegin(GL_LINES);
	for (auto& line : lines)
	{
		if (line.isExtra())
			continue;

		glVertex2d(line.v1->position.x, line.v1->position.y);
		glVertex2d(line.v2->position.x, line.v2->position.y);
	}
	glEnd();

	// --- Vertices ---

	// Setup rendering properties
	bool point = setupVertexRendering(1.0f);
	OpenGL::setColour(ColRGBA::WHITE);
	OpenGL::setColour(ColourConfiguration::colour("map_object_edit"), false);

	// Render vertices
	glBegin(GL_POINTS);
	for (auto& vertex_point : vertex_points)
		glVertex2d(vertex_point.x, vertex_point.y);
	glEnd();

	// Clean up
	if (point)
	{
		glDisable(GL_POINT_SPRITE);
		glDisable(GL_TEXTURE_2D);
	}

	// --- Things ---

	// Get things to draw
	vector<ObjectEditGroup::Thing> things;
	group->getThingsToDraw(things);

	if (!things.empty())
	{
		// Enable textures
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		tex_last_ = nullptr;

		// Draw things
		MapThing* thing;
		double    x, y, angle;
		for (auto& t : things)
		{
			// Get thing info
			thing = t.map_thing;
			x     = t.position.x;
			y     = t.position.y;
			angle = thing->angle();

			// Get thing type properties from game configuration
			auto& tt = Game::configuration().thingType(thing->type());

			// Draw thing depending on 'things_drawtype' cvar
			if (thing_drawtype == Sprite) // Drawtype 2: Sprites
				renderSpriteThing(x, y, angle, tt, thing->index(), 1.0f);
			else if (thing_drawtype == Round) // Drawtype 1: Round
				renderRoundThing(x, y, angle, tt, 1.0f);
			else // Drawtype 0 (or other): Square
				renderSquareThing(x, y, angle, tt, 1.0f, thing_drawtype < SquareSprite, thing_drawtype == FramedSprite);
		}

		// Draw thing sprites within squares if that drawtype is set
		if (thing_drawtype > Sprite)
		{
			glEnable(GL_TEXTURE_2D);

			for (auto& t : things)
			{
				// Get thing info
				thing    = t.map_thing;
				auto& tt = Game::configuration().thingType(thing->type());
				x        = t.position.x;
				y        = t.position.y;
				angle    = thing->angle();

				renderSpriteThing(x, y, angle, tt, thing->index(), 1.0f, true);
			}
		}

		// Set 'moving' colour
		OpenGL::setColour(ColourConfiguration::colour("map_object_edit"));

		// Draw moving thing overlays
		bool o_point = setupThingOverlay();
		for (auto& t : things)
		{
			thing         = t.map_thing;
			auto&  tt     = Game::configuration().thingType(thing->type());
			double radius = tt.radius();
			if (tt.shrinkOnZoom())
				radius = scaledRadius(radius);

			// Adjust radius if the overlay isn't square
			if (!thing_overlay_square)
				radius += 8;

			renderThingOverlay(t.position.x, t.position.y, radius, o_point);
		}

		// Clean up gl state
		if (o_point)
		{
			glDisable(GL_POINT_SPRITE);
			glDisable(GL_TEXTURE_2D);
		}
	}
}

// -----------------------------------------------------------------------------
// (Re)builds the map vertices VBO
// -----------------------------------------------------------------------------
void MapRenderer2D::updateVerticesVBO()
{
	// Create VBO if needed
	if (vbo_vertices_ == 0)
		glGenBuffers(1, &vbo_vertices_);

	// Fill vertices VBO
	int      nfloats = map_->nVertices() * 2;
	GLfloat* verts   = new GLfloat[nfloats];
	unsigned i       = 0;
	for (unsigned a = 0; a < map_->nVertices(); a++)
	{
		verts[i++] = map_->vertex(a)->xPos();
		verts[i++] = map_->vertex(a)->yPos();
	}
	glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * nfloats, verts, GL_STATIC_DRAW);

	// Clean up
	delete[] verts;
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	n_vertices_       = map_->nVertices();
	vertices_updated_ = App::runTimer();
}

// -----------------------------------------------------------------------------
// (Re)builds the map lines VBO
// -----------------------------------------------------------------------------
void MapRenderer2D::updateLinesVBO(bool show_direction, float base_alpha)
{
	LOG_MESSAGE(3, "Updating lines VBO");

	// Create VBO if needed
	if (vbo_lines_ == 0)
		glGenBuffers(1, &vbo_lines_);

	// Determine the number of vertices per line
	int vpl = 2;
	if (show_direction)
		vpl = 4;

	// Fill lines VBO
	int      nverts = map_->nLines() * vpl;
	GLVert*  lines  = new GLVert[nverts];
	unsigned v      = 0;
	ColRGBA   col;
	float    alpha;
	for (unsigned a = 0; a < map_->nLines(); a++)
	{
		MapLine* line = map_->line(a);

		// Get line colour
		col   = lineColour(line);
		alpha = base_alpha * col.fa();

		// Set line vertices
		lines[v].x     = line->v1()->xPos();
		lines[v].y     = line->v1()->yPos();
		lines[v + 1].x = line->v2()->xPos();
		lines[v + 1].y = line->v2()->yPos();

		// Set line colour(s)
		lines[v].r = lines[v + 1].r = col.fr();
		lines[v].g = lines[v + 1].g = col.fg();
		lines[v].b = lines[v + 1].b = col.fb();
		lines[v].a = lines[v + 1].a = alpha;

		// Direction tab if needed
		if (show_direction)
		{
			fpoint2_t mid  = line->point(MapObject::Point::Mid);
			fpoint2_t tab  = line->dirTabPoint();
			lines[v + 2].x = mid.x;
			lines[v + 2].y = mid.y;
			lines[v + 3].x = tab.x;
			lines[v + 3].y = tab.y;

			// Colours
			lines[v + 2].r = lines[v + 3].r = col.fr();
			lines[v + 2].g = lines[v + 3].g = col.fg();
			lines[v + 2].b = lines[v + 3].b = col.fb();
			lines[v + 2].a = lines[v + 3].a = alpha * 0.6f;
		}

		// Next line
		v += vpl;
	}
	glBindBuffer(GL_ARRAY_BUFFER, vbo_lines_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLVert) * nverts, lines, GL_STATIC_DRAW);

	// Clean up
	delete[] lines;
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	n_lines_       = map_->nLines();
	lines_updated_ = App::runTimer();
}

// -----------------------------------------------------------------------------
// (Re)builds the map flats VBO
// -----------------------------------------------------------------------------
void MapRenderer2D::updateFlatsVBO()
{
	if (!flats_use_vbo)
		return;

	// Create VBO if needed
	if (vbo_flats_ == 0)
		glGenBuffers(1, &vbo_flats_);

	// Get total size needed
	unsigned totalsize = 0;
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		Polygon2D* poly = map_->sector(a)->polygon();
		totalsize += poly->vboDataSize();
	}

	// Allocate buffer data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_flats_);
	glBufferData(GL_ARRAY_BUFFER, totalsize, nullptr, GL_STATIC_DRAW);

	// Write polygon data to VBO
	unsigned offset = 0;
	unsigned index  = 0;
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		Polygon2D* poly = map_->sector(a)->polygon();
		offset          = poly->writeToVBO(offset, index);
		index += poly->totalVertices();
	}

	// Clean up
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	flats_updated_ = App::runTimer();
}

// -----------------------------------------------------------------------------
// Updates map object visibility info depending on the current view
// -----------------------------------------------------------------------------
void MapRenderer2D::updateVisibility(const fpoint2_t& view_tl, const fpoint2_t& view_br)
{
	// Sector visibility
	if (map_->nSectors() != vis_s_.size())
	{
		// Number of sectors changed, reset array
		vis_s_.assign(map_->nSectors(), Vis::In);
	}
	for (unsigned a = 0; a < map_->nSectors(); a++)
	{
		// Check against sector bounding box
		BBox bbox = map_->sector(a)->boundingBox();
		vis_s_[a]   = Vis::In;
		if (bbox.max.x < view_tl.x || bbox.max.y < view_tl.y || bbox.min.x > view_br.x || bbox.min.y > view_br.y)
			vis_s_[a] = Vis::Out;

		// Check if the sector is worth drawing
		if ((bbox.max.x - bbox.min.x) * view_scale_ < 4 || (bbox.max.y - bbox.min.y) * view_scale_ < 4)
			vis_s_[a] = Vis::Small;
	}

	// Thing visibility
	if (map_->nThings() != vis_t_.size())
	{
		// Number of things changed, reset array
		vis_t_.assign(map_->nThings(), Vis::In);
	}
	double x, y;
	double radius;
	for (unsigned a = 0; a < vis_t_.size(); a++)
	{
		vis_t_[a] = Vis::In;
		x         = map_->thing(a)->xPos();
		y         = map_->thing(a)->yPos();

		// Get thing type properties from game configuration
		auto& tt = Game::configuration().thingType(map_->thing(a)->type());
		radius   = tt.radius() * 1.3;

		// Ignore if outside of screen
		if (x + radius < view_tl.x || x - radius > view_br.x || y + radius < view_tl.y || y - radius > view_br.y)
			vis_t_[a] = Vis::Out;

		// Check if the thing is worth drawing
		else if (radius * view_scale_ < 2)
			vis_t_[a] = Vis::Small;
	}
}

// -----------------------------------------------------------------------------
// Updates all VBOs and other cached data
// -----------------------------------------------------------------------------
void MapRenderer2D::forceUpdate(float line_alpha)
{
	// Update variables
	this->view_scale_inv_ = 1.0 / view_scale_;
	tex_flats_.clear();
	thing_sprites_.clear();
	thing_paths_.clear();

	if (OpenGL::vboSupport())
	{
		updateVerticesVBO();
		updateLinesVBO(lines_dirs_, line_alpha);
	}
	else
	{
		if (list_lines_ > 0)
		{
			glDeleteLists(list_lines_, 1);
			list_lines_ = 0;
		}
		if (list_vertices_ > 0)
		{
			glDeleteLists(list_vertices_, 1);
			list_vertices_ = 0;
		}
	}
	renderVertices(view_scale_);
	renderLines(lines_dirs_);
}

// -----------------------------------------------------------------------------
// Returns [radius] scaled such that it stays the same size on screen at all
// zoom levels
// -----------------------------------------------------------------------------
double MapRenderer2D::scaledRadius(int radius) const
{
	if (radius > 16)
		radius = 16;

	if (view_scale_ > 1.0)
		return radius * view_scale_inv_;
	else
		return (double)radius;
}

// -----------------------------------------------------------------------------
// Returns true if the current visibility info is valid
// -----------------------------------------------------------------------------
bool MapRenderer2D::visOK() const
{
	return !(map_->nSectors() != vis_s_.size() || map_->nThings() != vis_t_.size());
}
