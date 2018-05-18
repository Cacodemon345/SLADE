#pragma once

#include "OpenGL/Drawing.h"

class MapLine;

class LineInfoOverlay
{
public:
	LineInfoOverlay();
	~LineInfoOverlay() = default;

	void update(MapLine* line);
	void draw(int bottom, int right, float alpha = 1.0f);

private:
	int     last_size_ = 100;
	double  scale_     = 1.;
	TextBox text_box_;

	struct Side
	{
		bool   exists = false;
		string info;
		string offsets;
		string tex_upper;
		string tex_middle;
		string tex_lower;
		bool   needs_upper  = false;
		bool   needs_middle = false;
		bool   needs_lower  = false;
	};
	Side side_front_;
	Side side_back_;

	void drawSide(int bottom, int right, float alpha, Side& side, int xstart = 0) const;
	void drawTexture(float alpha, int x, int y, string_view texture, bool needed, string_view pos = "U") const;
};
