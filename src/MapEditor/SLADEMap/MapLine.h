#pragma once

#include "MapObject.h"
#include "common.h"

class MapVertex;
class MapSide;
class MapSector;

class MapLine : public MapObject
{
	friend class SLADEMap;

public:
	// Line parts
	enum Part
	{
		FrontMiddle = 0x01,
		FrontUpper  = 0x02,
		FrontLower  = 0x04,
		BackMiddle  = 0x08,
		BackUpper   = 0x10,
		BackLower   = 0x20,
	};

	// Binary map format structs
	struct DoomData
	{
		uint16_t vertex1;
		uint16_t vertex2;
		uint16_t flags;
		uint16_t type;
		uint16_t sector_tag;
		uint16_t side1;
		uint16_t side2;
	};
	struct HexenData
	{
		uint16_t vertex1;
		uint16_t vertex2;
		uint16_t flags;
		uint8_t  type;
		uint8_t  args[5];
		uint16_t side1;
		uint16_t side2;
	};
	struct Doom64Data
	{
		uint16_t vertex1;
		uint16_t vertex2;
		uint32_t flags;
		uint16_t type;
		uint16_t sector_tag;
		uint16_t side1;
		uint16_t side2;
	};

	// UDMF property id strings
	static const string PROP_V1;
	static const string PROP_V2;
	static const string PROP_S1;
	static const string PROP_S2;
	static const string PROP_SPECIAL;
	static const string PROP_ID;

	MapLine(SLADEMap* parent = nullptr) : MapObject{ Type::Line, parent } {}
	MapLine(MapVertex* v1, MapVertex* v2, MapSide* s1, MapSide* s2, SLADEMap* parent = nullptr);
	~MapLine() = default;

	bool isOk() const { return vertex1_ && vertex2_; }

	MapVertex* v1() const { return vertex1_; }
	MapVertex* v2() const { return vertex2_; }
	MapSide*   s1() const { return side1_; }
	MapSide*   s2() const { return side2_; }
	int        special() const { return special_; }
	int        id() const { return id_; }

	MapSector* frontSector() const;
	MapSector* backSector() const;

	double x1() const;
	double y1() const;
	double x2() const;
	double y2() const;

	int v1Index() const;
	int v2Index() const;
	int s1Index() const;
	int s2Index() const;

	bool   boolProperty(const string& key) override;
	int    intProperty(const string& key) override;
	double floatProperty(const string& key) override;
	string stringProperty(const string& key) override;
	void   setBoolProperty(const string& key, bool value) override;
	void   setIntProperty(const string& key, int value) override;
	void   setFloatProperty(const string& key, double value) override;
	void   setStringProperty(const string& key, const string& value) override;
	bool   scriptCanModifyProp(const string& key) override;

	void setS1(MapSide* side);
	void setS2(MapSide* side);

	fpoint2_t point(Point point) override;
	fpoint2_t point1() const;
	fpoint2_t point2() const;
	fseg2_t   seg() const;
	double    length();
	bool      doubleSector() const;
	fpoint2_t frontVector();
	fpoint2_t dirTabPoint(double length = 0);
	double    distanceTo(fpoint2_t point);
	int       needsTexture() const;
	bool      intersects(MapLine* other, Vec2<double>& intersect_point) const;

	void clearUnneededTextures();
	void resetInternals();
	void flip(bool sides = true);

	void writeBackup(Backup* backup) override;
	void readBackup(Backup* backup) override;
	void copy(MapObject*) override;

	operator Debuggable() const
	{
		if (!this)
			return "<line NULL>";

		return Debuggable(S_FMT("<line %u>", index_));
	}

private:
	// Basic data
	MapVertex* vertex1_ = nullptr;
	MapVertex* vertex2_ = nullptr;
	MapSide*   side1_   = nullptr;
	MapSide*   side2_   = nullptr;
	int        special_ = 0;
	int        id_      = 0;

	// Internally used info
	double    length_ = -1.;
	double    ca_     = 0.; // Used for intersection calculations
	double    sa_     = 0.; // ^^
	fpoint2_t front_vec_;
};
