#pragma once

#include "MapObject.h"

class MapLine;

class MapVertex : public MapObject
{
	friend class SLADEMap;

public:
	struct DoomData
	{
		short x;
		short y;
	};

	struct Doom64Data
	{
		// These are actually fixed_t
		int32_t x;
		int32_t y;
	};

	MapVertex(SLADEMap* parent = nullptr) : MapObject{ Type::Vertex, parent } {}
	MapVertex(double x, double y, SLADEMap* parent = nullptr);
	~MapVertex() = default;

	const Vec2<double>& position() const { return position_; }
	double              xPos() const { return position_.x; }
	double              yPos() const { return position_.y; }

	fpoint2_t point(Point point) override;

	int    intProperty(const string& key) override;
	double floatProperty(const string& key) override;
	void   setIntProperty(const string& key, int value) override;
	void   setFloatProperty(const string& key, double value) override;
	bool   scriptCanModifyProp(const string& key) override;

	void     connectLine(MapLine* line);
	void     disconnectLine(MapLine* line);
	unsigned nConnectedLines() const { return connected_lines_.size(); }
	MapLine* connectedLine(unsigned index);

	const vector<MapLine*>& connectedLines() const { return connected_lines_; }

	void writeBackup(Backup* backup) override;
	void readBackup(Backup* backup) override;

	operator Debuggable() const
	{
		if (!this)
			return Debuggable("<vertex NULL>");

		return Debuggable(fmt::format("<vertex {}>", index_));
	}

private:
	// Basic data
	Vec2<double> position_;

	// Internal info
	vector<MapLine*> connected_lines_;
};
