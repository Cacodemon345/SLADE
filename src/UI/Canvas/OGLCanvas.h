#pragma once

#include "Graphics/Palette/Palette.h"
#include "OpenGL/OpenGL.h"
#include "common.h"

class wxWindow;
#ifdef USE_SFML_RENDERWINDOW
class OGLCanvas : public wxControl, public sf::RenderWindow
{
#else
class OGLCanvas : public wxGLCanvas
{
#endif
public:
	OGLCanvas(wxWindow* parent, int id, bool handle_timer = true, int timer_interval = 100);
	~OGLCanvas() = default;

	Palette*     palette() { return &palette_; }
	void         setPalette(Palette* pal) { palette_.copyPalette(pal); }
	bool         setContext() const;
	void         createSFML();
	void         init();
	virtual void draw() = 0;
	virtual void update(long frametime) {}
	void         drawCheckeredBackground() const;
	wxWindow*    toPanel(wxWindow* parent);
	bool         setActive();
	void         setup2D() const;

#ifdef USE_SFML_RENDERWINDOW
	void SwapBuffers() { display(); }
#endif

protected:
	bool    init_done_ = false;
	Palette palette_;
	wxTimer timer_;
	long    last_time_ = 0;
	bool    recreate_  = false;

	void onPaint(wxPaintEvent& e);
	void onEraseBackground(wxEraseEvent& e);
	void onTimer(wxTimerEvent& e);
	void onResize(wxSizeEvent& e);
};
