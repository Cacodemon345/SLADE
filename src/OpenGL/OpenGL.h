#pragma once

// OpenGL
#ifdef __WXMSW__
// Windows GL headers
#include "External/glew/glew.h" // Use built-in GLEW so we don't need any extra dlls
#include <gl/GL.h>
#include <gl/GLU.h>
#elif __APPLE__
// OSX GL headers
#include <GL/glew.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
// Unix GL headers
#include <GL/gl.h>
#include <GL/glew.h>
#include <GL/glu.h>
#endif

#ifndef USE_SFML_RENDERWINDOW
#include <wx/glcanvas.h>
#undef None // Why does <X11/X.h> #define this? Idiotic
#endif

#define BLEND_NORMAL 0
#define BLEND_ADDITIVE 1
#define BLEND_IGNORE -1

namespace OpenGL
{
struct SystemInfo
{
	string vendor;
	string renderer;
	string version;
	string extensions;

	SystemInfo() { vendor = renderer = version = extensions = "OpenGL not initialised"; }
};

#ifndef USE_SFML_RENDERWINDOW
wxGLContext* getContext(wxGLCanvas* canvas);
#endif
bool     init();
bool     np2TexSupport();
bool     pointSpriteSupport();
bool     vboSupport();
bool     validTexDimension(unsigned dim);
float    maxPointSize();
unsigned maxTextureSize();
bool     isInitialised();
bool     accuracyTweak();
#ifndef USE_SFML_RENDERWINDOW
int* getWxGLAttribs();
#endif
void       setColour(const ColRGBA& col, bool set_blend = true);
void       setColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, int8_t blend = BLEND_IGNORE);
void       setBlend(int blend);
void       resetBlend();
SystemInfo systemInfo();
} // namespace OpenGL
