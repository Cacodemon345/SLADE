#pragma once

class PaletteCanvas;
class Palette;

class PaletteDialog : public wxDialog
{
public:
	PaletteDialog(Palette* palette);
	~PaletteDialog() = default;

	ColRGBA	selectedColour() const;

private:
	PaletteCanvas * pal_canvas_;
};
