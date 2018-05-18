#pragma once

#include "OpenGL/Drawing.h"

class MapThing;
class GLTexture;

class ThingInfoOverlay
{
public:
	ThingInfoOverlay();
	~ThingInfoOverlay() = default;

	void update(MapThing* thing);
	void draw(int bottom, int right, float alpha = 1.0f);

private:
	string  sprite_;
	string  translation_;
	string  palette_;
	string  icon_;
	int     zeth_ = 0;
	TextBox text_box_;
	int     last_size_ = 100;
};
