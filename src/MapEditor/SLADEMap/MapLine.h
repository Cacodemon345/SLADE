#pragma once

#include "common.h"
#include "MapObject.h"

class MapVertex;
class MapSide;
class MapSector;

class MapLine : public MapObject
{
	friend class SLADEMap;
public:
	// Line parts (textures)
	enum Part
	{
		FrontMiddle	= 0x01,
		FrontUpper	= 0x02,
		FrontLower	= 0x04,
		BackMiddle	= 0x08,
		BackUpper	= 0x10,
		BackLower	= 0x20,
	};

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
		uint16_t	vertex1;
		uint16_t	vertex2;
		uint16_t	flags;
		uint8_t		type;
		uint8_t		args[5];
		uint16_t	side1;
		uint16_t	side2;
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

	MapLine(SLADEMap* parent = nullptr);
	MapLine(MapVertex* v1, MapVertex* v2, MapSide* s1, MapSide* s2, SLADEMap* parent = nullptr);
	~MapLine();

	bool	isOk() const { return vertex1_ && vertex2_; }

	MapVertex*	v1() const { return vertex1_; }
	MapVertex*	v2() const { return vertex2_; }
	MapSide*	s1() const { return side1_; }
	MapSide*	s2() const { return side2_; }
	int			special() const { return special_; }
	int			id() const { return id_; }

	MapSector*	frontSector();
	MapSector*	backSector();

	double	x1();
	double	y1();
	double	x2();
	double	y2();

	int	v1Index();
	int	v2Index();
	int	s1Index();
	int	s2Index();

	bool	boolProperty(const string& key) override;
	int		intProperty(const string& key) override;
	double	floatProperty(const string& key) override;
	string	stringProperty(const string& key) override;
	void	setBoolProperty(const string& key, bool value) override;
	void	setIntProperty(const string& key, int value) override;
	void	setFloatProperty(const string& key, double value) override;
	void	setStringProperty(const string& key, const string& value) override;
	bool	scriptCanModifyProp(const string& key) override;

	void	setS1(MapSide* side);
	void	setS2(MapSide* side);

	fpoint2_t	point(Point point = Point::Mid) override;
	fpoint2_t	point1();
	fpoint2_t	point2();
	fseg2_t		seg();
	double		length();
	double		angle();
	bool		doubleSector();
	fpoint2_t	frontVector();
	fpoint2_t	dirTabPoint(double length = 0);
	double		distanceTo(fpoint2_t point);
	int			needsTexture();
	void		disconnectFromVertices();

	void	clearUnneededTextures();
	void	resetInternals();
	void	flip(bool sides = true);

	void	writeBackup(Backup* backup) override;
	void	readBackup(Backup* backup) override;
	void	copy(MapObject*) override;

	operator Debuggable() const {
		if (!this)
			return "<line NULL>";

		return Debuggable(S_FMT("<line %u>", index_));
	}

	typedef std::unique_ptr<MapLine> UPtr;

private:
	// Basic data
	MapVertex*	vertex1_	= nullptr;
	MapVertex*	vertex2_	= nullptr;
	MapSide*	side1_		= nullptr;
	MapSide*	side2_		= nullptr;
	int			special_	= 0;
	int			id_			= 0;

	// Internally used info
	double		length_	= -1;
	double		ca_		= 0;	// Used for intersection calculations
	double		sa_		= 0;	// ^^
	fpoint2_t	front_vec_;
};
