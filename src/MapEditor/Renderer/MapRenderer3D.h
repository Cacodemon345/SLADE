#pragma once

#include "General/ListenerAnnouncer.h"
#include "MapEditor/Edit/Edit3D.h"
#include "MapEditor/SLADEMap/SLADEMap.h"

class ItemSelection;
class GLTexture;
class Polygon2D;

namespace Game
{
class ThingType;
}

class MapRenderer3D : public Listener
{
public:
	// Structs
	enum
	{
		// Common flags
		TRANSADD = 2,

		// Quad/flat flags
		SKY = 4,

		// Quad flags
		BACK   = 8,
		UPPER  = 16,
		LOWER  = 32,
		MIDTEX = 64,

		// Flat flags
		CEIL = 8,

		// Thing flags
		ICON  = 4,
		DRAWN = 8,
		ZETH  = 16,
	};
	struct GLVertex
	{
		float x, y, z;
		float tx, ty;
	};
	struct Quad
	{
		GLVertex   points[4] = { {}, {}, {}, {} };
		ColRGBA     colour    = { 255, 255, 255, 255, 0 };
		ColRGBA     fogcolour = { 0, 0, 0, 255, 0 };
		uint8_t    light     = 255;
		GLTexture* texture   = nullptr;
		uint8_t    flags     = 0;
		float      alpha     = 1.f;
	};
	struct Line
	{
		vector<Quad> quads;
		long         updated_time = 0;
		bool         visible      = true;
		MapLine*     line         = nullptr;
	};
	struct Thing
	{
		uint8_t                flags        = 0;
		Game::ThingType const* type         = nullptr;
		MapSector*             sector       = nullptr;
		float                  z            = 0.f;
		float                  height       = 0.f;
		GLTexture*             sprite       = nullptr;
		long                   updated_time = 0;
	};
	struct Flat
	{
		uint8_t    flags = 0;
		uint8_t    light = 255;
		ColRGBA     colour;
		ColRGBA     fogcolour;
		GLTexture* texture = nullptr;
		Plane    plane;
		float      alpha        = 1.f;
		MapSector* sector       = nullptr;
		long       updated_time = 0;
	};

	MapRenderer3D(SLADEMap* map = nullptr);
	~MapRenderer3D();

	bool fullbrightEnabled() const { return fullbright_; }
	bool fogEnabled() const { return fog_; }
	void enableFullbright(bool enable = true) { fullbright_ = enable; }
	void enableFog(bool enable = true) { fog_ = enable; }
	int  itemDistance() const { return item_dist_; }
	void enableHilight(bool render) { render_hilight_ = render; }
	void enableSelection(bool render) { render_selection_ = render; }

	bool init();
	void refresh();
	void clearData();
	void buildSkyCircle();

	Quad* getQuad(const MapEditor::Item& item);
	Flat* getFlat(const MapEditor::Item& item);

	// Camera
	void cameraMove(double distance, bool z = true);
	void cameraTurn(double angle);
	void cameraMoveUp(double distance);
	void cameraStrafe(double distance);
	void cameraPitch(double amount);
	void cameraUpdateVectors();
	void cameraSet(const fpoint3_t& position, const fpoint2_t& direction);
	void cameraSetPosition(const fpoint3_t& position);
	void cameraApplyGravity(double mult);
	void cameraLook(double xrel, double yrel);

	double           camPitch() const { return cam_pitch_; }
	const fpoint3_t& camPosition() const { return cam_position_; }
	const fpoint2_t& camDirection() const { return cam_direction_; }

	// -- Rendering --
	void setupView(int width, int height);
	void setLight(ColRGBA& colour, uint8_t light, float alpha = 1.0f) const;
	void setFog(ColRGBA& fogcol, uint8_t light);
	void renderMap();
	void renderSkySlice(
		float top,
		float bottom,
		float atop,
		float abottom,
		float size,
		float tx = 0.125f,
		float ty = 2.0f);
	void renderSky();

	// Flats
	void updateFlatTexCoords(unsigned index, bool floor);
	void updateSector(unsigned index);
	void renderFlat(Flat* flat);
	void renderFlats();
	void renderFlatSelection(const ItemSelection& selection, float alpha = 1.0f) const;

	// Walls
	void setupQuad(Quad* quad, double x1, double y1, double x2, double y2, double top, double bottom) const;
	void setupQuad(Quad* quad, double x1, double y1, double x2, double y2, const Plane& top, const Plane& bottom) const;
	void setupQuadTexCoords(
		Quad*  quad,
		int    length,
		double o_left,
		double o_top,
		double h_top,
		double h_bottom,
		bool   pegbottom = false,
		double sx        = 1,
		double sy        = 1) const;
	void updateLine(unsigned index);
	void renderQuad(Quad* quad, float alpha = 1.0f);
	void renderWalls();
	void renderTransparentWalls();
	void renderWallSelection(const ItemSelection& selection, float alpha = 1.0f);

	// Things
	void updateThing(unsigned index, MapThing* thing);
	void renderThings();
	void renderThingSelection(const ItemSelection& selection, float alpha = 1.0f);

	// VBO stuff
	void updateFlatsVBO();
	void updateWallsVBO() {}

	// Visibility checking
	void  quickVisDiscard();
	float calcDistFade(double distance, double max = -1) const;
	void  checkVisibleQuads();
	void  checkVisibleFlats();

	// Hilight
	MapEditor::Item determineHilight();
	void            renderHilight(const MapEditor::Item& hilight, float alpha = 1.0f);

	// Listener stuff
	void onAnnouncement(Announcer* announcer, string_view event_name, MemChunk& event_data);

private:
	SLADEMap*  map_;
	bool       fullbright_       = false;
	bool       fog_              = true;
	GLTexture* tex_last_         = nullptr;
	unsigned   n_quads_          = 0;
	unsigned   n_flats_          = 0;
	int        flat_last_        = 9;
	bool       render_hilight_   = true;
	bool       render_selection_ = true;
	ColRGBA     fog_colour_last_;
	float      fog_depth_last_ = 0.f;

	// Visibility
	vector<float> dist_sectors_;

	// Camera
	fpoint3_t cam_position_;
	fpoint2_t cam_direction_;
	double    cam_pitch_ = 0.;
	fpoint3_t cam_dir3d_;
	fpoint3_t cam_strafe_;
	double    gravity_   = 0.5;
	int       item_dist_ = 0;

	// Map Structures
	vector<Line>  lines_;
	Quad**        quads_ = nullptr;
	vector<Quad*> quads_transparent_;
	vector<Thing> things_;
	vector<Flat>  floors_;
	vector<Flat>  ceilings_;
	Flat**        flats_ = nullptr;

	// VBOs
	unsigned vbo_floors_   = 0;
	unsigned vbo_ceilings_ = 0;
	unsigned vbo_walls_    = 0;

	// Sky
	struct GLVertexEx
	{
		float x, y, z;
		float tx, ty;
		float alpha;
	};
	string    skytex1_;
	string    skytex2_;
	ColRGBA    skycol_top_;
	ColRGBA    skycol_bottom_;
	fpoint2_t sky_circle_[32];
};
